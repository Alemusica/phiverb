#include "combined/engine.h"
#include "combined/waveguide_base.h"

#include "core/cl/common.h"
#include "core/environment.h"
#include "core/geo/box.h"
#include "core/scene_data_loader.h"

#include "raytracer/simulation_parameters.h"

#include "utilities/aligned/vector.h"
#include "utilities/string_builder.h"

#include "waveguide/mesh.h"
#include "waveguide/mesh_descriptor.h"
#include "waveguide/simulation_parameters.h"

#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

namespace {

std::string usage() {
    return "Usage: boundary_probe <scene.obj> [node_index]";
}

auto load_scene(const std::string& path) {
    wayverb::core::scene_data_loader loader{path};
    const auto scene = loader.get_scene_data();
    if (!scene) {
        throw std::runtime_error{
                util::build_string("Failed to load scene: ", path)};
    }

    return wayverb::core::make_scene_data(
            scene->get_triangles(),
            scene->get_vertices(),
            util::aligned::vector<wayverb::core::surface<wayverb::core::simulation_bands>>(
                    scene->get_surfaces().size(),
                    wayverb::core::surface<wayverb::core::simulation_bands>{}));
}

std::string face_name(int face) {
    switch (face) {
        case 0: return "nx";
        case 1: return "px";
        case 2: return "ny";
        case 3: return "py";
        case 4: return "nz";
        case 5: return "pz";
        default: return "invalid";
    }
}

void dump_boundary_entry(const wayverb::waveguide::mesh& mesh,
                         size_t node_index) {
    using namespace wayverb::waveguide;
    const auto& nodes = mesh.get_structure().get_condensed_nodes();
    if (node_index >= nodes.size()) {
        throw std::runtime_error("Node index out of range");
    }

    const auto& layout = mesh.get_structure().get_boundary_layout();
    if (node_index >= layout.node_lookup.size() ||
        layout.node_lookup[node_index] == std::numeric_limits<uint32_t>::max()) {
        std::cout << "Node " << node_index << " is not a boundary node.\n";
        return;
    }

    const uint32_t layout_index = layout.node_lookup[node_index];
    const auto& header = layout.headers[layout_index];
    const auto locator =
            compute_locator(mesh.get_descriptor(), static_cast<uint32_t>(node_index));

    const uint32_t expected_guard =
            static_cast<uint32_t>(node_index) ^ 0xA5A5A5A5u;
    const bool guard_ok = header.guard == expected_guard;
    const uint8_t face_mask = static_cast<uint8_t>(header.dif & 0x3Fu);
    const uint16_t dif_index = static_cast<uint16_t>((header.dif >> 6) & 0x3FFu);

    std::cout << "--- boundary probe ---\n";
    std::cout << "node_index: " << node_index << "\n";
    std::cout << "locator: (" << locator.x << ", " << locator.y << ", "
              << locator.z << ")\n";
    std::cout << "boundary_type: " << nodes[node_index].boundary_type << "\n";
    std::cout << "boundary_index: " << nodes[node_index].boundary_index << "\n";
    std::cout << "layout_index: " << layout_index << "\n";
    std::cout << "guard: 0x" << std::hex << header.guard << std::dec
              << (guard_ok ? " (ok)\n" : " (mismatch!)\n");
    std::cout << "dif_mask: 0x" << std::hex << static_cast<int>(face_mask)
              << std::dec << " material_index: " << header.material_index
              << " dif_coeff_block: " << dif_index << "\n";
    std::cout << "sdf_distance: " << layout.sdf_distance[layout_index] << "\n";
    const auto normal = layout.sdf_normal[layout_index];
    std::cout << "sdf_normal: (" << normal.x << ", " << normal.y << ", "
              << normal.z << ")\n";

    const auto coeff_offset = layout.coeff_block_offsets[layout_index];
    const auto coeff_block_begin =
            layout.coeff_blocks.begin() + coeff_offset;
    const auto* memory_base =
            layout.filter_memories.data() + layout_index * 6;

    for (int face = 0; face < 6; ++face) {
        if ((face_mask & (1 << face)) == 0) {
            continue;
        }
        const auto& coeff = coeff_block_begin[face];
        const auto& mem = memory_base[face];
        std::cout << "face[" << face_name(face) << "]: "
                  << "a0=" << static_cast<double>(coeff.a[0])
                  << " b0=" << static_cast<double>(coeff.b[0])
                  << " memory0=" << static_cast<double>(mem.array[0]) << "\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << usage() << '\n';
            return EXIT_FAILURE;
        }

        const std::string scene_path = argv[1];
        const size_t node_index =
                argc >= 3 ? static_cast<size_t>(std::stoull(argv[2])) : size_t{0};

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

        const auto& mesh = engine.get_voxels_and_mesh().mesh;
        dump_boundary_entry(mesh, node_index);
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "boundary_probe error: " << e.what() << '\n';
    }
    return EXIT_FAILURE;
}
