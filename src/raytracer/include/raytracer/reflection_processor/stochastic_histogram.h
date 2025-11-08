#pragma once

#include "raytracer/histogram.h"
#include "raytracer/simulation_parameters.h"
#include "raytracer/stochastic/finder.h"
#include "raytracer/stochastic/postprocessing.h"
#include "raytracer/reflection_processor/mis_weights.h"

#include "core/attenuator/hrtf.h"
#include "core/environment.h"
#include "core/spatial_division/scene_buffers.h"

namespace wayverb {

namespace raytracer {
namespace reflection_processor {

template <typename T, typename U, typename Alloc>
void energy_histogram_sum(const T& item,
                          double sample_rate,
                          std::vector<U, Alloc>& ret) {
    ret[time(item) * sample_rate] += volume(item);
}

template <typename T, typename U, typename Alloc, size_t Az, size_t El>
void energy_histogram_sum(
        const T& item,
        double sample_rate,
        core::vector_look_up_table<std::vector<U, Alloc>, Az, El>& ret) {
    using table = std::decay_t<decltype(ret)>;
    ret.at(table::index(item.pointing))[time(item) * sample_rate] +=
            volume(item);
}

struct energy_histogram_sum_functor final {
    template <typename T, typename Ret>
    void operator()(const T& item, double sample_rate, Ret& ret) const {
        energy_histogram_sum(item, sample_rate, ret);
    }
};

/// Where Histogram is probably a stochastic::energy_histogram or a
/// stochastic::directional_energy_histogram.
template <typename Histogram>
class stochastic_group_processor final {
public:
    /// A max_image_source_order of 0 = direct energy from image-source
    /// An order of 1 = direct and one reflection from image-source
    /// i.e. the order == the number of reflections for each image
    stochastic_group_processor(const core::compute_context& cc,
                               const glm::vec3& source,
                               const glm::vec3& receiver,
                               const core::environment& environment,
                               size_t total_rays,
                               size_t max_image_source_order,
                               float receiver_radius,
                                float histogram_sample_rate,
                                size_t group_items,
                                float mis_delta_pdf)
            : finder_(cc,
                      group_items,
                      source,
                      receiver,
                      receiver_radius,
                      stochastic::compute_ray_energy(
                              total_rays, source, receiver, receiver_radius))
            , receiver_{receiver}
            , environment_{environment}
            , max_image_source_order_{max_image_source_order}
            , mis_weights_{compute_mis_weights(total_rays, mis_delta_pdf)}
            , mis_enabled_{total_rays != 0}
            , histogram_{histogram_sample_rate} {}

    template <typename It>
    void process(It b,
                 It e,
                 const core::scene_buffers& buffers,
                 size_t step,
                 size_t /*total*/) {
        const auto output = finder_.process(b, e, buffers);

        struct intermediate_impulse final {
            core::bands_type volume;
            double time;
            glm::vec3 pointing;
        };

        const auto intermediate = [&] {
            util::aligned::vector<intermediate_impulse> ret;
            ret.reserve(output.stochastic.size() + output.specular.size());

            const auto push_vector = [&](const auto& vec, float weight = 1.0f) {
                for (const auto& impulse : vec) {
                    ret.emplace_back(intermediate_impulse{
                            impulse.volume * weight,
                            impulse.distance / environment_.speed_of_sound,
                            glm::normalize(core::to_vec3{}(impulse.position) -
                                           receiver_)});
                }
            };

            push_vector(output.stochastic);
            const auto spec_weight = specular_weight(step);
            if (spec_weight > 0.0f) {
                push_vector(output.specular, spec_weight);
            }

            return ret;
        }();

        incremental_histogram(histogram_.histogram,
                              begin(intermediate),
                              end(intermediate),
                              histogram_.sample_rate,
                              energy_histogram_sum_functor{});
    }

    Histogram get_results() const { return histogram_; }

private:
    stochastic::finder finder_;
    glm::vec3 receiver_;
    core::environment environment_;
    size_t max_image_source_order_;
    mis_weights mis_weights_;
    bool mis_enabled_;
    Histogram histogram_;

    float specular_weight(size_t step) const {
        if (max_image_source_order_ <= step) {
            return 1.0f;
        }
        if (!mis_enabled_) {
            return 0.0f;
        }
        return mis_weights_.path_tracer;
    }
};

////////////////////////////////////////////////////////////////////////////////

template <typename Histogram>
class stochastic_processor final {
public:
    stochastic_processor(const core::compute_context& cc,
                         const glm::vec3& source,
                         const glm::vec3& receiver,
                         const core::environment& environment,
                         size_t total_rays,
                         size_t max_image_source_order,
                         float receiver_radius,
                         float histogram_sample_rate,
                         float mis_delta_pdf)
            : cc_{cc}
            , source_{source}
            , receiver_{receiver}
            , environment_{environment}
            , total_rays_{total_rays}
            , max_image_source_order_{max_image_source_order}
            , receiver_radius_{receiver_radius}
            , histogram_sample_rate_{histogram_sample_rate}
            , mis_delta_pdf_{mis_delta_pdf}
            , histogram_{histogram_sample_rate} {}

    stochastic_group_processor<Histogram> get_group_processor(
            size_t num_directions) const {
        return {cc_,
                source_,
                receiver_,
                environment_,
                total_rays_,
                max_image_source_order_,
                receiver_radius_,
                histogram_sample_rate_,
                num_directions,
                mis_delta_pdf_};
    }

    void accumulate(const stochastic_group_processor<Histogram>& processor) {
        sum_histograms(histogram_, processor.get_results());
    }

    Histogram get_results() const { return histogram_; }

private:
    core::compute_context cc_;
    glm::vec3 source_;
    glm::vec3 receiver_;
    core::environment environment_;
    size_t total_rays_;
    size_t max_image_source_order_;
    float receiver_radius_;
    float histogram_sample_rate_;
    float mis_delta_pdf_;

    Histogram histogram_;
};

////////////////////////////////////////////////////////////////////////////////

class make_stochastic_histogram final {
public:
    make_stochastic_histogram(size_t total_rays,
                              size_t max_image_source_order,
                              float receiver_radius,
                              float histogram_sample_rate,
                              float mis_delta_pdf = default_mis_delta_pdf);

    stochastic_processor<stochastic::energy_histogram> get_processor(
            const core::compute_context& cc,
            const glm::vec3& source,
            const glm::vec3& receiver,
            const core::environment& environment,
            const core::voxelised_scene_data<
                    cl_float3,
                    core::surface<core::simulation_bands>>& voxelised) const;

private:
    size_t total_rays_;
    size_t max_image_source_order_;
    float receiver_radius_;
    float histogram_sample_rate_;
    float mis_delta_pdf_;
};

class make_directional_histogram final {
public:
    make_directional_histogram(size_t total_rays,
                               size_t max_image_source_order,
                               float receiver_radius,
                               float histogram_sample_rate,
                               float mis_delta_pdf = default_mis_delta_pdf);

    stochastic_processor<stochastic::directional_energy_histogram<20, 9>>
    get_processor(
            const core::compute_context& cc,
            const glm::vec3& source,
            const glm::vec3& receiver,
            const core::environment& environment,
            const core::voxelised_scene_data<
                    cl_float3,
                    core::surface<core::simulation_bands>>& voxelised) const;

private:
    size_t total_rays_;
    size_t max_image_source_order_;
    float receiver_radius_;
    float histogram_sample_rate_;
    float mis_delta_pdf_;
};

}  // namespace reflection_processor
}  // namespace raytracer
}  // namespace wayverb
