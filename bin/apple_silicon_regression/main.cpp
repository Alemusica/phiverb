#include "combined/engine.h"
#include "combined/waveguide_base.h"

#include "core/attenuator/null.h"
#include "core/cl/common.h"
#include "core/dsp_vector_ops.h"
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
#include "waveguide/precomputed_inputs.h"
#include "waveguide/simulation_parameters.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>

#ifndef WAYVERB_DEFAULT_SCENE
#define WAYVERB_DEFAULT_SCENE "assets/test_geometry/pyramid_twisted_minor.obj"
#endif

namespace {

struct regression_options final {
    std::string scene_path = WAYVERB_DEFAULT_SCENE;
    std::optional<glm::vec3> source;
    std::optional<glm::vec3> receiver;
    size_t rays = 1 << 15;
    int image_sources = 2;
    double sample_rate = 44100.0;
    double waveguide_cutoff = 1000.0;
    double waveguide_usable = 0.6;
};

glm::vec3 parse_vec3(std::string text) {
    std::replace(text.begin(), text.end(), ';', ',');
    std::replace(text.begin(), text.end(), ' ', ',');
    std::stringstream ss{text};
    std::array<double, 3> values{};
    size_t idx = 0;
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (token.empty()) {
            continue;
        }
        if (idx >= values.size()) {
            throw std::runtime_error{
                    util::build_string("Too many components in vec3: ", text)};
        }
        values[idx++] = std::stod(token);
    }
    if (idx != values.size()) {
        throw std::runtime_error{
                util::build_string("Expected 3 components for vec3, got ",
                                   idx,
                                   " from '",
                                   text,
                                   '\'')};
    }
    return glm::vec3{values[0], values[1], values[2]};
}

void print_usage(const char* exe) {
    std::cout << "Usage: " << exe << " [options]\n\n"
              << "Options:\n"
              << "  --scene <path>          Path to .obj/.way scene (default "
                 WAYVERB_DEFAULT_SCENE ")\n"
              << "  --source x,y,z          Source position in metres\n"
              << "  --receiver x,y,z        Receiver position in metres\n"
              << "  --rays <int>            Raytracer rays (default 32768)\n"
              << "  --img-src <int>         Image source order (default 2)\n"
              << "  --wg-cutoff <Hz>        Waveguide cutoff (default 1000)\n"
              << "  --wg-usable <0-1>       Waveguide usable portion (default "
                 "0.6)\n"
              << "  --sample-rate <Hz>      Output sample rate (default 44100)\n"
              << "  -h, --help              Show this message\n";
}

regression_options parse_args(int argc, char** argv) {
    regression_options opts;
    auto require_value = [&](int& index) -> const char* {
        if (index + 1 >= argc) {
            throw std::runtime_error{
                    util::build_string("Missing value for option ",
                                       argv[index])};
        }
        return argv[++index];
    };

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--scene") {
            opts.scene_path = require_value(i);
        } else if (arg == "--source") {
            opts.source = parse_vec3(require_value(i));
        } else if (arg == "--receiver") {
            opts.receiver = parse_vec3(require_value(i));
        } else if (arg == "--rays") {
            opts.rays = static_cast<size_t>(std::stoul(require_value(i)));
        } else if (arg == "--img-src") {
            opts.image_sources = std::stoi(require_value(i));
        } else if (arg == "--wg-cutoff") {
            opts.waveguide_cutoff = std::stod(require_value(i));
        } else if (arg == "--wg-usable") {
            opts.waveguide_usable = std::stod(require_value(i));
        } else if (arg == "--sample-rate") {
            opts.sample_rate = std::stod(require_value(i));
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            std::exit(EXIT_SUCCESS);
        } else {
            throw std::runtime_error{
                    util::build_string("Unknown option: ", arg)};
        }
    }

    return opts;
}

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

regression_result run_regression(const regression_options& opts) {
    const auto scene_data = load_scene(opts.scene_path);
    const auto aabb = wayverb::core::geo::compute_aabb(scene_data.get_vertices());
    const auto centre = util::centre(aabb);

    const glm::vec3 default_source = centre + glm::vec3{0.0f, 0.0f, 0.2f};
    const glm::vec3 default_receiver = centre + glm::vec3{0.0f, 0.0f, -0.2f};
    const glm::vec3 source = opts.source.value_or(default_source);
    const glm::vec3 receiver = opts.receiver.value_or(default_receiver);

    const auto waveguide_params =
            wayverb::waveguide::single_band_parameters{opts.waveguide_cutoff,
                                                       opts.waveguide_usable};

    auto precomputed_inputs = wayverb::waveguide::load_precomputed_inputs(
            opts.scene_path);
    if (precomputed_inputs) {
        wayverb::core::scene_data_loader loader{opts.scene_path};
        if (const auto strings_scene = loader.get_scene_data()) {
            precomputed_inputs->surface_names =
                    strings_scene->get_surfaces();
        }
    }

    wayverb::combined::engine engine{
            wayverb::core::compute_context{},
            scene_data,
            source,
            receiver,
            wayverb::core::environment{},
            wayverb::raytracer::simulation_parameters{
                    opts.rays,
                    static_cast<size_t>(opts.image_sources)},
            wayverb::combined::make_waveguide_ptr(waveguide_params),
            std::move(precomputed_inputs)};

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

    if (intermediate) {
        wayverb::core::attenuator::null attenuator{};
        const auto channel = intermediate->postprocess(attenuator,
                                                       opts.sample_rate);
        const auto non_finite = wayverb::core::count_non_finite(channel);
        if (non_finite != 0) {
            throw std::runtime_error{util::build_string(
                    "Postprocess output contains ",
                    non_finite,
                    " non-finite samples")};
        }
        const auto max_mag = wayverb::core::max_mag(channel);
        if (max_mag == 0.0f) {
            throw std::runtime_error{
                    "Postprocess output is silent (max magnitude == 0)."};
        }
        std::cout << "Channel max magnitude: " << max_mag << '\n';
    }

    const auto elapsed = std::chrono::duration<double>(end - start).count();
    return {elapsed};
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_args(argc, argv);

        if (!std::ifstream{options.scene_path}) {
            throw std::runtime_error{
                    util::build_string("Scene file not found: ",
                                       options.scene_path)};
        }

        std::cout << "Wayverb Apple Silicon regression starting...\n";
        std::cout << "scene: " << options.scene_path << '\n';
        if (options.source) {
            std::cout << "source: (" << options.source->x << ", "
                      << options.source->y << ", " << options.source->z << ")\n";
        }
        if (options.receiver) {
            std::cout << "receiver: (" << options.receiver->x << ", "
                      << options.receiver->y << ", " << options.receiver->z
                      << ")\n";
        }
        std::cout << "rays=" << options.rays
                  << " img_src=" << options.image_sources
                  << " wg_cutoff=" << options.waveguide_cutoff << " Hz"
                  << " wg_usable=" << options.waveguide_usable
                  << " sample_rate=" << options.sample_rate << " Hz\n";

        const auto result = run_regression(options);

        std::cout << "\nRegression completed successfully.\n";
        std::cout << "Engine runtime: " << result.total_seconds << " seconds\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Regression failed: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
