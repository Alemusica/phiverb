#include "waveguide/mesh.h"
#include "waveguide/boundary_adjust.h"
#include "waveguide/boundary_layout.h"
#include "waveguide/config.h"
#include "waveguide/fitted_boundary.h"
#include "waveguide/mesh_setup_program.h"
#include "waveguide/program.h"

#include "waveguide/cl/utils.h"

#include "core/conversions.h"
#include "core/scene_data_loader.h"
#include "core/spatial_division/scene_buffers.h"
#include "core/spatial_division/voxelised_scene_data.h"

#include "utilities/popcount.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>

namespace wayverb {
namespace waveguide {

mesh::mesh(mesh_descriptor descriptor, vectors vectors)
        : descriptor_(std::move(descriptor))
        , vectors_(std::move(vectors)) {}

const mesh_descriptor& mesh::get_descriptor() const { return descriptor_; }
const vectors& mesh::get_structure() const { return vectors_; }

bool is_inside(const mesh& m, size_t node_index) {
    return is_inside(m.get_structure().get_condensed_nodes()[node_index]);
}

void mesh::set_coefficients(coefficients_canonical coefficients) {
    vectors_.set_coefficients(coefficients);
}

void mesh::set_coefficients(
        util::aligned::vector<coefficients_canonical> coefficients) {
    vectors_.set_coefficients(std::move(coefficients));
}

double estimate_volume(const mesh& mesh) {
    const auto& nodes = mesh.get_structure().get_condensed_nodes();
    const auto num_inside =
            std::count_if(cbegin(nodes), cend(nodes), [](const auto& i) {
                return is_inside(i);
            });
    const auto spacing = mesh.get_descriptor().spacing;
    const auto node_volume = spacing * spacing * spacing;
    return node_volume * num_inside;
}

////////////////////////////////////////////////////////////////////////////////

mesh compute_mesh(
        const core::compute_context& cc,
        const core::voxelised_scene_data<cl_float3,
                                         core::surface<core::simulation_bands>>&
                voxelised,
        float mesh_spacing,
        float speed_of_sound) {
    const bool force_identity_coeffs =
            std::getenv("WAYVERB_FORCE_IDENTITY_COEFFS") != nullptr;
    const auto program = setup_program{cc};
    auto queue = cl::CommandQueue{cc.context, cc.device, CL_QUEUE_PROFILING_ENABLE};

    const auto buffers = make_scene_buffers(cc.context, voxelised);

    const auto desc = [&] {
        const auto aabb = voxelised.get_voxels().get_aabb();
        const auto dim = glm::ivec3{dimensions(aabb) / mesh_spacing};
        return mesh_descriptor{core::to_cl_float3{}(aabb.get_min()),
                               core::to_cl_int3{}(dim),
                               mesh_spacing};
    }();

    auto nodes = [&] {
        const auto num_nodes = compute_num_nodes(desc);

        cl::Buffer node_buffer{cc.context,
                               CL_MEM_READ_WRITE,
                               num_nodes * sizeof(condensed_node)};

        const auto enqueue = [&] {
            return cl::EnqueueArgs(queue, cl::NDRange(num_nodes));
        };

        //  find whether each node is inside or outside the model
        {
            auto kernel = program.get_node_inside_kernel();
            kernel(enqueue(),
                   node_buffer,
                   desc,
                   buffers.get_voxel_index_buffer(),
                   buffers.get_global_aabb(),
                   buffers.get_side(),
                   buffers.get_triangles_buffer(),
                   buffers.get_vertices_buffer());
        }

#ifndef NDEBUG
        {
            auto nodes =
                    core::read_from_buffer<condensed_node>(queue, node_buffer);
            const auto count =
                    count_boundary_type(nodes.begin(), nodes.end(), [](auto i) {
                        return i == id_inside;
                    });
            if (!count) {
                throw std::runtime_error{"No inside nodes found."};
            }
        }
#endif

        //  find node boundary type
        {
            auto kernel = program.get_node_boundary_kernel();
            kernel(enqueue(), node_buffer, desc);
        }

        return core::read_from_buffer<condensed_node>(queue, node_buffer);
    }();

    //  IMPORTANT
    //  compute_boundary_index_data mutates the nodes array, so it must
    //  be run before condensing the nodes.
    auto boundary_indices =
            compute_boundary_index_data(cc.device, buffers, desc, nodes);

    auto coefficients = util::map_to_vector(
            begin(voxelised.get_scene_data().get_surfaces()),
            end(voxelised.get_scene_data().get_surfaces()),
            [&](const auto& surface) {
                const auto make_identity_coeffs = [] {
                    coefficients_canonical identity{};
                    identity.b[0] = static_cast<filt_real>(1);
                    identity.a[0] = static_cast<filt_real>(1);
                    return identity;
                };

                if (force_identity_coeffs) {
                    return make_identity_coeffs();
                }

                auto coeffs = to_impedance_coefficients(
                        compute_reflectance_filter_coefficients(
                                surface.absorption.s,
                                1 / config::time_step(speed_of_sound,
                                                      mesh_spacing)));

                const auto sanitize = [](auto& coeff_array) {
                    for (auto& coeff : coeff_array) {
                        if (!std::isfinite(static_cast<double>(coeff))) {
                            coeff = static_cast<filt_real>(0);
                        }
                    }
                };

                sanitize(coeffs.b);
                sanitize(coeffs.a);

                const auto average_absorption = [&] {
                    double sum = 0.0;
                    for (const auto value : surface.absorption.s) {
                        sum += value;
                    }
                    return sum / 8.0;
                }();

                if (!is_stable(coeffs.a)) {
                    coeffs = make_identity_coeffs();
                }

                return coeffs;
            });

    auto boundary_layout = build_boundary_layout(
            desc, nodes, boundary_indices, coefficients, voxelised);

    auto v = vectors{std::move(nodes),
                     std::move(coefficients),
                     std::move(boundary_layout)};

    return {desc, std::move(v)};
}

voxels_and_mesh compute_voxels_and_mesh(const core::compute_context& cc,
                                        const core::gpu_scene_data& scene,
                                        const glm::vec3& anchor,
                                        double sample_rate,
                                        double speed_of_sound) {
    const auto mesh_spacing =
            config::grid_spacing(speed_of_sound, 1 / sample_rate);
    // Allow shrinking the voxel padding around the adjusted boundary via env.
    // Default is 5 (historical). Lower values reduce domain size and runtime
    // without materially changing results for interior receivers.
    int pad = 5;
    if (const char* p = std::getenv("WAYVERB_VOXEL_PAD")) {
        try {
            pad = std::max(0, std::min(16, static_cast<int>(std::strtol(p, nullptr, 10))));
        } catch (...) {
        }
    }
    auto voxelised = make_voxelised_scene_data(
            scene,
            pad,
            waveguide::compute_adjusted_boundary(
                    core::geo::compute_aabb(scene.get_vertices()),
                    anchor,
                    mesh_spacing));
    auto mesh = compute_mesh(cc, voxelised, mesh_spacing, speed_of_sound);
    return {std::move(voxelised), std::move(mesh)};
}

}  // namespace waveguide
}  // namespace wayverb
