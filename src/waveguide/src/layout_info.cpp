#include "waveguide/layout_info.h"

#include "waveguide/cl/filter_structs.h"
#include "waveguide/cl/structs.h"

#include <cstddef>

namespace wayverb {
namespace waveguide {

layout_info make_host_layout_info() noexcept {
    layout_info info{};
    info.sz_memory_canonical = static_cast<std::uint32_t>(sizeof(memory_canonical));
    info.sz_coefficients_canonical =
            static_cast<std::uint32_t>(sizeof(coefficients_canonical));
    info.sz_boundary_data = static_cast<std::uint32_t>(sizeof(boundary_data));
    info.sz_boundary_data_array_3 =
            static_cast<std::uint32_t>(sizeof(boundary_data_array_3));

    info.off_bd_filter_memory = static_cast<std::uint32_t>(
            offsetof(boundary_data, filter_memory));
    info.off_bd_coefficient_index = static_cast<std::uint32_t>(
            offsetof(boundary_data, coefficient_index));

    info.off_b3_data0 = static_cast<std::uint32_t>(
            offsetof(boundary_data_array_3, array[0]));
    info.off_b3_data1 = static_cast<std::uint32_t>(
            offsetof(boundary_data_array_3, array[1]));
    info.off_b3_data2 = static_cast<std::uint32_t>(
            offsetof(boundary_data_array_3, array[2]));

    return info;
}

}  // namespace waveverb
}  // namespace waveguide

