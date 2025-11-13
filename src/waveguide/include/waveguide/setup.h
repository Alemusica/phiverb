#pragma once

#include "waveguide/boundary_layout.h"
#include "waveguide/cl/utils.h"
#include "waveguide/mesh_descriptor.h"
#include "waveguide/program.h"

#include "core/cl/include.h"
#include "core/program_wrapper.h"
#include "core/spatial_division/scene_buffers.h"
#include "core/spatial_division/voxel_collection.h"

#include "utilities/aligned/vector.h"
#include "utilities/map_to_vector.h"

#include "glm/fwd.hpp"

namespace wayverb {
namespace waveguide {

constexpr bool is_inside(const condensed_node& c) {
    return c.boundary_type & id_inside;
}

////////////////////////////////////////////////////////////////////////////////

class vectors final {
public:
    vectors(util::aligned::vector<condensed_node> nodes,
            util::aligned::vector<coefficients_canonical> coefficients,
            boundary_layout boundary_layout);

    const util::aligned::vector<condensed_node>& get_condensed_nodes() const;
    const util::aligned::vector<coefficients_canonical>& get_coefficients()
            const;
    const boundary_layout& get_boundary_layout() const { return boundary_layout_; }

    void set_coefficients(coefficients_canonical c);
    void set_coefficients(util::aligned::vector<coefficients_canonical> c);

private:
    util::aligned::vector<condensed_node> condensed_nodes_;
    util::aligned::vector<coefficients_canonical> coefficients_;
    boundary_layout boundary_layout_;
};

}  // namespace waveguide
}  // namespace wayverb
