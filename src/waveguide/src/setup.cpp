#include "waveguide/setup.h"
#include "waveguide/mesh_setup_program.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace wayverb {
namespace waveguide {

namespace {

void sanitize_coefficients(util::aligned::vector<coefficients_canonical>& coeffs) {
    constexpr double kMinB0 = 1.0e-12;
    size_t sanitized = 0;
    double min_b0 = std::numeric_limits<double>::infinity();
    double max_b0 = 0.0;
    std::cerr << "[waveguide] coefficient sets: " << coeffs.size() << '\n';
    for (auto& coeff : coeffs) {
        bool all_zero = true;
        for (size_t i = 0; i < coefficients_canonical::storage_size; ++i) {
            if (coeff.a[i] != static_cast<filt_real>(0) ||
                coeff.b[i] != static_cast<filt_real>(0)) {
                all_zero = false;
                break;
            }
        }
        const auto b0 = static_cast<double>(coeff.b[0]);
        min_b0 = std::min(min_b0, std::fabs(b0));
        max_b0 = std::max(max_b0, std::fabs(b0));
        const bool invalid_b0 =
                !std::isfinite(b0) || std::fabs(b0) < kMinB0;
        if (all_zero || invalid_b0) {
            std::fill(std::begin(coeff.a),
                      std::begin(coeff.a) + coefficients_canonical::storage_size,
                      static_cast<filt_real>(0));
            std::fill(std::begin(coeff.b),
                      std::begin(coeff.b) + coefficients_canonical::storage_size,
                      static_cast<filt_real>(0));
            coeff.b[0] = static_cast<filt_real>(1);
            ++sanitized;
        }
    }
    if (sanitized != 0) {
        std::cerr << "[waveguide] sanitized " << sanitized
                  << " boundary coefficient set(s); applied rigid fallback\n";
        std::cerr << "    b0 range before fallback: min|b0|=" << min_b0
                  << " max|b0|=" << max_b0 << '\n';
    } else if (!coeffs.empty()) {
        std::cerr << "[waveguide] coefficient stats: min|b0|=" << min_b0
                  << " max|b0|=" << max_b0 << '\n';
    }
}

}  // namespace

vectors::vectors(util::aligned::vector<condensed_node> nodes,
                 util::aligned::vector<coefficients_canonical> coefficients,
                 boundary_layout boundary_layout)
        : condensed_nodes_(std::move(nodes))
        , coefficients_(std::move(coefficients))
        , boundary_layout_(std::move(boundary_layout)) {
    sanitize_coefficients(coefficients_);
}

const util::aligned::vector<condensed_node>& vectors::get_condensed_nodes()
        const {
    return condensed_nodes_;
}

const util::aligned::vector<coefficients_canonical>& vectors::get_coefficients()
        const {
    return coefficients_;
}

void vectors::set_coefficients(coefficients_canonical c) {
    std::fill(begin(coefficients_), end(coefficients_), c);
}

void vectors::set_coefficients(
        util::aligned::vector<coefficients_canonical> c) {
    if (c.size() != coefficients_.size()) {
        throw std::runtime_error(
                "Size of new coefficients vector must be equal to the existing "
                "one in order to maintain object invariants.");
    }
    coefficients_ = std::move(c);
}

}  // namespace waveguide
}  // namespace wayverb
