#include "waveguide/setup.h"
#include "waveguide/mesh_setup_program.h"

namespace wayverb {
namespace waveguide {

vectors::vectors(util::aligned::vector<condensed_node> nodes,
                 util::aligned::vector<coefficients_canonical> coefficients,
                 boundary_index_data boundary_index_data)
        : condensed_nodes_(std::move(nodes))
        , coefficients_(std::move(coefficients))
        , boundary_index_data_(std::move(boundary_index_data)) {
#ifndef NDEBUG
    auto throw_if_mismatch = [&](auto checker, auto size) {
        if (count_boundary_type(condensed_nodes_.begin(),
                                condensed_nodes_.end(),
                                checker) != size) {
            throw std::runtime_error(
                    "Number of nodes does not match number of boundaries.");
        }
    };

    throw_if_mismatch(is_boundary<1>, boundary_index_data_.b1.size());
    throw_if_mismatch(is_boundary<2>, boundary_index_data_.b2.size());
    throw_if_mismatch(is_boundary<3>, boundary_index_data_.b3.size());
#endif

    boundary_nodes_1_.resize(boundary_index_data_.b1.size());
    boundary_nodes_2_.resize(boundary_index_data_.b2.size());
    boundary_nodes_3_.resize(boundary_index_data_.b3.size());

    for (cl_uint idx = 0; idx < condensed_nodes_.size(); ++idx) {
        const auto& node = condensed_nodes_[idx];
        if (is_boundary<1>(node.boundary_type)) {
            boundary_nodes_1_[node.boundary_index] = idx;
        }
        if (is_boundary<2>(node.boundary_type)) {
            boundary_nodes_2_[node.boundary_index] = idx;
        }
        if (is_boundary<3>(node.boundary_type)) {
            boundary_nodes_3_[node.boundary_index] = idx;
        }
    }
}

const util::aligned::vector<condensed_node>& vectors::get_condensed_nodes()
        const {
    return condensed_nodes_;
}

const util::aligned::vector<coefficients_canonical>& vectors::get_coefficients()
        const {
    return coefficients_;
}

void vectors::set_coefficients(coefficients_canonical c) {
    std::fill(begin(coefficients_), end(coefficients_), c);
}

void vectors::set_coefficients(
        util::aligned::vector<coefficients_canonical> c) {
    if (c.size() != coefficients_.size()) {
        throw std::runtime_error(
                "Size of new coefficients vector must be equal to the existing "
                "one in order to maintain object invariants.");
    }
    coefficients_ = std::move(c);
}

}  // namespace waveguide
}  // namespace wayverb
