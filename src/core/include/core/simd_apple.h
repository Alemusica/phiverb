#pragma once

#ifdef __APPLE__

#include <TargetConditionals.h>

#if TARGET_CPU_ARM64
#include <arm_neon.h>

namespace wayverb {
namespace core {

/// SIMD operations for Apple Silicon using NEON intrinsics.
class simd_apple final {
public:
    using float4 = float32x4_t;
    using int4 = int32x4_t;
    
    // Basic load/store operations
    static inline float4 load(const float* ptr) {
        return vld1q_f32(ptr);
    }
    
    static inline void store(float* ptr, float4 v) {
        vst1q_f32(ptr, v);
    }
    
    // Arithmetic operations
    static inline float4 add(float4 a, float4 b) {
        return vaddq_f32(a, b);
    }
    
    static inline float4 sub(float4 a, float4 b) {
        return vsubq_f32(a, b);
    }
    
    static inline float4 mul(float4 a, float4 b) {
        return vmulq_f32(a, b);
    }
    
    static inline float4 fma(float4 a, float4 b, float4 c) {
        // Fused multiply-add: c + a * b
        return vmlaq_f32(c, a, b);
    }
    
    // Reduction operations
    static inline float sum(float4 v) {
        float32x2_t sum_pairs = vadd_f32(vget_low_f32(v), vget_high_f32(v));
        float32x2_t sum = vpadd_f32(sum_pairs, sum_pairs);
        return vget_lane_f32(sum, 0);
    }
    
    /// Process a pressure field with SIMD vectorization.
    /// Applies a damping factor to all elements in the field.
    static void process_pressure_field_simd(float* field, 
                                           size_t size, 
                                           float damping) {
        const float4 damping_vec = vdupq_n_f32(damping);
        const size_t simd_size = 4;
        const size_t aligned_size = (size / simd_size) * simd_size;
        
        // Process aligned portion with SIMD
        for (size_t i = 0; i < aligned_size; i += simd_size) {
            float4 v = load(&field[i]);
            v = mul(v, damping_vec);
            store(&field[i], v);
        }
        
        // Handle remainder elements
        for (size_t i = aligned_size; i < size; ++i) {
            field[i] *= damping;
        }
    }
    
    /// Apply a scalar operation to a pressure field with SIMD.
    static void add_scalar_simd(float* field, size_t size, float scalar) {
        const float4 scalar_vec = vdupq_n_f32(scalar);
        const size_t simd_size = 4;
        const size_t aligned_size = (size / simd_size) * simd_size;
        
        for (size_t i = 0; i < aligned_size; i += simd_size) {
            float4 v = load(&field[i]);
            v = add(v, scalar_vec);
            store(&field[i], v);
        }
        
        for (size_t i = aligned_size; i < size; ++i) {
            field[i] += scalar;
        }
    }
};

}  // namespace core
}  // namespace wayverb

#endif  // TARGET_CPU_ARM64
#endif  // __APPLE__
