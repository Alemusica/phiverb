#include "raytracer/postprocess.h"
#include "raytracer/image_source/postprocess.h"
#include "raytracer/stochastic/postprocess.h"

#include "core/attenuator/null.h"
#include "core/geo/box.h"
#include "core/scene_data.h"
#include "core/spatial_division/voxelised_scene_data.h"

#include "gtest/gtest.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <vector>

using namespace wayverb;
using namespace wayverb::core;

namespace {

struct shoebox_scene final {
    glm::vec3 min{0.0f};
    glm::vec3 max;
    double absorption;
    double scattering{0.0};
};

auto build_scene(const shoebox_scene& box) {
    const geo::box gbox{box.min, box.max};
    const auto surface = make_surface<simulation_bands>(
            static_cast<float>(box.absorption),
            static_cast<float>(box.scattering));
    const auto scene = geo::get_scene_data(gbox, surface);
    return make_voxelised_scene_data(scene, 5, 0.1f);
}

double box_volume(const shoebox_scene& box) {
    return static_cast<double>((box.max.x - box.min.x) *
                               (box.max.y - box.min.y) *
                               (box.max.z - box.min.z));
}

double band_average(const core::bands_type& v) {
    double sum = 0.0;
    for (size_t i = 0; i < simulation_bands; ++i) {
        sum += std::fabs(v.s[i]);
    }
    return sum / simulation_bands;
}

template <typename Histogram>
struct simulation_fixture final {
    raytracer::simulation_results<Histogram> aural;
    double room_volume;
    core::environment env;
};

auto run_simulation(const shoebox_scene& box,
                    const glm::vec3& source,
                    const glm::vec3& receiver,
                    std::uint64_t seed,
                    std::optional<size_t> max_image_order = std::nullopt) {
    const compute_context cc{};
    const auto voxelised = build_scene(box);
    const environment env{};
    std::atomic_bool keep_going{true};

    raytracer::simulation_parameters params{};
    params.rays = 1 << 14;
    params.maximum_image_source_order = max_image_order.value_or(4);
    params.receiver_radius = 0.1;
    params.histogram_sample_rate = 2000.0;
    params.rng_seed = seed;

    auto canonical = raytracer::canonical(cc,
                                          voxelised,
                                          source,
                                          receiver,
                                          env,
                                          params,
                                          0,
                                          keep_going,
                                          [](auto, auto) {});
    EXPECT_TRUE(canonical);
    using histogram_t =
            std::decay_t<decltype(canonical->aural.stochastic)>;
    return simulation_fixture<histogram_t>{std::move(canonical->aural),
                                           box_volume(box),
                                           env};
}

double sabine_rt(double volume, double surface_area, double absorption) {
    const double area = absorption * surface_area;
    return 0.161 * volume / area;
}

double eyring_rt(double volume, double surface_area, double absorption) {
    return 0.161 * volume / (-surface_area * std::log(1.0 - absorption));
}

std::vector<float> render_ir(const shoebox_scene& box,
                             const glm::vec3& source,
                             const glm::vec3& receiver,
                             double sample_rate,
                             std::uint64_t seed,
                             std::optional<size_t> max_image_order =
                                     std::nullopt) {
    auto sim = run_simulation(box, source, receiver, seed, max_image_order);
    const auto ir = raytracer::postprocess(sim.aural,
                                           core::attenuator::null{},
                                           receiver,
                                           sim.room_volume,
                                           sim.env,
                                           sample_rate);
    return {begin(ir), end(ir)};
}

std::vector<double> compute_edc_db(const std::vector<float>& ir) {
    std::vector<double> edc(ir.size());
    double accum = 0.0;
    for (size_t i = ir.size(); i-- > 0;) {
        accum += static_cast<double>(ir[i]) * ir[i];
        edc[i] = accum;
    }
    const double ref = edc.front();
    std::vector<double> edc_db(ir.size());
    for (size_t i = 0; i < edc.size(); ++i) {
        edc_db[i] = 10.0 * std::log10(std::max(edc[i], 1e-30) / ref);
    }
    return edc_db;
}

std::optional<double> find_time(const std::vector<double>& edc_db,
                                double sample_rate,
                                double target_db) {
    for (size_t i = 1; i < edc_db.size(); ++i) {
        if (edc_db[i - 1] >= target_db && edc_db[i] <= target_db) {
            const double t0 = (i - 1) / sample_rate;
            const double t1 = i / sample_rate;
            const double v0 = edc_db[i - 1];
            const double v1 = edc_db[i];
            const double alpha = (target_db - v0) / (v1 - v0);
            return t0 + alpha * (t1 - t0);
        }
    }
    return std::nullopt;
}

double compute_rt(const std::vector<double>& edc_db,
                  double sample_rate,
                  double start_db,
                  double end_db,
                  double scale) {
    const auto t0 = find_time(edc_db, sample_rate, start_db);
    const auto t1 = find_time(edc_db, sample_rate, end_db);
    if (!t0 || !t1) {
        return 0.0;
    }
    return (*t1 - *t0) * scale;
}

double surface_area(const shoebox_scene& box) {
    const double lx = box.max.x - box.min.x;
    const double ly = box.max.y - box.min.y;
    const double lz = box.max.z - box.min.z;
    return 2.0 * (lx * ly + lx * lz + ly * lz);
}

}  // namespace

