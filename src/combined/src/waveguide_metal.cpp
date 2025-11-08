#include "combined/waveguide_base.h"

#if defined(WAYVERB_ENABLE_METAL) && defined(__APPLE__)

#include "metal/metal_context.h"
#include "metal/waveguide_pipeline.h"
#include "metal/waveguide_simulation.h"

#include "waveguide/bandpass_band.h"
#include "waveguide/calibration.h"
#include "waveguide/canonical.h" // fallback for now
#include "waveguide/postprocessor/directional_receiver.h"

#include "utilities/crash_reporter.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <numeric>

namespace wayverb {
namespace combined {

namespace {

struct directional_receiver_metal final {
    directional_receiver_metal(const waveguide::mesh_descriptor& descriptor,
                               double sample_rate,
                               double ambient_density,
                               size_t output_node)
            : mesh_spacing_{descriptor.spacing}
            , sample_rate_{sample_rate}
            , ambient_density_{ambient_density}
            , output_node_{output_node}
            , surrounding_nodes_{waveguide::compute_neighbors(descriptor, output_node)} {
        for (const auto& idx : surrounding_nodes_) {
            if (idx == waveguide::mesh_descriptor::no_neighbor) {
                throw std::runtime_error(
                        "Directional receiver adjacent to boundary; Metal backend requires interior node.");
            }
        }
    }

    waveguide::postprocessor::directional_receiver::output operator()(const float* buffer) {
        const float pressure = buffer[output_node_];
        std::array<float, 6> surrounding{};
        for (size_t i = 0; i < surrounding.size(); ++i) {
            const auto idx = surrounding_nodes_[i];
            surrounding[i] = (buffer[idx] - pressure) / mesh_spacing_;
        }

        const glm::dvec3 gradient{(surrounding[1] - surrounding[0]) * 0.5,
                                   (surrounding[3] - surrounding[2]) * 0.5,
                                   (surrounding[5] - surrounding[4]) * 0.5};

        velocity_ -= gradient / (ambient_density_ * sample_rate_);
        const glm::dvec3 intensity = velocity_ * static_cast<double>(pressure);
        return {intensity, pressure};
    }

private:
    double mesh_spacing_{}
    ;
    double sample_rate_{};
    double ambient_density_{};
    size_t output_node_{};
    std::array<unsigned, 6> surrounding_nodes_{};
    glm::dvec3 velocity_{0.0};
};

template <typename Sim>
class metal_waveguide final : public waveguide_base {
public:
    explicit metal_waveguide(Sim sim) : sim_{std::move(sim)} {}

    std::unique_ptr<waveguide_base> clone() const override {
        return std::make_unique<metal_waveguide>(*this);
    }

    double compute_sampling_frequency() const override {
        return waveguide::compute_sampling_frequency(sim_);
    }

