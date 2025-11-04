#include "combined/engine.h"
#include "combined/waveguide_base.h"

#include "core/cl/common.h"
#include "core/environment.h"
#include "core/geo/box.h"
#include "core/scene_data_loader.h"

#include "glm/glm.hpp"

#include "raytracer/simulation_parameters.h"

#include "utilities/aligned/vector.h"
#include "utilities/string_builder.h"

#include "waveguide/boundary_coefficient_finder.h"
#include "waveguide/cl/utils.h"
#include "waveguide/mesh.h"
#include "waveguide/mesh_descriptor.h"
#include "waveguide/simulation_parameters.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
#include <vector>

namespace {

constexpr float courant = 0.5773502691896257f;  // 1 / sqrt(3)
constexpr float courant_sq = 1.0f / 3.0f;

std::string usage() {
    return "Usage: boundary_probe <scene.obj> [node_index]";
}

auto load_scene(const std::string& path) {
    wayverb::core::scene_data_loader loader{path};
    const auto scene = loader.get_scene_data();
    if (!scene) {
        throw std::runtime_error{util::build_string(
                "Failed to load scene: ", path)};
    }

    return wayverb::core::make_scene_data(
            scene->get_triangles(),
            scene->get_vertices(),
            util::aligned::vector<wayverb::core::surface<wayverb::core::simulation_bands>>(
                    scene->get_surfaces().size(),
                    wayverb::core::surface<wayverb::core::simulation_bands>{}));
}

int port_to_neighbor_index(int port) {
    switch (port) {
        case 0: return 0;  // nx
        case 1: return 1;  // px
        case 2: return 2;  // ny
        case 3: return 3;  // py
        case 4: return 4;  // nz
        case 5: return 5;  // pz
    }
    return -1;
}

std::array<int, 2> get_inner_dirs_2(int boundary_type) {
    using namespace wayverb::waveguide;
    switch (boundary_type) {
        case id_nx | id_ny: return {0, 2};
        case id_nx | id_py: return {0, 3};
        case id_px | id_ny: return {1, 2};
        case id_px | id_py: return {1, 3};
        case id_nx | id_nz: return {0, 4};
        case id_nx | id_pz: return {0, 5};
        case id_px | id_nz: return {1, 4};
        case id_px | id_pz: return {1, 5};
        case id_ny | id_nz: return {2, 4};
        case id_ny | id_pz: return {2, 5};
        case id_py | id_nz: return {3, 4};
        case id_py | id_pz: return {3, 5};
        default: return {-1, -1};
    }
}

std::array<int, 2> get_surrounding_dirs(const std::array<int, 2>& inner_dirs) {
    const bool has_x = (inner_dirs[0] == 0 || inner_dirs[0] == 1 ||
                        inner_dirs[1] == 0 || inner_dirs[1] == 1);
    const bool has_y = (inner_dirs[0] == 2 || inner_dirs[0] == 3 ||
                        inner_dirs[1] == 2 || inner_dirs[1] == 3);
    if (has_x && has_y) {
        return {4, 5};
    }
    if (has_x) {
        return {2, 3};
    }
    return {0, 1};
}

float neighbor_value(const std::vector<float>& buffer,
                     const std::array<cl_uint, 6>& neighbors,
                     int port) {
    const int slot = port_to_neighbor_index(port);
    if (slot < 0) {
        return 0.0f;
    }
    const auto index = neighbors[slot];
    if (index == wayverb::waveguide::mesh_descriptor::no_neighbor) {
        return 0.0f;
    }
    if (index >= buffer.size()) {
        return 0.0f;
    }
    return buffer[index];
}

template <size_t order>
float filter_step_host(float input,
                       wayverb::waveguide::memory<order>& memory,
                       const wayverb::waveguide::coefficients<order>& coeffs) {
    const auto safe_mul = [](float coeff, float value) {
        return coeff == 0.0f ? 0.0f : coeff * value;
    };

    const float output = (input * coeffs.b[0] + memory.array[0]) / coeffs.a[0];
    for (int i = 0; i != static_cast<int>(order) - 1; ++i) {
        const float b = safe_mul(coeffs.b[i + 1], input);
        const float a = safe_mul(coeffs.a[i + 1], output);
        memory.array[i] = b - a + memory.array[i + 1];
    }
    const float b = safe_mul(coeffs.b[order], input);
    const float a = safe_mul(coeffs.a[order], output);
    memory.array[order - 1] = b - a;
    return output;
}

float filter_step_host(float input,
                       wayverb::waveguide::memory_canonical& memory,
                       const wayverb::waveguide::coefficients_canonical& coeffs) {
    constexpr auto order = static_cast<int>(
            wayverb::waveguide::memory_canonical::order);
    const auto safe_mul = [](float coeff, float value) {
        return coeff == 0.0f ? 0.0f : coeff * value;
    };

    const float a0 = static_cast<float>(coeffs.a[0]);
    const float b0 = static_cast<float>(coeffs.b[0]);
    const float output = (input * b0 + static_cast<float>(memory.array[0])) / a0;
    for (int i = 0; i != order - 1; ++i) {
        const float b = safe_mul(static_cast<float>(coeffs.b[i + 1]), input);
        const float a = safe_mul(static_cast<float>(coeffs.a[i + 1]), output);
        memory.array[i] = static_cast<wayverb::waveguide::filt_real>(
                b - a + static_cast<float>(memory.array[i + 1]));
    }
    const float b_last = safe_mul(
            static_cast<float>(coeffs.b[order]), input);
    const float a_last = safe_mul(
            static_cast<float>(coeffs.a[order]), output);
    memory.array[order - 1] =
            static_cast<wayverb::waveguide::filt_real>(b_last - a_last);
    return output;
}

float boundary2_host(const wayverb::waveguide::mesh& mesh,
                     size_t node_index,
                     const std::vector<float>& current,
                     const std::vector<float>& previous,
                     util::aligned::vector<wayverb::waveguide::boundary_data_array_2>& boundary_data,
                     std::vector<wayverb::waveguide::coefficients_canonical>& coefficients,
                     bool update_filters) {
    using namespace wayverb::waveguide;

    const auto& nodes = mesh.get_structure().get_condensed_nodes();
    const auto& descriptor = mesh.get_descriptor();

    if (node_index >= nodes.size()) {
        throw std::runtime_error("Node index out of range");
    }

    const auto node = nodes[node_index];
    const auto boundary_type = node.boundary_type;
    const auto boundary_index = node.boundary_index;
    const auto locator = compute_locator(descriptor, node_index);
    const auto neighbors = compute_neighbors(descriptor, node_index);

    const auto inner_dirs = get_inner_dirs_2(boundary_type & ~id_inside);
    const auto surrounding_dirs = get_surrounding_dirs(inner_dirs);

    float sum_inner = 0.0f;
    for (int dir : inner_dirs) {
        if (dir >= 0) {
            sum_inner += 2.0f * neighbor_value(current, neighbors, dir);
        }
    }

    float sum_surrounding = 0.0f;
    for (int dir : surrounding_dirs) {
        sum_surrounding += neighbor_value(current, neighbors, dir);
    }

    const float current_surrounding_weighting =
            courant_sq * (sum_inner + sum_surrounding);

    auto accumulate_filter_weighting = [&](const auto& arr) {
        float sum = 0.0f;
        for (const auto& bd : arr.array) {
            const auto coeff_index = bd.coefficient_index;
            if (coeff_index >= coefficients.size()) { continue; }
            const auto& coeff = coefficients[coeff_index];
            const float b0 = coeff.b[0];
            if (b0 != 0.0f) {
                sum += static_cast<float>(bd.filter_memory.array[0]) / b0;
            }
        }
        return sum;
    };

    float filter_weighting = 0.0f;
    if (boundary_index < boundary_data.size()) {
        filter_weighting = accumulate_filter_weighting(boundary_data[boundary_index]);
    }
    filter_weighting *= courant_sq;

    float coeff_weighting = 0.0f;
    if (boundary_index < boundary_data.size()) {
        for (const auto& bd : boundary_data[boundary_index].array) {
            const auto coeff_index = bd.coefficient_index;
            if (coeff_index >= coefficients.size()) { continue; }
            const auto& coeff = coefficients[coeff_index];
            const float b0 = coeff.b[0];
            if (b0 != 0.0f) {
                coeff_weighting += coeff.a[0] / b0;
            }
        }
    }

    const float prev_pressure = previous[node_index];
    const float current_pressure = current[node_index];
    const float prev_weighting = (coeff_weighting - 1.0f) * prev_pressure;
    const float numerator = current_surrounding_weighting + filter_weighting + prev_weighting;
    const float denominator = 1.0f + coeff_weighting;
    const float next_pressure = numerator / denominator;

    if (update_filters && boundary_index < boundary_data.size()) {
        auto& bd_array = boundary_data[boundary_index];
        for (auto& bd : bd_array.array) {
            const auto coeff_index = bd.coefficient_index;
            if (coeff_index >= coefficients.size()) { continue; }
            auto& coeff = coefficients[coeff_index];
            const float b0 = coeff.b[0];
            const float a0 = coeff.a[0];
            const float filt_state = static_cast<float>(bd.filter_memory.array[0]);
            const float diff = (a0 * (prev_pressure - next_pressure)) / (b0 * courant) +
                               (filt_state / b0);
            const float filter_input = -diff;
            filter_step_host(filter_input, bd.filter_memory, coeff);
        }
    }

    std::cout << "--- Host boundary_2 diagnostics ---\n";
    std::cout << "node_index: " << node_index << "\n";
    std::cout << "boundary_type: " << boundary_type << "\n";
    std::cout << "boundary_index: " << boundary_index << "\n";
    std::cout << "locator: (" << locator.x << ", " << locator.y << ", " << locator.z << ")\n";
    std::cout << "current_surrounding_weighting: " << current_surrounding_weighting << "\n";
    std::cout << "filter_weighting: " << filter_weighting << "\n";
    std::cout << "coeff_weighting: " << coeff_weighting << "\n";
    std::cout << "prev_weighting: " << prev_weighting << "\n";
    std::cout << "numerator: " << numerator << "\n";
    std::cout << "denominator: " << denominator << "\n";
    std::cout << "next_pressure: " << next_pressure << "\n";

    return next_pressure;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << usage() << '\n';
            return EXIT_FAILURE;
        }