TEST(raytracer_reverb, shoebox_tail_within_bounds) {
    const shoebox_scene box{{0.0f, 0.0f, 0.0f},
                            {6.0f, 5.0f, 3.0f},
                            0.2,
                            0.25};
    const glm::vec3 source{1.0f, 1.5f, 1.0f};
    const glm::vec3 receiver{2.5f, 2.0f, 1.2f};
    const double sample_rate = 48000.0;
    const auto ir =
            render_ir(box, source, receiver, sample_rate, 9001, 70);

    const auto edc_db = compute_edc_db(ir);
    ASSERT_LT(edc_db.back(), -60.0);

    const double t30 =
            compute_rt(edc_db, sample_rate, -5.0, -35.0, 2.0 /*T30*/);
    const double volume =
            static_cast<double>((box.max.x - box.min.x) *
                                (box.max.y - box.min.y) *
                                (box.max.z - box.min.z));
    const double surface = surface_area(box);
    const double sabine = sabine_rt(volume, surface, box.absorption) * 0.5;
    const double eyring = eyring_rt(volume, surface, box.absorption) * 0.5;
    const double lower = 0.85 * std::min(sabine, eyring);
    const double upper = 1.15 * std::max(sabine, eyring);
    EXPECT_GT(t30, lower);
    EXPECT_LT(t30, upper);
}

TEST(raytracer_reverb, shoebox_ism_rt_parity) {
    const shoebox_scene box{{0.0f, 0.0f, 0.0f},
                            {6.0f, 4.0f, 3.0f},
                            0.05};
    const glm::vec3 source{1.0f, 1.0f, 1.0f};
    const glm::vec3 receiver{3.0f, 1.5f, 1.2f};
    const double sample_rate = 48000.0;

    auto sim = run_simulation(box, source, receiver, 1337);
    const auto combined_ir = raytracer::postprocess(sim.aural,
                                                    core::attenuator::null{},
                                                    receiver,
                                                    sim.room_volume,
                                                    sim.env,
                                                    sample_rate);
    const auto ism_ir = raytracer::image_source::postprocess(
            begin(sim.aural.image_source),
            end(sim.aural.image_source),
            core::attenuator::null{},
            receiver,
            sim.env.speed_of_sound,
            sample_rate);

    const size_t comparison_window =
            std::min<size_t>({combined_ir.size(), ism_ir.size(), 4096ul});
    ASSERT_GT(comparison_window, 0u);

    auto argmax_index = [&](const auto& buffer) {
        size_t idx = 0;
        double max_val = 0.0;
        for (size_t i = 0; i < comparison_window; ++i) {
            const double val = std::fabs(buffer[i]);
            if (val > max_val) {
                max_val = val;
                idx = i;
            }
        }
        return idx;
    };

    const size_t peak_combined = argmax_index(combined_ir);
    const size_t peak_ism = argmax_index(ism_ir);
    EXPECT_LE(std::abs(static_cast<long>(peak_combined) -
                       static_cast<long>(peak_ism)),
              1);

    double max_db_error = 0.0;
    for (size_t i = 0; i < comparison_window; ++i) {
        const double ism = ism_ir[i];
        const double combined = combined_ir[i];
        if (std::fabs(ism) < 1e-8 && std::fabs(combined) < 1e-8) {
            continue;
        }
        const double ratio =
                std::max(std::fabs(combined), 1e-9) /
                std::max(std::fabs(ism), 1e-9);
        const double diff_db = 20.0 * std::log10(ratio);
        max_db_error = std::max(max_db_error, std::fabs(diff_db));
    }
    EXPECT_LT(max_db_error, 0.5);
}

TEST(raytracer_reverb, shoebox_scattering_zero_has_no_stochastic_energy) {
    const shoebox_scene box{{0.0f, 0.0f, 0.0f},
                            {6.0f, 4.0f, 3.0f},
                            0.1};
    const glm::vec3 source{1.0f, 1.0f, 1.0f};
    const glm::vec3 receiver{3.0f, 1.5f, 1.2f};
    const double sample_rate = 48000.0;

    const auto voxelised = build_scene(box);
    for (const auto& surface : voxelised.get_scene_data().get_surfaces()) {
        for (size_t band = 0; band < simulation_bands; ++band) {
            ASSERT_EQ(surface.scattering.s[band], 0.0f);
        }
    }

    auto sim = run_simulation(box, source, receiver, 2468);
    const auto stochastic_ir = raytracer::stochastic::postprocess(
            sim.aural.stochastic,
            core::attenuator::null{},
            sim.room_volume,
            sim.env,
            sample_rate);

    double max_amp = 0.0;
    const size_t window =
            std::min<size_t>(stochastic_ir.size(), static_cast<size_t>(4096));
    for (size_t i = 0; i < window; ++i) {
        max_amp =
                std::max(max_amp,
                         static_cast<double>(std::fabs(stochastic_ir[i])));
    }
    EXPECT_LT(max_amp, 1e-6);
}