    std::optional<util::aligned::vector<waveguide::bandpass_band>> run(
            const core::compute_context& cc,
            const waveguide::voxels_and_mesh& voxelised,
            const glm::vec3& source,
            const glm::vec3& receiver,
            const core::environment& env,
            double simulation_time,
            const std::atomic_bool& keep,
            std::function<void(cl::CommandQueue&, const cl::Buffer&, size_t, size_t)> pressure_cb) override {
        const char* force = std::getenv("WAYVERB_METAL");
        if (force && std::string(force) == std::string("force-opencl")) {
            return waveguide::canonical(cc, voxelised, source, receiver, env, sim_, simulation_time, keep, std::move(pressure_cb));
        }

        metal::context mctx;
        if (!mctx.valid()) {
            std::cerr << "[metal] No MTLDevice available; falling back to OpenCL\n";
            return waveguide::canonical(cc, voxelised, source, receiver, env, sim_, simulation_time, keep, std::move(pressure_cb));
        }

        metal::waveguide_pipeline pipeline{mctx};
        metal::waveguide_simulation simulation{mctx, pipeline, voxelised.mesh};
        if (!simulation.valid()) {
            std::cerr << "[metal] Waveguide simulation setup failed; falling back to OpenCL\n";
            return waveguide::canonical(cc, voxelised, source, receiver, env, sim_, simulation_time, keep, std::move(pressure_cb));
        }

        const auto sample_rate = waveguide::compute_sample_rate(
                simulation.descriptor(), env.speed_of_sound);

        const auto compute_mesh_index = [&](const glm::vec3& pt) -> size_t {
            const auto idx = waveguide::compute_index(simulation.descriptor(), pt);
            if (!waveguide::is_inside(
                        voxelised.mesh.get_structure().get_condensed_nodes()[idx])) {
                throw std::runtime_error{
                        "Source/receiver node position appears to be outside mesh."};
            }
            return idx;
        };

        const auto source_index = compute_mesh_index(source);
        const auto receiver_index = compute_mesh_index(receiver);

        const auto ideal_steps = static_cast<size_t>(
                std::ceil(sample_rate * simulation_time));
        if (ideal_steps == 0) {
            return util::aligned::vector<waveguide::bandpass_band>{};
        }

        std::vector<float> input(ideal_steps, 0.0f);
        if (!input.empty()) {
            input.front() = rectilinear_calibration_factor(
                    simulation.descriptor().spacing,
                    env.acoustic_impedance);
        }

        const double ambient_density = get_ambient_density(env);
        directional_receiver_metal receiver_state{
                simulation.descriptor(), sample_rate, ambient_density, receiver_index};

        util::aligned::vector<waveguide::postprocessor::directional_receiver::output> outputs;
        outputs.reserve(ideal_steps);

        std::cerr << "[metal] starting waveguide simulation (Metal backend)\n";

        const size_t completed = simulation.run(
                input,
                source_index,
                ideal_steps,
                keep,
                [&](const float* current_buffer, size_t step, size_t total) {
                    (void)total;
                    outputs.emplace_back(receiver_state(current_buffer));
                },
                [&](size_t step_completed, size_t total_steps) {
                    if (total_steps == 0) return;
                    if ((step_completed % 500) == 0 || step_completed == total_steps) {
                        const double prog = total_steps > 1
                                ? static_cast<double>(step_completed) /
                                          static_cast<double>(total_steps)
                                : 1.0;
                        std::cerr << "[metal] waveguide step=" << step_completed
                                  << "/" << total_steps << " ("
                                  << static_cast<int>(prog * 100.0) << "%)\n";
                        std::ostringstream os;
                        os << "wg step=" << step_completed << "/" << total_steps;
                        util::crash::reporter::set_status(os.str());
                    }
                });

        if (!keep || completed != ideal_steps || outputs.size() != ideal_steps) {
            std::cerr << "[metal] simulation did not complete (steps=" << completed
                      << "/" << ideal_steps << "); falling back to OpenCL\n";
            return waveguide::canonical(cc, voxelised, source, receiver, env, sim_, simulation_time, keep, std::move(pressure_cb));
        }

        waveguide::band band_value{
                util::aligned::vector<
                        waveguide::postprocessor::directional_receiver::output>{
                        outputs.begin(), outputs.end()},
                sample_rate};

        util::aligned::vector<waveguide::bandpass_band> result;
        result.emplace_back(waveguide::bandpass_band{
                std::move(band_value),
                util::make_range(0.0, sim_.cutoff)});

        return result;

        (void)pressure_cb; // progress callbacks unsupported on Metal path for now.
    }

private:
    Sim sim_;
};

} // namespace

std::unique_ptr<waveguide_base> make_metal_waveguide_ptr(
        const waveguide::single_band_parameters& t) {
    return std::make_unique<metal_waveguide<waveguide::single_band_parameters>>(t);
}

std::unique_ptr<waveguide_base> make_metal_waveguide_ptr(
        const waveguide::multiple_band_constant_spacing_parameters& t) {
    return std::make_unique<metal_waveguide<
            waveguide::multiple_band_constant_spacing_parameters>>(t);
}

} // namespace combined
} // namespace wayverb

#endif // WAYVERB_ENABLE_METAL && __APPLE__
