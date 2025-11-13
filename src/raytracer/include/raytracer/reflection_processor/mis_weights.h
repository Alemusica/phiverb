#pragma once

#include <cstddef>

namespace wayverb {
namespace raytracer {
namespace reflection_processor {

struct mis_weights final {
    float image_source;
    float path_tracer;
};

constexpr float default_mis_delta_pdf = 1.0e6f;

inline mis_weights compute_mis_weights(std::size_t total_rays,
                                       float delta_pdf = default_mis_delta_pdf) {
    if (total_rays == 0) {
        return mis_weights{1.0f, 0.0f};
    }

    const float n_pt = static_cast<float>(total_rays);
    const float denom = delta_pdf + n_pt;
    if (denom == 0.0f) {
        return mis_weights{0.0f, 1.0f};
    }

    return mis_weights{delta_pdf / denom, n_pt / denom};
}

}  // namespace reflection_processor
}  // namespace raytracer
}  // namespace wayverb