        const std::string scene_path = argv[1];
        const size_t node_index = argc >= 3 ? static_cast<size_t>(std::stoull(argv[2]))
                                            : size_t{234840};
        const bool force_identity = (std::getenv("WAYVERB_FORCE_IDENTITY_COEFFS") != nullptr);

        constexpr double sample_rate = 44100.0;
        constexpr double speed_of_sound = 343.0;

        wayverb::core::compute_context cc;

        auto scene_data = load_scene(scene_path);
        const auto aabb = wayverb::core::geo::compute_aabb(scene_data.get_vertices());
        const auto centre = util::centre(aabb);

        const glm::vec3 source = centre + glm::vec3{0.0f, 0.0f, 0.2f};
        const glm::vec3 receiver = centre + glm::vec3{0.0f, 0.0f, -0.2f};

        wayverb::combined::engine engine{
                cc,
                scene_data,
                source,
                receiver,
                wayverb::core::environment{},
                wayverb::raytracer::simulation_parameters{1 << 15, 2},
                wayverb::combined::make_waveguide_ptr(
                        wayverb::waveguide::single_band_parameters{1000.0, 0.6})};

        const auto& voxels_and_mesh = engine.get_voxels_and_mesh();
        const auto& mesh = voxels_and_mesh.mesh;
        const auto num_nodes = mesh.get_structure().get_condensed_nodes().size();

