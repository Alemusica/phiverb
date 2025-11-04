#include "waveguide/filters.h"

#include "utilities/foldl.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace wayverb {
namespace waveguide {

coefficients_biquad get_peak_coefficients(const filter_descriptor& n) {
    const auto to_filt = [](double v) {
        return static_cast<filt_real>(v);
    };

    const filt_real A = to_filt(util::decibels::db2a(n.gain / 2));
    const filt_real w0 = to_filt(2.0) * to_filt(M_PI) * to_filt(n.centre);
    const filt_real cw0 = std::cos(w0);
    const filt_real sw0 = std::sin(w0);
    const filt_real alpha = (sw0 / to_filt(2.0)) * to_filt(n.Q);
    const filt_real a0 = to_filt(1.0) + alpha / A;
    return coefficients_biquad{
            {(to_filt(1.0) + (alpha * A)) / a0,
             (-to_filt(2.0) * cw0) / a0,
             (to_filt(1.0) - alpha * A) / a0},
            {to_filt(1.0),
             (-to_filt(2.0) * cw0) / a0,
             (to_filt(1.0) - alpha / A) / a0}};
}

biquad_coefficients_array get_peak_biquads_array(
        const std::array<filter_descriptor, biquad_sections>& n) {
    return get_biquads_array(get_peak_coefficients, n);
}

coefficients_canonical convolve(const biquad_coefficients_array& a) {
    const auto accumulated = util::foldl(
            [](const auto& i, const auto& j) { return convolve(i, j); },
            a.array);

    coefficients_canonical result{};
    std::copy(std::begin(accumulated.b),
              std::begin(accumulated.b) + coefficients_canonical::order + 1,
              result.b);
    std::copy(std::begin(accumulated.a),
              std::begin(accumulated.a) + coefficients_canonical::order + 1,
              result.a);
    return result;
}

}  // namespace waveguide
}  // namespace wayverb
