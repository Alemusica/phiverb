#pragma once

#include <cmath>
#include <limits>
#include <algorithm>

namespace wayverb {
namespace waveguide {

class numerical_safety final {
public:
    // Use inline constants to avoid ODR issues
    static constexpr float epsilon() { return 1e-6f; }
    static constexpr float max_coefficient() { return 0.999f; }
    static constexpr float pi() { return 3.14159265358979323846f; }
    
    /// Safe division that avoids division by zero and returns finite results.
    static inline float safe_divide(float numerator, float denominator) {
        if (std::abs(denominator) < epsilon()) {
            return 0.0f;
        }
        const float result = numerator / denominator;
        return std::isfinite(result) ? result : 0.0f;
    }
    
    /// Safe sine calculation that clamps to finite range.
    /// Note: This is a simplified version that assumes angles are already in reasonable range.
    /// For production use, consider wrapping angle to [0, 2π] first.
    static inline float safe_sin(float angle) {
        // Just return regular sin, checking for finite result
        const float result = std::sin(angle);
        return std::isfinite(result) ? result : 0.0f;
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
        return std::max(-max_coefficient(), std::min(max_coefficient(), coeff));
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
