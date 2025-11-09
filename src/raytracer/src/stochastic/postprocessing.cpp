#include "raytracer/stochastic/postprocessing.h"

#include "core/cl/iterator.h"
#include "core/mixdown.h"
#include "core/pressure_intensity.h"

#include "utilities/for_each.h"
#include "utilities/map.h"

#include <array>
#include <iostream>

namespace wayverb {
namespace raytracer {
namespace stochastic {

double constant_mean_event_occurrence(double speed_of_sound,
                                      double room_volume) {
    return 4 * M_PI * std::pow(speed_of_sound, 3.0) / room_volume;
}

double mean_event_occurrence(double constant, double t) {
    return std::min(constant * std::pow(t, 2.0), 10000.0);
}

double t0(double constant) {
    return std::pow(2.0 * std::log(2.0) / constant, 1.0 / 3.0);
}

dirac_sequence generate_dirac_sequence(double speed_of_sound,
                                       double room_volume,
                                       double sample_rate,
                                       double max_time) {
    const auto constant_mean_occurrence =
            constant_mean_event_occurrence(speed_of_sound, room_volume);

    std::default_random_engine engine{std::random_device{}()};

    util::aligned::vector<float> ret(std::ceil(max_time * sample_rate), 0);
    for (auto t = t0(constant_mean_occurrence); t < max_time;
         t += interval_size(
                 engine, mean_event_occurrence(constant_mean_occurrence, t))) {
        const auto sample_index = t * sample_rate;
        const size_t twice = 2 * sample_index;
        const bool negative = (twice % 2) != 0;
        ret[sample_index] = negative ? -1 : 1;
    }
    return {ret, sample_rate};
}

void sum_histograms(energy_histogram& a, const energy_histogram& b) {
    sum_vectors(a.histogram, b.histogram);
    a.sample_rate = b.sample_rate;
}

util::aligned::vector<core::bands_type> weight_sequence(
        const energy_histogram& histogram,
        const dirac_sequence& sequence,
        double acoustic_impedance,
        const std::array<double, core::simulation_bands>&
                sqrt_bandwidth_fractions) {
    auto ret = util::map_to_vector(
            begin(sequence.sequence), end(sequence.sequence), [](auto i) {
                return core::make_bands_type(i);
            });

    const auto convert_index = [&](auto ind) -> size_t {
        return ind * sequence.sample_rate / histogram.sample_rate;
    };

    const auto ideal_sequence_length =
            convert_index(histogram.histogram.size());
    if (ideal_sequence_length < ret.size()) {
        ret.resize(ideal_sequence_length);
    }

    for (auto i = 0ul, e = histogram.histogram.size(); i != e; ++i) {
        const auto get_sequence_index = [&](auto ind) {
            return std::min(convert_index(ind), ret.size());
        };

        const auto beg = get_sequence_index(i);
        const auto end = get_sequence_index(i + 1);

        const auto squared_summed = frequency_domain::square_sum(
                begin(sequence.sequence) + beg, begin(sequence.sequence) + end);
        core::bands_type scale_factor{};
        if (squared_summed != 0.0f) {
            const auto pressure = core::intensity_to_pressure(
                    histogram.histogram[i] / squared_summed,
                    acoustic_impedance);
            for (size_t band = 0; band < core::simulation_bands; ++band) {
                scale_factor.s[band] = static_cast<float>(
                        pressure.s[band] *
                        sqrt_bandwidth_fractions[band]);
            }
        }

        std::for_each(begin(ret) + beg, begin(ret) + end, [&](auto& i) {
            i *= scale_factor;
        });
    }

    return ret;
}

util::aligned::vector<float> postprocessing(const energy_histogram& histogram,
                                            const dirac_sequence& sequence,
                                            double acoustic_impedance) {
    //  Each diffuse rain band represents a fraction of the Nyquist bandwidth.
    //  Use the real filter bandwidths (Hz) to mirror Eq. 5.47.
    const auto params_hz = hrtf_data::hrtf_band_params_hz();
    std::array<double, core::simulation_bands> sqrt_bandwidth_fractions{};
    const double nyquist = std::max(sequence.sample_rate * 0.5, 1.0);
    for (size_t band = 0; band < core::simulation_bands; ++band) {
        const double bandwidth_hz =
                params_hz.edges[band + 1] - params_hz.edges[band];
        const double fraction =
                std::max(bandwidth_hz / nyquist, 0.0);
        sqrt_bandwidth_fractions[band] = std::sqrt(fraction);
    }

    auto weighted = weight_sequence(histogram,
                                    sequence,
                                    acoustic_impedance,
                                    sqrt_bandwidth_fractions);
    return core::multiband_filter_and_mixdown(
            begin(weighted),
            end(weighted),
            sequence.sample_rate,
            [](auto it, auto index) {
                return core::make_cl_type_iterator(std::move(it), index);
            });
}

}  // namespace stochastic
}  // namespace raytracer
}  // namespace wayverb
