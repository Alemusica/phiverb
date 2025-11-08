#include <metal_stdlib>
using namespace metal;

struct DiagCounters {
    atomic_uint nanCount;
};

inline float finite_or_zero(float x, device atomic_uint* counter) {
    if (!isfinite(x)) {
        atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
        return 0.0f;
    }
    return x;
}
