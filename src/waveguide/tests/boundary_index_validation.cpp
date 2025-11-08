#include "waveguide/setup.h"

#include "gtest/gtest.h"

namespace wayverb {
namespace waveguide {
namespace {

coefficients_canonical make_coeff(float b0 = 1.0f, float a0 = 1.0f) {
    coefficients_canonical coeff{};
    coeff.b[0] = static_cast<filt_real>(b0);
    coeff.a[0] = static_cast<filt_real>(a0);
    return coeff;
}

struct test_vectors_builder {
    util::aligned::vector<condensed_node> nodes;
    util::aligned::vector<coefficients_canonical> coeffs;
    boundary_index_data boundary_data;

    test_vectors_builder& add_node(cl_int boundary_type,
                                   cl_uint boundary_index) {
        nodes.push_back(condensed_node{boundary_type, boundary_index});
        return *this;
    }

    test_vectors_builder& set_coeff_count(size_t count) {
        coeffs.assign(count, make_coeff());
        return *this;
    }

    test_vectors_builder& add_boundary_1(cl_uint c0) {
        boundary_data.b1.push_back(boundary_index_array_1{{c0}});
        return *this;
    }

    test_vectors_builder& add_boundary_2(cl_uint c0, cl_uint c1) {
        boundary_data.b2.push_back(boundary_index_array_2{{c0, c1}});
        return *this;
    }

    test_vectors_builder& add_boundary_3(cl_uint c0, cl_uint c1, cl_uint c2) {
        boundary_data.b3.push_back(boundary_index_array_3{{c0, c1, c2}});
        return *this;
    }

    vectors build() {
        return vectors{std::move(nodes), std::move(coeffs), std::move(boundary_data)};
    }
};

}  // namespace

TEST(BoundaryNodeIndices, MapsOneToOnePerDimension) {
    test_vectors_builder builder;
    builder.set_coeff_count(4)
            .add_node(id_inside, 0)              // index 0 - ignored
            .add_node(id_nx, 0)                  // index 1
            .add_node(id_px, 1)                  // index 2
            .add_node(id_nx | id_ny, 0)          // index 3
            .add_node(id_nx | id_ny | id_nz, 0); // index 4

    builder.add_boundary_1(0).add_boundary_1(1);
    builder.add_boundary_2(0, 1);
    builder.add_boundary_3(0, 1, 2);

    const auto vecs = builder.build();

    const auto& nodes1 = vecs.get_boundary_node_indices<1>();
    ASSERT_EQ(nodes1.size(), 2u);
    EXPECT_EQ(nodes1[0], 1u);
    EXPECT_EQ(nodes1[1], 2u);

    const auto& nodes2 = vecs.get_boundary_node_indices<2>();
    ASSERT_EQ(nodes2.size(), 1u);
    EXPECT_EQ(nodes2[0], 3u);

    const auto& nodes3 = vecs.get_boundary_node_indices<3>();
    ASSERT_EQ(nodes3.size(), 1u);
    EXPECT_EQ(nodes3[0], 4u);
}

TEST(BoundaryNodeIndices, SkipsReentrantNodes) {
    test_vectors_builder builder;
    builder.set_coeff_count(3)
            .add_node(id_reentrant, 0)  // should be ignored entirely
            .add_node(id_nx, 0)         // valid boundary
            .add_node(id_nx, 1);        // second boundary entry

    builder.add_boundary_1(0).add_boundary_1(1);

    const auto vecs = builder.build();
    const auto& nodes1 = vecs.get_boundary_node_indices<1>();
    ASSERT_EQ(nodes1.size(), 2u);
    EXPECT_EQ(nodes1[0], 1u);
    EXPECT_EQ(nodes1[1], 2u);
}

}  // namespace waveguide
}  // namespace wayverb