        if (node_index >= num_nodes) {
            std::cerr << "Node index " << node_index
                      << " out of range (" << num_nodes << ")\n";
            return EXIT_FAILURE;
        }

        std::vector<float> current(num_nodes, 0.0f);
        std::vector<float> previous(num_nodes, 0.0f);

        auto boundary_data_2 = wayverb::waveguide::get_boundary_data<2>(mesh.get_structure());

        auto coefficients = std::vector<wayverb::waveguide::coefficients_canonical>{
                mesh.get_structure().get_coefficients().begin(),
                mesh.get_structure().get_coefficients().end()};

        if (std::getenv("WAYVERB_LIST_DUP")) {
            const auto& nodes = mesh.get_structure().get_condensed_nodes();
            const auto boundary_type = nodes[node_index].boundary_type;
            const auto boundary_index = nodes[node_index].boundary_index;
            std::cout << "Nodes sharing boundary_index " << boundary_index
                      << " and boundary_type " << boundary_type << ":\n";
            for (size_t idx = 0; idx < nodes.size(); ++idx) {
                if (nodes[idx].boundary_type == boundary_type &&
                    nodes[idx].boundary_index == boundary_index) {
                    std::cout << "  " << idx << '\n';
                }
            }
        }

        if (force_identity) {
            for (auto& coeff : coefficients) {
                std::fill(std::begin(coeff.b), std::end(coeff.b), 0.0);
                std::fill(std::begin(coeff.a), std::end(coeff.a), 0.0);
                coeff.b[0] = 1.0;
                coeff.a[0] = 1.0;
            }
        }

        const float next_pressure = boundary2_host(mesh,
                                                   node_index,
                                                   current,
                                                   previous,
                                                   boundary_data_2,
                                                   coefficients,
                                                   true);

        std::cout << "Computed next pressure: " << next_pressure << "\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "boundary_probe error: " << e.what() << '\n';
    }
    return EXIT_FAILURE;
}
