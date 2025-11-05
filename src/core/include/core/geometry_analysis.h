#pragma once

#include "core/scene_data.h"

#include <cstddef>

namespace wayverb {
namespace core {

struct geometry_report final {
    std::size_t vertices{};
    std::size_t triangles{};
    std::size_t zero_area{};
    std::size_t duplicate_vertices{};
    std::size_t boundary_edges{};      // edges used by exactly one triangle
    std::size_t non_manifold_edges{};  // edges used by > 2 triangles
    bool watertight{};
};

// Analyze a generic_scene_data (Vertex must be cl_float3-like).
template <typename Vertex, typename Surface>
geometry_report analyze_geometry(
        const generic_scene_data<Vertex, Surface>& scene,
        float weld_epsilon = 1.0e-6f);

// Produce a sanitized copy: weld duplicate vertices (within epsilon) and
// remove zero-area/degenerate triangles. Surfaces are preserved.
template <typename Vertex, typename Surface>
generic_scene_data<Vertex, Surface> sanitize_geometry(
        const generic_scene_data<Vertex, Surface>& scene,
        float weld_epsilon = 1.0e-6f);

}  // namespace core
}  // namespace wayverb

