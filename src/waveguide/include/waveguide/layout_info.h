#pragma once

#include <cstdint>

namespace wayverb {
namespace waveguide {

/// Describes the byte sizes and critical offsets for core waveguide data
/// structures. Used to guarantee Metal kernels match the OpenCL/host layout.
struct layout_info final {
    std::uint32_t sz_memory_canonical{};
    std::uint32_t sz_coefficients_canonical{};
    std::uint32_t sz_boundary_data{};
    std::uint32_t sz_boundary_data_array_3{};

    std::uint32_t off_bd_filter_memory{};
    std::uint32_t off_bd_coefficient_index{};

    std::uint32_t off_b3_data0{};
    std::uint32_t off_b3_data1{};
    std::uint32_t off_b3_data2{};
};

layout_info make_host_layout_info() noexcept;

}  // namespace waveguide
}  // namespace wayverb
