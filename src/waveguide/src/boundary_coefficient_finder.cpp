#include "waveguide/boundary_coefficient_finder.h"
#include "waveguide/boundary_coefficient_program.h"
#include "waveguide/mesh.h"
#include "waveguide/setup.h"
#include "waveguide/precomputed_inputs.h"
#include "waveguide/mesh_descriptor.h"
#include "waveguide/cl/utils.h"

#include "glm/glm.hpp"
#include "glm/common.hpp"

#include <array>

namespace wayverb {
namespace waveguide {

namespace {

size_t sdf_index(const sdf_volume& vol, const glm::ivec3& idx) {
    return (static_cast<size_t>(idx.z) * vol.dims.y + idx.y) * vol.dims.x +
           idx.x;
}

int sample_label(const sdf_volume& vol, const glm::vec3& pos) {
    if (vol.voxel_pitch <= 0.0f || vol.total_voxels() == 0) {
        return -1;
    }
    const glm::vec3 coord = (pos - vol.origin) / vol.voxel_pitch;
    const glm::vec3 upper = glm::vec3(vol.dims) - glm::vec3(1.0f);
    const glm::vec3 clamped = glm::clamp(coord,
                                         glm::vec3(0.0f),
                                         upper);
    const auto base = glm::ivec3(glm::floor(clamped));
    const auto index = sdf_index(vol, base);
    return static_cast<int>(vol.label_at(index));
}

const std::string& label_name_for(const sdf_volume& vol, int label_id) {
    static const std::string empty{};
    if (label_id >= 0 &&
        label_id < static_cast<int>(vol.label_names.size())) {
        return vol.label_names[static_cast<size_t>(label_id)];
    }
    return empty;
}

glm::vec3 mask_to_vector(cl_int mask) {
    if (mask & id_nx) return {-1.f, 0.f, 0.f};
    if (mask & id_px) return {1.f, 0.f, 0.f};
    if (mask & id_ny) return {0.f, -1.f, 0.f};
    if (mask & id_py) return {0.f, 1.f, 0.f};
    if (mask & id_nz) return {0.f, 0.f, -1.f};
    if (mask & id_pz) return {0.f, 0.f, 1.f};
    return {0.f, 0.f, 0.f};
}

cl_uint resolve_coefficient(const precomputed_boundary_state& state,
                            const mesh_descriptor& descriptor,
                            const glm::ivec3& locator,
                            const glm::vec3& dir) {
    if (!state.volume) {
        auto it = state.label_to_coefficient.find("walls");
        return it != state.label_to_coefficient.end() ? it->second
                                                      : state.default_coefficient;
    }
    if (glm::length(dir) < 1e-6f) {
        return state.default_coefficient;
    }
    const glm::vec3 base = compute_position(descriptor, locator);
    const glm::vec3 offset = dir * (descriptor.spacing * 0.5f);
    const auto label = sample_label(*state.volume, base + offset);
    const auto& name = label_name_for(*state.volume, label);
    const auto it = state.label_to_coefficient.find(name);
    if (it != state.label_to_coefficient.end()) {
        return it->second;
    }
    return state.default_coefficient;
}

template <size_t N>
std::array<glm::vec3, N> boundary_vectors(cl_int bt) {
    std::array<glm::vec3, N> dirs{};
    size_t idx = 0;
    const auto push = [&](cl_int mask) {
        if ((bt & mask) && idx < N) {
            dirs[idx++] = mask_to_vector(mask);
        }
    };
    push(id_nx);
    push(id_px);
    push(id_ny);
    push(id_py);
    push(id_nz);
    push(id_pz);
    while (idx < N) {
        dirs[idx++] = glm::vec3(0.0f);
    }
    return dirs;
}

template <size_t N, typename DirectionsFn, typename Predicate>
util::aligned::vector<boundary_index_array<N>> build_boundary_arrays(
        const mesh_descriptor& descriptor,
        const util::aligned::vector<condensed_node>& nodes,
        const precomputed_boundary_state& state,
        DirectionsFn&& directions_fn,
        Predicate&& predicate) {
    util::aligned::vector<boundary_index_array<N>> result;
    result.reserve(count_boundary_type(nodes.begin(), nodes.end(), predicate));
    for (size_t idx = 0; idx < nodes.size(); ++idx) {
        const auto& node = nodes[idx];
        if (!predicate(node.boundary_type)) {
            continue;
        }
        const auto locator = compute_locator(descriptor, idx);
        const auto dirs = directions_fn(node.boundary_type);
        boundary_index_array<N> arr{};
        for (size_t i = 0; i != N; ++i) {
            arr.array[i] = resolve_coefficient(
                    state, descriptor, locator, dirs[i]);
        }
        result.emplace_back(arr);
    }
    return result;
}

template <typename it, typename func>
void set_boundary_index(it begin, it end, func f) {
    auto count = 0u;
    for (; begin != end; ++begin) {
        if (f(begin->boundary_type)) {
            begin->boundary_index = count++;
        }
    }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

template <typename T, typename It, typename Func>
cl::Buffer init_buffer(It begin,
                       It end,
                       const cl::Context& context,
                       Func func) {
    const auto num_indices = count_boundary_type(begin, end, func);
    if (!num_indices) {
        throw std::runtime_error("No boundaries.");
    }
    set_boundary_index(begin, end, func);
    return cl::Buffer{context, CL_MEM_READ_WRITE, sizeof(T) * num_indices};
}

//  mmmmmm beautiful
boundary_index_data compute_boundary_index_data(
        const cl::Device& device,
        const core::scene_buffers& buffers,
        const mesh_descriptor& descriptor,
        util::aligned::vector<condensed_node>& nodes,
        const precomputed_boundary_state* precomputed) {
    if (precomputed && precomputed->volume) {
        auto ret_1 = build_boundary_arrays<1>(
                descriptor, nodes, *precomputed, boundary_vectors<1>, is_boundary<1>);
        auto ret_2 = build_boundary_arrays<2>(
                descriptor, nodes, *precomputed, boundary_vectors<2>, is_boundary<2>);
        auto ret_3 = build_boundary_arrays<3>(
                descriptor, nodes, *precomputed, boundary_vectors<3>, is_boundary<3>);
        set_boundary_index(nodes.begin(), nodes.end(), is_boundary<1>);
        return {std::move(ret_1), std::move(ret_2), std::move(ret_3)};
    }

    //  load up buffers
    auto index_buffer_1 =
            init_buffer<boundary_index_array_1>(nodes.begin(),
                                                nodes.end(),
                                                buffers.get_context(),
                                                is_1d_boundary_or_reentrant);
    auto index_buffer_2 = init_buffer<boundary_index_array_2>(
            nodes.begin(), nodes.end(), buffers.get_context(), is_boundary<2>);
    auto index_buffer_3 = init_buffer<boundary_index_array_3>(
            nodes.begin(), nodes.end(), buffers.get_context(), is_boundary<3>);

    //  load the nodes vector to a cl buffer
    const auto nodes_buffer =
            core::load_to_buffer(buffers.get_context(), nodes, true);

    //  fire up the program
    const boundary_coefficient_program program{
            core::compute_context{buffers.get_context(), device}};

    //  create a queue to make sure the cl stuff gets ordered properly
    cl::CommandQueue queue{buffers.get_context(), device, CL_QUEUE_PROFILING_ENABLE};

    //  all our programs use the same size/queue, which can be set up here
    const auto enqueue = [&] {
        return cl::EnqueueArgs{queue, cl::NDRange{nodes.size()}};
    };

    //  run the kernels to compute boundary indices

    auto ret_1 = [&] {
        auto kernel = program.get_boundary_coefficient_finder_1d_kernel();
        kernel(enqueue(),
               nodes_buffer,
               descriptor,
               index_buffer_1,
               buffers.get_voxel_index_buffer(),
               buffers.get_global_aabb(),
               buffers.get_side(),
               buffers.get_triangles_buffer(),
               buffers.get_triangles_buffer().getInfo<CL_MEM_SIZE>() /
                       sizeof(core::triangle),
               buffers.get_vertices_buffer());
        const auto out = core::read_from_buffer<boundary_index_array_1>(
                queue, index_buffer_1);

        const auto num_surfaces_1d =
                count_boundary_type(nodes.begin(), nodes.end(), is_boundary<1>);

        util::aligned::vector<boundary_index_array_1> ret;
        ret.reserve(num_surfaces_1d);

        //  we need to remove reentrant nodes from these results
        //  i am dead inside and idk what <algorithm> this is
        for (const auto& i : nodes) {
            if (is_boundary<1>(i.boundary_type)) {
                ret.emplace_back(out[i.boundary_index]);
            }
        }
        return ret;
    }();

    auto ret_2 = [&] {
        auto kernel = program.get_boundary_coefficient_finder_2d_kernel();
        kernel(enqueue(),
               nodes_buffer,
               descriptor,
               index_buffer_2,
               index_buffer_1);
        return core::read_from_buffer<boundary_index_array_2>(queue,
                                                              index_buffer_2);
    }();

    auto ret_3 = [&] {
        auto kernel = program.get_boundary_coefficient_finder_3d_kernel();
        kernel(enqueue(),
               nodes_buffer,
               descriptor,
               index_buffer_3,
               index_buffer_1);
        return core::read_from_buffer<boundary_index_array_3>(queue,
                                                              index_buffer_3);
    }();

    //  finally, update node boundary indices so that the 1d indices point only
    //  to boundaries and not to reentrant nodes
    set_boundary_index(nodes.begin(), nodes.end(), is_boundary<1>);

    return {std::move(ret_1), std::move(ret_2), std::move(ret_3)};
}

}  // namespace waveguide
}  // namespace wayverb
