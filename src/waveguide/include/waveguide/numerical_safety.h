#pragma once

#include <cmath>
#include <limits>
#include <algorithm>

namespace wayverb {
namespace waveguide {

class numerical_safety final {
public:
    static constexpr float epsilon = 1e-6f;
    static constexpr float max_coefficient = 0.999f;
    
    /// Safe division that avoids division by zero and returns finite results.
    static inline float safe_divide(float numerator, float denominator) {
        if (std::abs(denominator) < epsilon) {
            return 0.0f;
        }
        const float result = numerator / denominator;
        return std::isfinite(result) ? result : 0.0f;
    }
    
    /// Safe sine calculation that avoids singularities at 0 and π.
    static inline float safe_sin(float angle) {
        // Clamp angle to avoid problems at boundaries
        angle = std::max(epsilon, std::min(float(M_PI) - epsilon, angle));
        return std::sin(angle);
    }
    
    /// Sanitize a float value by replacing NaN/Inf with a default value.
    static inline float sanitize(float value, float default_value = 0.0f) {
        if (!std::isfinite(value)) {
            return default_value;
        }
        return value;
    }
    
    /// Sanitize and clamp a reflection coefficient to valid range.
    static inline float sanitize_reflection_coefficient(float coeff) {
        if (!std::isfinite(coeff)) {
            return 0.0f;
        }
        return std::max(-max_coefficient, std::min(max_coefficient, coeff));
    }
    
    /// Sanitize an array of pressure values, replacing NaN/Inf with 0.
    /// Returns the count of NaN/Inf values found.
    static size_t sanitize_pressure_field(float* field, size_t size) {
        size_t nan_count = 0;
        for (size_t i = 0; i < size; ++i) {
            if (!std::isfinite(field[i])) {
                field[i] = 0.0f;
                ++nan_count;
            } else {
                // Clamp extreme values to prevent overflow
                field[i] = std::max(-1e6f, std::min(1e6f, field[i]));
            }
        }
        return nan_count;
    }
};

}  // namespace waveguide
}  // namespace wayverb
