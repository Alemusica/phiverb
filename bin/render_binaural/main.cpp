#include "combined/engine.h"
#include "combined/waveguide_base.h"

#include "core/cl/common.h"
#include "core/environment.h"
#include "core/geo/box.h"
#include "core/scene_data_loader.h"
#include "core/attenuator/hrtf.h"
#include "raytracer/simulation_parameters.h"
#include "waveguide/simulation_parameters.h"

#include "audio_file/audio_file.h"

#include "glm/glm.hpp"

#include <iostream>
#include <fstream>
#include <optional>

using namespace wayverb;

namespace {

std::string usage() {
    return "Usage: render_binaural <scene.obj> [out_prefix]\n"
           "Env (optional): RT_RAYS, RT_IMG, WG_CUTOFF, WG_USABLE, IR_SECONDS";
}

template <typename T>
T env_or(const char* name, T fallback) {
    if (const char* s = std::getenv(name)) {
        try {
            if constexpr (std::is_integral_v<T>) return static_cast<T>(std::stoll(s));
            else return static_cast<T>(std::stod(s));
        } catch (...) {}
    }
    return fallback;
}

auto load_scene(const std::string& path) {
    core::scene_data_loader loader{path};
    const auto scene_data = loader.get_scene_data();
    if (!scene_data) {
        throw std::runtime_error{"Failed to load scene"};
    }
    // Assign neutral surfaces if empty
    if (scene_data->get_surfaces().empty()) {
        auto data = core::make_scene_data(
                scene_data->get_triangles(),
                scene_data->get_vertices(),
                util::aligned::vector<std::string>(scene_data->get_surfaces().size(), std::string{}));
        return data;
    }
    return *scene_data;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << usage() << "\n";
            return EXIT_FAILURE;
        }

        const std::string scene_path = argv[1];
        const std::string out_prefix = argc >= 3 ? argv[2] : std::string{"ir_binaural"};

        core::compute_context cc;
        auto scene_data = load_scene(scene_path);

        const auto aabb = core::geo::compute_aabb(scene_data.get_vertices());
        const auto centre = util::centre(aabb);
        const glm::vec3 source = centre + glm::vec3{0.0f, 0.0f, 0.5f};
        const glm::vec3 receiver = centre + glm::vec3{0.0f, 0.0f, -0.5f};

        const auto rays = static_cast<unsigned>(env_or<long long>("RT_RAYS", 1ll << 16)); // default ~65k
        const auto image_sources = static_cast<unsigned>(env_or<long long>("RT_IMG", 4));
        const auto sample_rate = env_or<double>("IR_SR", 48000.0);
        const core::environment env{};

        combined::engine engine{
            cc,
            core::scene_with_extracted_surfaces(scene_data, util::aligned::unordered_map<std::string, core::surface<core::simulation_bands>>{}),
            source,
            receiver,
            env,
            raytracer::simulation_parameters{rays, image_sources},
            combined::make_waveguide_ptr(waveguide::single_band_parameters{env_or<double>("WG_CUTOFF", 1000.0), env_or<double>("WG_USABLE", 0.6)})};

        std::atomic_bool keep{true};
        auto inter = engine.run(keep);
        if (!inter) {
            std::cerr << "Render returned empty intermediate." << std::endl;
            return EXIT_FAILURE;
        }

        // Binaural postprocess (left/right)
        core::attenuator::hrtf left{core::orientation{}, core::attenuator::hrtf::channel::left};
        core::attenuator::hrtf right{core::orientation{}, core::attenuator::hrtf::channel::right};

        auto l = inter->postprocess(left, sample_rate);
        auto r = inter->postprocess(right, sample_rate);

        if (l.empty() && r.empty()) {
            std::cerr << "Postprocess produced empty IRs." << std::endl;
            return EXIT_FAILURE;
        }

        // Build stereo channels (optionally pad to IR_SECONDS)
        const double min_seconds = env_or<double>("IR_SECONDS", 0.0);
        size_t n = std::max(l.size(), r.size());
        if (min_seconds > 0.0) {
            const size_t min_samples = static_cast<size_t>(min_seconds * sample_rate);
            n = std::max(n, min_samples);
        }
        util::aligned::vector<float> lch = l; lch.resize(n, 0.0f);
        util::aligned::vector<float> rch = r; rch.resize(n, 0.0f);

        const std::string out = out_prefix + ".wav";
        // Write as two channels
        std::array<util::aligned::vector<float>, 2> channels{std::move(lch), std::move(rch)};
        audio_file::write(out.c_str(), channels.begin(), channels.end(), static_cast<int>(sample_rate), audio_file::format::wav, audio_file::bit_depth::pcm24);
        std::cout << "Wrote binaural IR: " << out << "\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "render_binaural error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
