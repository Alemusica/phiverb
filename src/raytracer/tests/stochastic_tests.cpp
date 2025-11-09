#include "raytracer/stochastic/finder.h"

#include "core/conversions.h"
#include "core/geo/box.h"
#include "core/scene_data_loader.h"
#include "core/spatial_division/voxelised_scene_data.h"

#include "gtest/gtest.h"

#ifndef OBJ_PATH
#define OBJ_PATH ""
#endif

using namespace wayverb::raytracer;
using namespace wayverb::core;

TEST(stochastic, bad_reflections_box) {
    const geo::box box{glm::vec3{0, 0, 0}, glm::vec3{4, 3, 6}};
    constexpr glm::vec3 source{1, 2, 1}, receiver{2, 1, 5};
    constexpr auto s = 0.01;
    constexpr auto d = 0.1;
    constexpr auto surface = make_surface<simulation_bands>(s, d);

    const compute_context cc{};

    const auto scene = geo::get_scene_data(box, surface);
    const auto voxelised = make_voxelised_scene_data(scene, 5, 0.1f);

    const scene_buffers buffers{cc.context, voxelised};

    const util::aligned::vector<reflection> bad_reflections{
            reflection{cl_float3{{2.66277409, 0.0182733424, 6}},
                       10,
                       0.0f,
                       1,
                       1,
                       0,
                       0},
            reflection{cl_float3{{3.34029818, 1.76905692, 6}},
                       10,
                       0.0f,
                       1,
                       1,
                       0,
                       0},
            reflection{cl_float3{{4, 2.46449089, 1.54567611}},
                       7,
                       0.0f,
                       1,
                       1,
                       0,
                       0},
    };

    const auto receiver_radius = 1.0f;

    stochastic::finder diff{
            cc,
            bad_reflections.size(),
            source,
            receiver,
            receiver_radius,
            stochastic::compute_ray_energy(
                    bad_reflections.size(), source, receiver, receiver_radius)};

    diff.process(begin(bad_reflections), end(bad_reflections), buffers);
}

TEST(stochastic, bad_reflections_vault) {
    constexpr glm::vec3 source{0, 1, 0}, receiver{0, 1, 1};

    const compute_context cc{};

    const auto scene = scene_with_extracted_surfaces(
            *scene_data_loader{OBJ_PATH}.get_scene_data(),
            util::aligned::unordered_map<std::string,
                                         surface<simulation_bands>>{});
    const auto voxelised = make_voxelised_scene_data(scene, 5, 0.1f);

    const scene_buffers buffers{cc.context, voxelised};

    const util::aligned::vector<reflection> bad_reflections{
            reflection{cl_float3{{2.29054403, 1.00505638, -1.5}},
                       2906,
                       0.0f,
                       1,
                       1,
                       0,
                       0},
            reflection{cl_float3{{5.28400469, 3.0999999, -3.8193748}},
                       2671,
                       0.0f,
                       1,
                       1,
                       0,
                       0},
            reflection{cl_float3{{5.29999971, 2.40043592, -2.991467}},
                       2808,
                       0.0f,
                       1,
                       1,
                       0,
                       0},
            reflection{cl_float3{{-1.29793882, 2.44466829, 5.30000019}},
                       1705,
                       0.0f,
                       1,
                       1,
                       0,
                       0},
    };

    const auto receiver_radius = 1.0f;

    stochastic::finder diff{
            cc,
            bad_reflections.size(),
            source,
            receiver,
            receiver_radius,
            stochastic::compute_ray_energy(
                    bad_reflections.size(), source, receiver, receiver_radius)};

    diff.process(begin(bad_reflections), end(bad_reflections), buffers);
}

TEST(stochastic, diffuse_rain_deterministic) {
    const compute_context cc{};
    const geo::box box{glm::vec3{0}, glm::vec3{4, 3, 6}};
    constexpr float absorption = 0.2f;
    constexpr float scattering = 0.6f;
    const auto surface = make_surface<simulation_bands>(absorption, scattering);
    const auto scene = geo::get_scene_data(box, surface);
    const auto voxelised = make_voxelised_scene_data(scene, 5, 0.1f);
    const scene_buffers buffers{cc.context, voxelised};

    ASSERT_FALSE(scene.get_triangles().empty());
    const auto& triangles = scene.get_triangles();
    const auto& vertices = scene.get_vertices();
    const auto tri = triangles.front();
    const glm::vec3 centroid =
            (to_vec3{}(vertices[tri.v0]) + to_vec3{}(vertices[tri.v1]) +
             to_vec3{}(vertices[tri.v2])) /
            3.0f;

    const glm::vec3 source{1.0f, 1.0f, 1.0f};
    const glm::vec3 receiver{2.0f, 1.5f, 1.5f};
    constexpr float receiver_radius = 0.2f;
    const float ray_energy =
            stochastic::compute_ray_energy(1, source, receiver, receiver_radius);

    const auto run_with_choice = [&](bool sampled_diffuse) {
        stochastic::finder finder{cc,
                                  1,
                                  source,
                                  receiver,
                                  receiver_radius,
                                  ray_energy};

        reflection refl{};
        refl.position = to_cl_float3{}(centroid);
        refl.triangle = 0;
        refl.scatter_probability = scattering;
        refl.keep_going = 1;
        refl.receiver_visible = 1;
        refl.sampled_diffuse = static_cast<cl_char>(sampled_diffuse);

        util::aligned::vector<reflection> reflections{refl};
        const auto results =
                finder.process(begin(reflections), end(reflections), buffers);
        EXPECT_FALSE(results.stochastic.empty());
        return results.stochastic.front().volume;
    };

    const auto rain_specular = run_with_choice(false);
    const auto rain_diffuse = run_with_choice(true);

    for (size_t i = 0; i < simulation_bands; ++i) {
        EXPECT_NEAR(rain_specular.s[i], rain_diffuse.s[i], 1e-6f)
                << "band " << i << " mismatch";
    }
}
