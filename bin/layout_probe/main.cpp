#include "waveguide/cl/filter_structs.h"
#include "waveguide/program.h"

#include "core/cl/common.h"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <tuple>

namespace {

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

layout_info compute_host_layout() {
    using namespace wayverb::waveguide;
    layout_info info{};
    info.sz_memory_canonical = static_cast<std::uint32_t>(sizeof(memory_canonical));
    info.sz_coefficients_canonical =
            static_cast<std::uint32_t>(sizeof(coefficients_canonical));
    info.sz_boundary_data = static_cast<std::uint32_t>(sizeof(boundary_data));
    info.sz_boundary_data_array_3 =
            static_cast<std::uint32_t>(sizeof(boundary_data_array_3));
    info.off_bd_filter_memory =
            static_cast<std::uint32_t>(offsetof(boundary_data, filter_memory));
    info.off_bd_coefficient_index =
            static_cast<std::uint32_t>(offsetof(boundary_data, coefficient_index));
    info.off_b3_data0 =
            static_cast<std::uint32_t>(offsetof(boundary_data_array_3, array[0]));
    info.off_b3_data1 =
            static_cast<std::uint32_t>(offsetof(boundary_data_array_3, array[1]));
    info.off_b3_data2 =
            static_cast<std::uint32_t>(offsetof(boundary_data_array_3, array[2]));
    return info;
}

void print_layout(const char* label, const layout_info& info) {
    std::cout << label << ":\n"
              << "  sz_memory_canonical       = " << info.sz_memory_canonical << '\n'
              << "  sz_coefficients_canonical = " << info.sz_coefficients_canonical << '\n'
              << "  sz_boundary_data          = " << info.sz_boundary_data << '\n'
              << "  sz_boundary_data_array_3  = " << info.sz_boundary_data_array_3 << '\n'
              << "  off_bd_filter_memory      = " << info.off_bd_filter_memory << '\n'
              << "  off_bd_coefficient_index  = " << info.off_bd_coefficient_index << '\n'
              << "  off_b3_data0              = " << info.off_b3_data0 << '\n'
              << "  off_b3_data1              = " << info.off_b3_data1 << '\n'
              << "  off_b3_data2              = " << info.off_b3_data2 << '\n';
}

}  // namespace

int main() {
    try {
        wayverb::core::compute_context cc;
        wayverb::waveguide::program prog{cc};
        auto kernel = prog.get_layout_probe_kernel();
        cl::CommandQueue queue{cc.context, cc.device};

        cl::Buffer device_out{
                cc.context, CL_MEM_WRITE_ONLY, sizeof(layout_info)};

        kernel(cl::EnqueueArgs{queue, cl::NDRange{1}}, device_out);
        queue.finish();

        layout_info device_info{};
        queue.enqueueReadBuffer(
                device_out, CL_TRUE, 0, sizeof(layout_info), &device_info);

        const auto host_info = compute_host_layout();

        print_layout("Host", host_info);
        print_layout("Device", device_info);

        const auto report = [&](const char* name,
                                std::uint32_t layout_info::*member) {
            const auto host_value = host_info.*member;
            const auto device_value = device_info.*member;
            if (host_value != device_value) {
                std::cout << "  mismatch " << name << ": host=" << host_value
                          << " device=" << device_value << '\n';
                return false;
            }
            return true;
        };

        bool ok = true;
        ok &= report("sz_memory_canonical", &layout_info::sz_memory_canonical);
        ok &= report("sz_coefficients_canonical",
                     &layout_info::sz_coefficients_canonical);
        ok &= report("sz_boundary_data", &layout_info::sz_boundary_data);
        ok &= report("sz_boundary_data_array_3",
                     &layout_info::sz_boundary_data_array_3);
        ok &= report("off_bd_filter_memory",
                     &layout_info::off_bd_filter_memory);
        ok &= report("off_bd_coefficient_index",
                     &layout_info::off_bd_coefficient_index);
        ok &= report("off_b3_data0", &layout_info::off_b3_data0);
        ok &= report("off_b3_data1", &layout_info::off_b3_data1);
        ok &= report("off_b3_data2", &layout_info::off_b3_data2);

        if (!ok) {
            std::cout << "\nLayout mismatch detected.\n";
            return EXIT_FAILURE;
        }

        std::cout << "\nLayout matches between host and device.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "layout_probe error: " << e.what() << '\n';
    }
    return EXIT_FAILURE;
}
