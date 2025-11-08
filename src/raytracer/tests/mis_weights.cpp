#include "raytracer/reflection_processor/mis_weights.h"

#include "gtest/gtest.h"

using wayverb::raytracer::reflection_processor::compute_mis_weights;

TEST(mis_weights, zero_rays_defaults) {
    const auto weights = compute_mis_weights(0);
    EXPECT_FLOAT_EQ(1.0f, weights.image_source);
    EXPECT_FLOAT_EQ(0.0f, weights.path_tracer);
}

TEST(mis_weights, balanced_distribution) {
    constexpr float delta_pdf = 1.0f;
    const auto weights = compute_mis_weights(1, delta_pdf);
    EXPECT_NEAR(0.5f, weights.image_source, 1e-5f);
    EXPECT_NEAR(0.5f, weights.path_tracer, 1e-5f);
}

