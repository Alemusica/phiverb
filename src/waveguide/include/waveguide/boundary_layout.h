#pragma once

#include "waveguide/boundary_coefficient_finder.h"
#include "waveguide/cl/structs.h"
#include "waveguide/cl/utils.h"
#include "waveguide/mesh_descriptor.h"

#include "core/spatial_division/voxelised_scene_data.h"

#include "utilities/aligned/vector.h"

#include "glm/glm.hpp"

#include <cstdint>

namespace wayverb {
namespace waveguide {

struct BoundaryHeader final {
    uint32_t guard{};
    uint16_t dif{};
    uint16_t material_index{};
};

struct boundary_layout final {
    util::aligned::vector<BoundaryHeader> headers;
    util::aligned::vector<float> sdf_distance;
    util::aligned::vector<glm::vec3> sdf_normal;
    util::aligned::vector<uint32_t> coeff_block_offsets;
    util::aligned::vector<coefficients_canonical> coeff_blocks;
    util::aligned::vector<memory_canonical> filter_memories;
    util::aligned::vector<uint32_t> node_indices;
    util::aligned::vector<uint32_t> node_lookup;
};

boundary_layout build_boundary_layout(
        const mesh_descriptor& descriptor,
        const util::aligned::vector<condensed_node>& nodes,
        const boundary_index_data& index_data,
        const util::aligned::vector<coefficients_canonical>& surface_coeffs,
        const core::voxelised_scene_data<cl_float3,
                                         core::surface<core::simulation_bands>>&
                voxelised);

}  // namespace waveguide
}  // namespace wayverb
