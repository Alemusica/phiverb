#pragma once

#include <cstdint>

namespace wayverb {
namespace waveguide {

inline std::uint32_t make_boundary_guard_tag(std::uint32_t node_index) {
    return node_index ^ 0xA5A5A5A5u;
}

}  // namespace waveguide
}  // namespace wayverb
