#include "waveguide/cl/structs.h"
#include "waveguide/cl/utils.h"

#include "utilities/aligned/vector.h"

#include "gtest/gtest.h"

namespace wayverb {
namespace waveguide {
namespace {

template <int Dim>
std::vector<cl_uint> collect_boundary_nodes(
        const util::aligned::vector<condensed_node>& nodes) {
    std::vector<cl_uint> indices;
    for (cl_uint idx = 0; idx < nodes.size(); ++idx) {
        const auto boundary_type = nodes[idx].boundary_type;
        if (boundary_type & id_reentrant) {
            continue;
        }
        const auto mask = boundary_type & (id_nx | id_px | id_ny | id_py | id_nz | id_pz);
        if (__builtin_popcount(mask) == Dim) {
            indices.push_back(idx);
        }
    }
    return indices;
}

}  // namespace

TEST(BoundaryNodeIndices, MapsOneToOnePerDimension) {
    util::aligned::vector<condensed_node> nodes{
            condensed_node{id_inside, 0},              // ignored
            condensed_node{id_nx, 0},                  // 1D
            condensed_node{id_px, 1},                  // 1D
            condensed_node{id_nx | id_ny, 0},          // 2D
            condensed_node{id_nx | id_ny | id_nz, 0},  // 3D
    };

    const auto nodes1 = collect_boundary_nodes<1>(nodes);
    ASSERT_EQ(nodes1.size(), 2u);
    EXPECT_EQ(nodes1[0], 1u);
    EXPECT_EQ(nodes1[1], 2u);

    const auto nodes2 = collect_boundary_nodes<2>(nodes);
    ASSERT_EQ(nodes2.size(), 1u);
    EXPECT_EQ(nodes2[0], 3u);

    const auto nodes3 = collect_boundary_nodes<3>(nodes);
    ASSERT_EQ(nodes3.size(), 1u);
    EXPECT_EQ(nodes3[0], 4u);
}

TEST(BoundaryNodeIndices, SkipsReentrantNodes) {
    util::aligned::vector<condensed_node> nodes{
            condensed_node{id_reentrant, 0},
            condensed_node{id_nx, 0},
            condensed_node{id_nx, 1},
    };

    const auto nodes1 = collect_boundary_nodes<1>(nodes);
    ASSERT_EQ(nodes1.size(), 2u);
    EXPECT_EQ(nodes1[0], 1u);
    EXPECT_EQ(nodes1[1], 2u);
}

}  // namespace waveguide
}  // namespace wayverb
