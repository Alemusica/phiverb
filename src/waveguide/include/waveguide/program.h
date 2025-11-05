#pragma once

#include "waveguide/cl/structs.h"

#include "core/cl/common.h"
#include "core/program_wrapper.h"
#include "core/scene_data.h"

#include "utilities/decibels.h"
#include "utilities/string_builder.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace wayverb {
namespace waveguide {

class program final {
public:
    program(const core::compute_context& cc);

    auto get_kernel() const {
        return program_wrapper_
                .get_kernel<cl::Buffer,  /// previous
                            cl::Buffer,  /// current
                            cl::Buffer,  /// nodes
                            cl_int3,     /// dimensions
                            cl::Buffer,  /// boundary_data_1
                            cl::Buffer,  /// boundary_data_2
                            cl::Buffer,  /// boundary_data_3
                            cl::Buffer,  /// boundary_coefficients
                            cl::Buffer,  /// error_flag
                            cl::Buffer,  /// debug_info
                            cl_uint,     /// num_prev
                            cl_uint,     /// trace_target
                            cl::Buffer,  /// trace_records
                            cl::Buffer,  /// trace_head
                            cl_uint,     /// trace_capacity
                            cl_uint,     /// trace_enabled flag
                            cl_uint      /// step index
                            >("condensed_waveguide");
    }

    auto get_zero_buffer_kernel() const {
        return program_wrapper_.get_kernel<cl::Buffer>("zero_buffer");
    }

    auto get_filter_test_kernel() const {
        return program_wrapper_
                .get_kernel<cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer>(
                        "filter_test");
    }

    auto get_filter_test_2_kernel() const {
        return program_wrapper_
                .get_kernel<cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer>(
                        "filter_test_2");
    }

    auto get_layout_probe_kernel() const {
        return program_wrapper_.get_kernel<cl::Buffer>("layout_probe");
    }

    auto get_probe_previous_kernel() const {
        return program_wrapper_
                .get_kernel<cl::Buffer, cl_uint, cl::Buffer>("probe_previous");
    }

    auto get_update_boundary_1_kernel() const {
        return program_wrapper_
                .get_kernel<cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl_int3,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl_uint,
                            cl::Buffer,
                            cl::Buffer,
                            cl_uint,
                            cl_uint,
                            cl_uint>("update_boundary_1");
    }

    auto get_update_boundary_2_kernel() const {
        return program_wrapper_
                .get_kernel<cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl_int3,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl_uint,
                            cl::Buffer,
                            cl::Buffer,
                            cl_uint,
                            cl_uint,
                            cl_uint>("update_boundary_2");
    }

    auto get_update_boundary_3_kernel() const {
        return program_wrapper_
                .get_kernel<cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl_int3,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl::Buffer,
                            cl_uint,
                            cl::Buffer,
                            cl::Buffer,
                            cl_uint,
                            cl_uint,
                            cl_uint>("update_boundary_3");
    }

    template <cl_program_info T>
    auto get_info() const {
        return program_wrapper_.template get_info<T>();
    }

    cl::Device get_device() const { return program_wrapper_.get_device(); }

private:
    core::program_wrapper program_wrapper_;
};

}  // namespace waveguide
}  // namespace wayverb
