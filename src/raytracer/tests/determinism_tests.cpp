#include "raytracer/canonical.h"
#include "raytracer/stochastic/postprocessing.h"

#include "core/geo/box.h"
#include "core/scene_data.h"
#include "core/spatial_division/voxelised_scene_data.h"
#include "glm/glm.hpp"

#include "gtest/gtest.h"

#include <atomic>
#include <cstring>

using namespace wayverb;
using namespace wayverb::core;

namespace {

auto make_voxelised_box() {
    const geo::box box{glm::vec3{0}, glm::vec3{4, 3, 6}};
    const auto surface =
            make_surface<simulation_bands>(0.2, 0.1);  // light absorption/scatt
    const auto scene = geo::get_scene_data(box, surface);
    return make_voxelised_scene_data(scene, 5, 0.1f);
}

auto make_params(std::uint64_t seed) {
    raytracer::simulation_parameters params{};
    params.rays = 1 << 12;
    params.maximum_image_source_order = 2;
    params.receiver_radius = 0.1;
    params.histogram_sample_rate = 1000.0;
    params.rng_seed = seed;
    return params;
}

auto run_canonical(const raytracer::simulation_parameters& params) {
    const compute_context cc{};
    static const auto voxelised = make_voxelised_box();
    const glm::vec3 source{1.0f, 1.5f, 1.0f};
    const glm::vec3 receiver{2.0f, 1.2f, 2.0f};
    const environment env{};
    std::atomic_bool keep_going{true};

    auto result = raytracer::canonical(cc,
                                       voxelised,
                                       source,
                                       receiver,
                                       env,
                                       params,
                                       0,
                                       keep_going,
                                       [](auto, auto) {});
    EXPECT_TRUE(result);
    return result.value();
}

bool hist_equal(const raytracer::stochastic::energy_histogram& a,
                const raytracer::stochastic::energy_histogram& b) {
    if (a.sample_rate != b.sample_rate ||
        a.histogram.size() != b.histogram.size()) {
        return false;
    }
    for (size_t i = 0; i < a.histogram.size(); ++i) {
        if (a.histogram[i] != b.histogram[i]) {
            return false;
        }
    }
    return true;
}

bool impulses_equal(const util::aligned::vector<raytracer::impulse<8>>& a,
                    const util::aligned::vector<raytracer::impulse<8>>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].distance != b[i].distance ||
            a[i].position.s[0] != b[i].position.s[0] ||
            a[i].position.s[1] != b[i].position.s[1] ||
            a[i].position.s[2] != b[i].position.s[2] ||
            std::memcmp(&a[i].volume,
                        &b[i].volume,
                        sizeof(a[i].volume)) != 0) {
            return false;
        }
    }
    return true;
}

template <size_t Az, size_t El>
auto flatten_histogram(
        const raytracer::stochastic::directional_energy_histogram<Az, El>& h) {
    return raytracer::stochastic::sum_directional_histogram(h);
}

auto flatten_histogram(
        const raytracer::stochastic::energy_histogram& h) {
    return h;
}

}  // namespace

TEST(raytracer_determinism, identical_seed_matches) {
    const auto params = make_params(1337);
    const auto result_a = run_canonical(params);
    const auto result_b = run_canonical(params);

    ASSERT_TRUE(impulses_equal(result_a.aural.image_source,
                               result_b.aural.image_source));
    const auto hist_a = flatten_histogram(result_a.aural.stochastic);
    const auto hist_b = flatten_histogram(result_b.aural.stochastic);
    ASSERT_TRUE(hist_equal(hist_a, hist_b));
}

TEST(raytracer_determinism, different_seed_changes_output) {
    const auto result_a = run_canonical(make_params(1234));
    const auto result_b = run_canonical(make_params(5678));
    const auto hist_a = flatten_histogram(result_a.aural.stochastic);
    const auto hist_b = flatten_histogram(result_b.aural.stochastic);
    EXPECT_FALSE(hist_equal(hist_a, hist_b));
}
