#pragma once

#include "waveguide/bandpass_band.h"
#include "waveguide/calibration.h"
#include "waveguide/fitted_boundary.h"
#include "waveguide/make_transparent.h"
#include "waveguide/pcs.h"
#include "waveguide/postprocessor/directional_receiver.h"
#include "waveguide/preprocessor/soft_source.h"
#include "waveguide/simulation_parameters.h"
#include "waveguide/waveguide.h"
#include "waveguide/backend_selector.h"

#include "utilities/aligned/vector.h"

#include "core/callback_accumulator.h"
#include "core/environment.h"
#include "core/reverb_time.h"

#include "hrtf/multiband.h"

#include <algorithm>
#include <cmath>
#include <iostream>

/// \file canonical.h
/// The waveguide algorithm in waveguide.h is modular, in that
/// you can supply different combinations of sources and receivers.
/// The method below drives the combination deemed to be most approriate for
/// single-run simulation.

namespace wayverb {
namespace waveguide {
namespace detail {

constexpr size_t kMaxPcsKernelLength = 1u << 15;
constexpr double kPcsRadiusMeters = 0.05;
constexpr double kPcsSphereMassKg = 0.025;
constexpr double kPcsLowCutoffHz = 100.0;
constexpr double kPcsLowQ = 0.7;
constexpr float kTransparentGuardMagnitude = 1.0e6f;

inline util::aligned::vector<float> make_pcs_transparent_signal(
        size_t steps,
        double acoustic_impedance,
        double speed_of_sound,
        double sample_rate,
        double grid_spacing) {
    if (steps == 0) {
        return {};
    }

    const auto kernel_length = std::max<size_t>(
            1u, std::min<size_t>(steps, kMaxPcsKernelLength));

    auto pcs = design_pcs_source(kernel_length,
                                 acoustic_impedance,
                                 speed_of_sound,
                                 sample_rate,
                                 kPcsRadiusMeters,
                                 kPcsSphereMassKg,
                                 kPcsLowCutoffHz,
                                 kPcsLowQ);

    util::aligned::vector<float> raw(pcs.signal.size());
    std::transform(pcs.signal.begin(),
                   pcs.signal.end(),
                   raw.begin(),
                   [](double sample) { return static_cast<float>(sample); });

    auto transparent = make_transparent(raw.data(), raw.data() + raw.size());

    util::aligned::vector<float> signal;
    signal.assign(transparent.begin(), transparent.end());
    signal.resize(steps, 0.0f);

    const auto calibration = static_cast<float>(
            rectilinear_calibration_factor(grid_spacing, acoustic_impedance));

    for (auto& sample : signal) {
        if (!std::isfinite(sample)) {
            sample = 0.0f;
            continue;
        }
        const auto scaled = sample * calibration;
        sample = std::clamp(scaled,
                            -kTransparentGuardMagnitude,
                            kTransparentGuardMagnitude);
    }

    return signal;
}

template <typename Callback>
std::optional<band> canonical_impl(
        const core::compute_context& cc,
        const mesh& mesh,
        double simulation_time,
        const glm::vec3& source,
        const glm::vec3& receiver,
        const core::environment& environment,
        const std::atomic_bool& keep_going,
        Callback&& callback) {
    const auto sample_rate = compute_sample_rate(mesh.get_descriptor(),
                                                environment.speed_of_sound);

    const auto compute_mesh_index = [&](const auto& pt) {
        const auto ret = compute_index(mesh.get_descriptor(), pt);
        if (!waveguide::is_inside(
                    mesh.get_structure().get_condensed_nodes()[ret])) {
            throw std::runtime_error{
                    "Source/receiver node position appears to be outside "
                    "mesh."};
        }
        return ret;
    };

    const auto ideal_steps = std::ceil(sample_rate * simulation_time);
    const auto total_steps = static_cast<size_t>(ideal_steps);

    auto input = make_pcs_transparent_signal(total_steps,
                                             environment.acoustic_impedance,
                                             environment.speed_of_sound,
                                             sample_rate,
                                             mesh.get_descriptor().spacing);

    auto prep = preprocessor::make_soft_source(
            compute_mesh_index(source), begin(input), end(input));

    auto output_accumulator =
            core::callback_accumulator<postprocessor::directional_receiver>{
                    mesh.get_descriptor(),
                    sample_rate,
                    get_ambient_density(environment),
                    compute_mesh_index(receiver)};

    const auto steps = run(cc,
                           mesh,
                           prep,
                           [&](auto& queue, const auto& buffer, auto step) {
                               output_accumulator(queue, buffer, step);
                               callback(queue, buffer, step, ideal_steps);
                           },
                           keep_going);

    if (steps != total_steps) {
        return std::nullopt;
    }

    return band{std::move(output_accumulator.get_output()), sample_rate};
}

template <typename Callback>
std::optional<band> bempp_canonical_impl(
        const core::compute_context& /*cc*/,
        const mesh& /*mesh*/,
        double /*simulation_time*/,
        const glm::vec3& /*source*/,
        const glm::vec3& /*receiver*/,
        const core::environment& /*environment*/,
        const std::atomic_bool& keep_going,
        Callback&& /*callback*/) {
    if (!keep_going) {
        return std::nullopt;
    }
    static bool warned = false;
    if (!warned) {
        std::cerr << "[waveguide] Bempp backend selected but not yet implemented. "
                     "Falling back to null result.\n";
        warned = true;
    }
    return std::nullopt;
}

}  // namespace detail

////////////////////////////////////////////////////////////////////////////////

/// Run a waveguide using:
///     specified sample rate
///     receiver at specified location
///     source at closest available location
///     single hard source
///     single directional receiver
template <typename PressureCallback>
std::optional<util::aligned::vector<bandpass_band>> canonical(
        const core::compute_context& cc,
        voxels_and_mesh voxelised,
        const glm::vec3& source,
        const glm::vec3& receiver,
        const core::environment& environment,
        const single_band_parameters& sim_params,
        double simulation_time,
        const std::atomic_bool& keep_going,
        PressureCallback&& pressure_callback) {
    const auto backend = select_backend();
    if (backend == waveguide_backend::bempp_cpu) {
        if (auto ret = detail::bempp_canonical_impl(cc,
                                                    voxelised.mesh,
                                                    simulation_time,
                                                    source,
                                                    receiver,
                                                    environment,
                                                    keep_going,
                                                    pressure_callback)) {
            return util::aligned::vector<bandpass_band>{bandpass_band{
                    std::move(*ret), util::make_range(0.0, sim_params.cutoff)}};
        }
        return std::nullopt;
    }

    if (auto ret = detail::canonical_impl(cc,
                                          voxelised.mesh,
                                          simulation_time,
                                          source,
                                          receiver,
                                          environment,
                                          keep_going,
                                          pressure_callback)) {
        return util::aligned::vector<bandpass_band>{bandpass_band{
                std::move(*ret), util::make_range(0.0, sim_params.cutoff)}};
    }

    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////

inline auto set_flat_coefficients_for_band(voxels_and_mesh& voxels_and_mesh,
                                           size_t band) {
    voxels_and_mesh.mesh.set_coefficients(util::map_to_vector(
            begin(voxels_and_mesh.voxels.get_scene_data().get_surfaces()),
            end(voxels_and_mesh.voxels.get_scene_data().get_surfaces()),
            [&](const auto& surface) {
                return to_flat_coefficients(surface.absorption.s[band]);
            }));
}

/// This is a sort of middle ground - more accurate boundary modelling, but
/// really unbelievably slow.
template <typename PressureCallback>
std::optional<util::aligned::vector<bandpass_band>> canonical(
        const core::compute_context& cc,
        voxels_and_mesh voxelised,
        const glm::vec3& source,
        const glm::vec3& receiver,
        const core::environment& environment,
        const multiple_band_constant_spacing_parameters& sim_params,
        double simulation_time,
        const std::atomic_bool& keep_going,
        PressureCallback&& pressure_callback) {
    const auto band_params = hrtf_data::hrtf_band_params_hz();

    if (select_backend() == waveguide_backend::bempp_cpu) {
        // Multi-band Bempp support will come later. For now, we log and bail.
        std::cerr << "[waveguide] Multiple-band Bempp backend not yet implemented.\n";
        (void)voxelised;
        (void)source;
        (void)receiver;
        (void)environment;
        (void)sim_params;
        (void)simulation_time;
        (void)keep_going;
        (void)pressure_callback;
        return std::nullopt;
    }

    util::aligned::vector<bandpass_band> ret{};

    //  For each band, up to the maximum band specified.
    for (auto band = 0; band != sim_params.bands; ++band) {
        set_flat_coefficients_for_band(voxelised, band);

        if (auto rendered_band = detail::canonical_impl(cc,
                                                        voxelised.mesh,
                                                        simulation_time,
                                                        source,
                                                        receiver,
                                                        environment,
                                                        keep_going,
                                                        pressure_callback)) {
            ret.emplace_back(bandpass_band{
                    std::move(*rendered_band),
                    util::make_range(band_params.edges[band],
                                     band_params.edges[band + 1])});
        } else {
            return std::nullopt;
        }
    }

    return ret;
}

}  // namespace waveguide
}  // namespace wayverb
