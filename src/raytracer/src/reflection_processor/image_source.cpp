#include "raytracer/reflection_processor/image_source.h"
#include "raytracer/image_source/fast_pressure_calculator.h"
#include "raytracer/image_source/get_direct.h"

#include "core/pressure_intensity.h"

#include <iterator>

namespace wayverb {
namespace raytracer {
namespace reflection_processor {

image_source_group_processor::image_source_group_processor(size_t max_order,
                                                           size_t items)
        : max_image_source_order_{max_order}
        , builder_{items} {}

////////////////////////////////////////////////////////////////////////////////

image_source_processor::image_source_processor(
        const glm::vec3& source,
        const glm::vec3& receiver,
        const core::environment& environment,
        const core::voxelised_scene_data<cl_float3,
                                         core::surface<core::simulation_bands>>&
                voxelised,
        size_t max_order,
        size_t total_rays,
        float mis_delta_pdf)
        : source_{source}
        , receiver_{receiver}
        , environment_{environment}
        , voxelised_{voxelised}
        , max_order_{max_order}
        , total_rays_{total_rays} {
    const auto weights = compute_mis_weights(total_rays_, mis_delta_pdf);
    mis_image_source_weight_ = weights.image_source;
    mis_enabled_ = total_rays_ != 0;
}

image_source_group_processor image_source_processor::get_group_processor(
        size_t num_directions) const {
    return {max_order_, num_directions};
}

void image_source_processor::accumulate(
        const image_source_group_processor& processor) {
    for (const auto& path : processor.get_results()) {
        tree_.push(path);
    }
}

util::aligned::vector<impulse<8>> image_source_processor::get_results() const {
    util::aligned::vector<impulse<8>> ret;

    const auto calculator = image_source::make_fast_pressure_calculator(
            begin(voxelised_.get_scene_data().get_surfaces()),
            end(voxelised_.get_scene_data().get_surfaces()),
            receiver_,
            false);

    const auto callback = [&](const auto& image_source_position,
                              const auto begin,
                              const auto end) {
        auto impulse = calculator(image_source_position, begin, end);
        const auto order = static_cast<size_t>(std::distance(begin, end));
        impulse.volume *= mis_weight_for_order(order);
        ret.emplace_back(impulse);
    };

    for (const auto& branch : tree_.get_branches()) {
        image_source::find_valid_paths(
                branch, source_, receiver_, voxelised_, callback);
    }

    //  Add the line-of-sight contribution, which isn't directly detected by
    //  the image-source machinery.
    using namespace image_source;
    if (const auto direct = get_direct(source_, receiver_, voxelised_)) {
        auto weighted = *direct;
        weighted.volume *= mis_weight_for_order(0);
        ret.emplace_back(weighted);
    }

    //  Correct for distance travelled.
    for (auto& imp : ret) {
        imp.volume *= core::pressure_for_distance(
                imp.distance, environment_.acoustic_impedance);
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////

make_image_source::make_image_source(size_t max_order,
                                     size_t total_rays,
                                     float mis_delta_pdf)
        : max_order_{max_order}
        , total_rays_{total_rays}
        , mis_delta_pdf_{mis_delta_pdf} {}

image_source_processor make_image_source::get_processor(
        const core::compute_context& /*cc*/,
        const glm::vec3& source,
        const glm::vec3& receiver,
        const core::environment& environment,
        const core::voxelised_scene_data<cl_float3,
                                         core::surface<core::simulation_bands>>&
                voxelised) const {
    return {source,
            receiver,
            environment,
            voxelised,
            max_order_,
            total_rays_,
            mis_delta_pdf_};
}

float image_source_processor::mis_weight_for_order(size_t order) const {
    if (!mis_enabled_) {
        return 1.0f;
    }
    if (order <= max_order_) {
        return mis_image_source_weight_;
    }
    return 1.0f;
}

}  // namespace reflection_processor
}  // namespace raytracer
}  // namespace wayverb
