#include "combined/engine.h"
#include "combined/waveguide_base.h"

#include "core/cl/common.h"
#include "core/environment.h"
#include "core/geo/box.h"
#include "core/scene_data_loader.h"

#include "glm/glm.hpp"

#include "raytracer/simulation_parameters.h"

#include "utilities/aligned/vector.h"
#include "utilities/progress_bar.h"
#include "utilities/string_builder.h"
#include "utilities/range.h"

#include "waveguide/mesh.h"
#include "waveguide/setup.h"
#include "waveguide/simulation_parameters.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <algorithm>

#ifndef WAYVERB_DEFAULT_SCENE
#define WAYVERB_DEFAULT_SCENE "assets/test_geometry/pyramid_twisted_minor.obj"
#endif

namespace {

auto load_scene(const std::string& path) {
    wayverb::core::scene_data_loader loader{path};
    const auto scene_data = loader.get_scene_data();
    if (!scene_data) {
        throw std::runtime_error{
                util::build_string("Failed to load scene: ", path)};
    }

    // Assign neutral surfaces so the simulation can proceed.
    const auto blank_surface =
            wayverb::core::surface<wayverb::core::simulation_bands>{
                    {{0.07, 0.09, 0.11, 0.12, 0.13, 0.14, 0.16, 0.17}},
                    {{0.07, 0.09, 0.11, 0.12, 0.13, 0.14, 0.16, 0.17}}};

    auto data = wayverb::core::make_scene_data(
            scene_data->get_triangles(),
            scene_data->get_vertices(),
            util::aligned::vector<
                    wayverb::core::surface<wayverb::core::simulation_bands>>(
                    scene_data->get_surfaces().size(), blank_surface));

    if (data.get_surfaces().empty()) {
        data.set_surfaces(blank_surface);
    }

    return data;
}

struct regression_result final {
    double total_seconds{};
};

void log_mesh_summary(const wayverb::combined::engine& engine) {
    const auto& voxels_mesh = engine.get_voxels_and_mesh();
    const auto& nodes =
            voxels_mesh.mesh.get_structure().get_condensed_nodes();
    const auto inside = std::count_if(
            nodes.begin(),
            nodes.end(),
            [](const auto& node) { return wayverb::waveguide::is_inside(node); });
    const auto spacing = voxels_mesh.mesh.get_descriptor().spacing;

    std::cout << "Mesh summary:\n"
              << "  total nodes : " << nodes.size() << '\n'
              << "  inside nodes: " << inside << '\n'
              << "  spacing     : " << spacing << " m\n";
}

void validate_scene(const wayverb::combined::engine& engine,
                    const glm::vec3& source,
                    const glm::vec3& receiver) {
    const auto distance = glm::length(source - receiver);
    if (distance < 1.0e-3f) {
        throw std::runtime_error{
                "Source and receiver coincide; adjust positions before running."};
    }

    const auto& nodes = engine.get_voxels_and_mesh().mesh.get_structure().get_condensed_nodes();
    const auto inside = std::count_if(
            nodes.begin(), nodes.end(), [](const auto& node) {
                return wayverb::waveguide::is_inside(node);
            });
    if (inside == 0) {
        throw std::runtime_error{
                "Mesh sanity check failed: no inside nodes detected. Verify geometry is watertight and correctly scaled."};
    }
}

regression_result run_regression(const std::string& scene_path) {
    const auto scene_data = load_scene(scene_path);
    const auto aabb = wayverb::core::geo::compute_aabb(scene_data.get_vertices());
    const auto centre = util::centre(aabb);

    const glm::vec3 source = centre + glm::vec3{0.0f, 0.0f, 0.2f};
    const glm::vec3 receiver = centre + glm::vec3{0.0f, 0.0f, -0.2f};

    const auto rays = 1 << 15;
    const auto image_sources = 2;
    const auto sample_rate = 44100.0;

    const auto waveguide_params =
            wayverb::waveguide::single_band_parameters{1000.0, 0.6};

    wayverb::combined::engine engine{
            wayverb::core::compute_context{},
            scene_data,
            source,
            receiver,
            wayverb::core::environment{},
            wayverb::raytracer::simulation_parameters{rays, image_sources},
            wayverb::combined::make_waveguide_ptr(waveguide_params)};

    validate_scene(engine, source, receiver);
    log_mesh_summary(engine);

    util::progress_bar progress{std::cout};
    engine.connect_engine_state_changed(
            [&](auto /*state*/, auto p) { set_progress(progress, p); });

    const auto start = std::chrono::steady_clock::now();
    const auto intermediate = engine.run(true);
    const auto end = std::chrono::steady_clock::now();

    const bool allow_empty = std::getenv("WAYVERB_ALLOW_EMPTY_INTERMEDIATE") != nullptr;
    if (!intermediate) {
        if (!allow_empty) {
            throw std::runtime_error{"Engine returned empty intermediate result."};
        }
        std::cout << "Empty intermediate tolerated (WAYVERB_ALLOW_EMPTY_INTERMEDIATE=1).\n";
    }

    const auto elapsed =
            std::chrono::duration<double>(end - start).count();
    return {elapsed};
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string scene_path =
                argc > 1 ? argv[1] : WAYVERB_DEFAULT_SCENE;

        if (!std::ifstream{scene_path}) {
            throw std::runtime_error{
                    util::build_string("Scene file not found: ", scene_path)};
        }

        std::cout << "Wayverb Apple Silicon regression starting...\n";
        std::cout << "scene: " << scene_path << '\n';

        const auto result = run_regression(scene_path);

        std::cout << "\nRegression completed successfully.\n";
        std::cout << "Engine runtime: " << result.total_seconds << " seconds\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Regression failed: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
