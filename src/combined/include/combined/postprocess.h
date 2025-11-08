#pragma once

#include "raytracer/postprocess.h"

#include "waveguide/canonical.h"
#include "waveguide/config.h"
#include "waveguide/postprocess.h"

#include "core/dsp_vector_ops.h"
#include "core/sinc.h"
#include "core/sum_ranges.h"

#include "audio_file/audio_file.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace wayverb {
namespace combined {

template <typename Histogram>
struct combined_results final {
    raytracer::simulation_results<Histogram> raytracer;
    util::aligned::vector<waveguide::bandpass_band> waveguide;
};

template <typename Histogram>
auto make_combined_results(
        raytracer::simulation_results<Histogram> raytracer,
        util::aligned::vector<waveguide::bandpass_band> waveguide) {
    return combined_results<Histogram>{std::move(raytracer),
                                       std::move(waveguide)};
}

////////////////////////////////////////////////////////////////////////////////

template <typename LoIt, typename HiIt>
auto crossover_filter(LoIt b_lo,
                      LoIt e_lo,
                      HiIt b_hi,
                      HiIt e_hi,
                      double cutoff,
                      double width) {
    frequency_domain::filter filt{
            frequency_domain::best_fft_length(std::max(
                    std::distance(b_lo, e_lo), std::distance(b_hi, e_hi)))
            << 2};

    constexpr auto l = 0;

    const auto run_filter = [&](auto b, auto e, auto mag_func) {
        auto ret = std::vector<float>(std::distance(b, e));
        filt.run(b, e, begin(ret), [&](auto cplx, auto freq) {
            return cplx * static_cast<float>(mag_func(freq, cutoff, width, l));
        });
        return ret;
    };

    const auto lo =
            run_filter(b_lo, e_lo, frequency_domain::compute_lopass_magnitude);
    const auto hi =
            run_filter(b_hi, e_hi, frequency_domain::compute_hipass_magnitude);

    return core::sum_vectors(lo, hi);
}

////////////////////////////////////////////////////////////////////////////////

struct max_frequency_functor final {
    template <typename T>
    auto operator()(T&& t) const {
        return t.valid_hz.get_max();
    }
};

template <typename Histogram, typename Method>
auto postprocess(const combined_results<Histogram>& input,
                 const Method& method,
                 const glm::vec3& source_position,
                 const glm::vec3& receiver_position,
                 double room_volume,
                 const core::environment& environment,
                 double output_sample_rate) {
    //  Individual processing.
    const auto waveguide_processed =
            waveguide::postprocess(input.waveguide,
                                   method,
                                   environment.acoustic_impedance,
                                   output_sample_rate);

    const auto raytracer_processed = raytracer::postprocess(input.raytracer,
                                                            method,
                                                            receiver_position,
                                                            room_volume,
                                                            environment,
                                                            output_sample_rate);

    const auto make_iterator = [](auto it) {
        return util::make_mapping_iterator_adapter(std::move(it),
                                                   max_frequency_functor{});
    };

    auto has_energy = [](const auto& v) {
        for (const auto& s : v) {
            if (std::abs(s) > 1.0e-15f) return true;
        }
        return false;
    };

    const auto log_non_finite = [](const char* label, const auto& buffer) {
        const auto count = core::count_non_finite(buffer);
        if (count != 0) {
            std::cerr << "[combined] detected " << count
                      << " non-finite samples in " << label << '\n';
        }
    };

    const auto log_channel_stats = [](const char* label, const auto& buffer) {
        size_t non_zero = 0;
        float max_mag = 0.0f;
        for (auto sample : buffer) {
            const auto mag = std::abs(sample);
            if (mag > 0.0f) {
                ++non_zero;
                max_mag = std::max(max_mag, mag);
            }
        }
        std::cerr << "[combined] " << label << ": non-zero samples=" << non_zero
                  << '/' << buffer.size() << " max|x|=" << max_mag << '\n';
    };

    log_non_finite("waveguide postprocess output", waveguide_processed);
    log_non_finite("raytracer postprocess output", raytracer_processed);
    log_channel_stats("waveguide postprocess output", waveguide_processed);
    log_channel_stats("raytracer postprocess output", raytracer_processed);

    const bool allow_silent_fallback =
            std::getenv("WAYVERB_ALLOW_SILENT_FALLBACK") != nullptr;

    if (input.waveguide.empty()) {
        auto out = raytracer_processed;
        if (!has_energy(out)) {
            log_channel_stats("raytracer-only output", out);
            if (!allow_silent_fallback) {
                std::cerr << "[combined] ERROR: raytracer+waveguide produced "
                             "silent IR (waveguide path empty).\n";
                throw std::runtime_error{"All channels are silent."};
            }
            // Fallback: inject direct-path free-field impulse to avoid silent IRs.
            const auto d = distance(source_position, receiver_position);
            const auto idx = static_cast<size_t>(std::floor(
                    d * output_sample_rate / environment.speed_of_sound));
            const size_t n = std::max<size_t>(out.size(), idx + 1);
            out.resize(n, 0.0f);
            const float amp = static_cast<float>(1.0 / std::max<double>(1.0e-6, d));
            out[idx] += amp;
            std::cerr << "[combined] WARNING: raytracer+waveguide produced silent IR; "
                         "injecting direct-path fallback (d="
                      << d << ", idx=" << idx
                      << "). This executes only when WAYVERB_ALLOW_SILENT_FALLBACK=1.\n";
        }
        return out;
    }

    const auto cutoff = *std::max_element(make_iterator(begin(input.waveguide)),
                                          make_iterator(end(input.waveguide))) /
                        output_sample_rate;
    const auto width = 0.2;  //  Wider = more natural-sounding
    auto filtered = crossover_filter(begin(waveguide_processed),
                                     end(waveguide_processed),
                                     begin(raytracer_processed),
                                     end(raytracer_processed),
                                     cutoff,
                                     width);

    //  Just in case the start has a bit of a dc offset, we do a sneaky window.
    const auto window_length =
            std::min(filtered.size(),
                     static_cast<size_t>(std::floor(
                             distance(source_position, receiver_position) *
                             output_sample_rate / environment.speed_of_sound)));

    if (window_length == 0) {
        if (!has_energy(filtered)) {
            log_channel_stats("combined mix (no window)", filtered);
            if (!allow_silent_fallback) {
                std::cerr << "[combined] ERROR: combined IR energy is zero.\n";
                throw std::runtime_error{"All channels are silent."};
            }
        }
        return filtered;
    }

    const auto window = core::left_hanning(std::floor(window_length));

    //  Multiply together the window and filtered signal.
    std::transform(
            begin(window),
            end(window),
            begin(filtered),
            begin(filtered),
            [](auto envelope, auto signal) { return envelope * signal; });

    const auto sanitize = [](auto& buffer) {
        size_t replaced = 0;
        for (auto& sample : buffer) {
            if (!std::isfinite(sample)) {
                sample = 0.0f;
                ++replaced;
            }
        }
        return replaced;
    };

    if (const auto sanitized = sanitize(filtered); sanitized != 0) {
        std::cerr << "[combined] sanitized " << sanitized
                  << " non-finite samples in crossover output before fallback."
                  << '\n';
    }

    if (!has_energy(filtered)) {
        log_channel_stats("combined mix (windowed)", filtered);
        if (!allow_silent_fallback) {
            std::cerr << "[combined] ERROR: combined IR energy is zero "
                         "after crossover/windowing.\n";
            throw std::runtime_error{"All channels are silent."};
        }
        // Fallback: guarantee a minimal non-silent IR when explicitly allowed.
        const auto d = distance(source_position, receiver_position);
        const auto idx = static_cast<size_t>(std::floor(
                d * output_sample_rate / environment.speed_of_sound));
        const size_t n = std::max<size_t>(filtered.size(), idx + 1);
        auto out = filtered;
        out.resize(n, 0.0f);
        const float amp = static_cast<float>(1.0 / std::max<double>(1.0e-6, d));
        const auto sanitized_fallback = sanitize(out);
        if (sanitized_fallback != 0) {
            std::cerr
                    << "[combined] sanitized " << sanitized_fallback
                    << " non-finite samples in fallback buffer before impulse."
                    << '\n';
        }
        const float before = out[idx];
        out[idx] += amp;
        const float after = out[idx];
        const auto injected_max = core::max_mag(out);
        std::cerr << "[combined] WARNING: postprocess produced silent IR; "
                     "injecting direct-path fallback (d="
                  << d << ", idx=" << idx << ", size=" << filtered.size()
                  << ", amp=" << amp << ", before=" << before
                  << ", after=" << after << ", max_after=" << injected_max
                  << "). This executes only when WAYVERB_ALLOW_SILENT_FALLBACK=1.\n";
        return out;
    }

    return filtered;
}

}  // namespace combined
}  // namespace wayverb
