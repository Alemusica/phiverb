#pragma once

#include "core/cl/representation.h"
#include "core/cl/traits.h"

#include <algorithm>

namespace wayverb {
namespace waveguide {

#ifndef WAYVERB_FORCE_SINGLE_PRECISION
#  if defined(__APPLE__)
// Default to single-precision on Apple platforms unless explicitly overridden.
#    define WAYVERB_FORCE_SINGLE_PRECISION 1
#  else
#    define WAYVERB_FORCE_SINGLE_PRECISION 0
#  endif
#endif

constexpr size_t biquad_order{2};
constexpr size_t biquad_sections{3};

constexpr size_t canonical_filter_order{biquad_order * biquad_sections};
constexpr size_t canonical_filter_storage{canonical_filter_order + 2};
constexpr size_t canonical_coeff_order{biquad_order * biquad_sections};
constexpr size_t canonical_coeff_storage{canonical_coeff_order + 2};

#if defined(WAYVERB_FORCE_SINGLE_PRECISION) && WAYVERB_FORCE_SINGLE_PRECISION
using filt_real = cl_float;
#else
using filt_real = cl_double;
#endif

////////////////////////////////////////////////////////////////////////////////

/// Just an array of filt_real to use as a delay line.
template <size_t o>
struct alignas(1 << 3) memory final {
    static constexpr size_t order = o;
    filt_real array[order]{};
};

template <size_t D>
inline bool operator==(const memory<D>& a, const memory<D>& b) {
    return std::equal(
            std::begin(a.array), std::end(a.array), std::begin(b.array));
}

template <size_t D>
inline bool operator!=(const memory<D>& a, const memory<D>& b) {
    return !(a == b);
}

////////////////////////////////////////////////////////////////////////////////

/// IIR filter coefficient storage.
template <size_t o>
struct alignas(1 << 3) coefficients final {
    static constexpr auto order = o;
    filt_real b[order + 1]{};
    filt_real a[order + 1]{};
};

template <size_t D>
inline bool operator==(const coefficients<D>& a, const coefficients<D>& b) {
    return std::equal(std::begin(a.a), std::end(a.a), std::begin(b.a)) &&
           std::equal(std::begin(a.b), std::end(a.b), std::begin(b.b));
}

template <size_t D>
inline bool operator!=(const coefficients<D>& a, const coefficients<D>& b) {
    return !(a == b);
}

////////////////////////////////////////////////////////////////////////////////

using memory_biquad = memory<biquad_order>;

using coefficients_biquad = coefficients<biquad_order>;

struct alignas(1 << 5) memory_canonical final {
    static constexpr size_t order = canonical_filter_order;
    static constexpr size_t storage_size = canonical_filter_storage;
    filt_real array[storage_size]{};

    memory_canonical() = default;
};

struct alignas(1 << 6) coefficients_canonical final {
    static constexpr auto order = canonical_coeff_order;
    static constexpr auto storage_size = canonical_coeff_storage;
    filt_real b[storage_size]{};
    filt_real a[storage_size]{};

    coefficients_canonical() = default;
};

static_assert(sizeof(memory_canonical) ==
                      memory_canonical::storage_size * sizeof(filt_real),
              "memory_canonical size mismatch");
static_assert(sizeof(coefficients_canonical) ==
                      coefficients_canonical::storage_size * sizeof(filt_real) *
                              2,
              "coefficients_canonical size mismatch");

inline bool operator==(const memory_canonical& a, const memory_canonical& b) {
    return std::equal(
            std::begin(a.array), std::begin(a.array) + memory_canonical::order, std::begin(b.array));
}

inline bool operator!=(const memory_canonical& a, const memory_canonical& b) {
    return !(a == b);
}

inline bool operator==(const coefficients_canonical& a,
                       const coefficients_canonical& b) {
    return std::equal(std::begin(a.a),
                      std::begin(a.a) + coefficients_canonical::order + 1,
                      std::begin(b.a)) &&
           std::equal(std::begin(a.b),
                      std::begin(a.b) + coefficients_canonical::order + 1,
                      std::begin(b.b));
}

inline bool operator!=(const coefficients_canonical& a,
                       const coefficients_canonical& b) {
    return !(a == b);
}

using memory_6 = memory_canonical;
using coefficients_6 = coefficients_canonical;

////////////////////////////////////////////////////////////////////////////////

/// Several biquad delay lines in a row.
struct alignas(1 << 3) biquad_memory_array final {
    memory_biquad array[biquad_sections]{};
};

////////////////////////////////////////////////////////////////////////////////

/// Several sets of biquad parameters.
struct alignas(1 << 3) biquad_coefficients_array final {
    coefficients_biquad array[biquad_sections]{};
};

}  // namespace waveguide

template <>
struct core::cl_representation<waveguide::filt_real> final {
#if defined(WAYVERB_FORCE_SINGLE_PRECISION) && WAYVERB_FORCE_SINGLE_PRECISION
    static constexpr auto value = R"(
typedef float filt_real;
)";
#else
    static constexpr auto value = R"(
typedef double filt_real;
)";
#endif
};

template <>
struct core::cl_representation<waveguide::memory_biquad> final {
    static const std::string value;
};

template <>
struct core::cl_representation<waveguide::coefficients_biquad> final {
    static const std::string value;
};

template <>
struct core::cl_representation<waveguide::memory_canonical> final {
    static const std::string value;
};

template <>
struct core::cl_representation<waveguide::coefficients_canonical> final {
    static const std::string value;
};

template <>
struct core::cl_representation<waveguide::biquad_memory_array> final {
    static constexpr auto value = R"(
typedef struct {
    memory_biquad array[BIQUAD_SECTIONS];
} biquad_memory_array;
)";
};

template <>
struct core::cl_representation<waveguide::biquad_coefficients_array> final {
    static constexpr auto value = R"(
typedef struct {
    coefficients_biquad array[BIQUAD_SECTIONS];
} biquad_coefficients_array;
)";
};

}  // namespace wayverb
