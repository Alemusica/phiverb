#pragma once

#include "raytracer/image_source/tree.h"

namespace wayverb {
namespace raytracer {
namespace image_source {

/// Lightweight view over a valid image-source path.
struct path_event_view final {
    glm::vec3 image_source;
    util::aligned::vector<reflection_metadata>::const_iterator begin;
    util::aligned::vector<reflection_metadata>::const_iterator end;
};

template <typename Callback>
void enumerate_valid_paths(
        const multitree<path_element>& tree,
        const glm::vec3& source,
        const glm::vec3& receiver,
        const core::voxelised_scene_data<cl_float3,
                                         core::surface<core::simulation_bands>>&
                voxelised,
        Callback&& callback) {
    find_valid_paths(
            tree,
            source,
            receiver,
            voxelised,
            [&](const glm::vec3& image_source,
                util::aligned::vector<reflection_metadata>::const_iterator begin,
                util::aligned::vector<reflection_metadata>::const_iterator end) {
                callback(path_event_view{image_source, begin, end});
            });
}

}  // namespace image_source
}  // namespace raytracer
}  // namespace wayverb
