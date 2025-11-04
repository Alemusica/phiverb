#pragma once

#include "waveguide/mesh.h"

#include "core/cl/common.h"
#include "core/cl/include.h"
#include "core/conversions.h"
#include "core/exceptions.h"

#include "utilities/aligned/vector.h"
#include "utilities/string_builder.h"

#include <atomic>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <algorithm>
#include <limits>
#include <vector>

namespace wayverb {
namespace waveguide {

/// Will set up and run a waveguide using an existing 'template' (the mesh).
///
/// cc:             OpenCL context and device to use
/// mesh:           contains node placements and surface filter information
/// pre:            will be run before each step, should inject inputs
/// post:           will be run after each step, should collect outputs
/// keep_going:     toggle this from another thread to quit early
///
/// returns:        the number of steps completed successfully

/// step_preprocessor
/// Run before each waveguide iteration.
///
/// returns:        true if the simulation should continue

/// step_postprocessor
/// Run after each waveguide iteration.
/// Could be a stateful object which accumulates mesh state in some way.

template <typename step_preprocessor, typename step_postprocessor>
size_t run(const core::compute_context& cc,
           const mesh& mesh,
           step_preprocessor&& pre,
           step_postprocessor&& post,
           const std::atomic_bool& keep_going) {

    const auto num_nodes = mesh.get_structure().get_condensed_nodes().size();

    const program program{cc};
    cl::CommandQueue queue{cc.context, cc.device};
    const auto& nodes_host = mesh.get_structure().get_condensed_nodes();
    const auto& coefficients_host = mesh.get_structure().get_coefficients();
    const auto make_zeroed_buffer = [&] {
        auto ret = cl::Buffer{
                cc.context, CL_MEM_READ_WRITE, sizeof(cl_float) * num_nodes};
        auto kernel = program.get_zero_buffer_kernel();
        kernel(cl::EnqueueArgs{queue, cl::NDRange{num_nodes}}, ret);
        return ret;
    };

    auto previous = make_zeroed_buffer();
    auto current = make_zeroed_buffer();

    const auto node_buffer = core::load_to_buffer(
            cc.context, mesh.get_structure().get_condensed_nodes(), true);

    const auto boundary_coefficients_buffer = core::load_to_buffer(
            cc.context, mesh.get_structure().get_coefficients(), true);

    cl::Buffer error_flag_buffer{cc.context, CL_MEM_READ_WRITE, sizeof(cl_int)};

    auto boundary_host_1 = get_boundary_data<1>(mesh.get_structure());
    auto boundary_host_2 = get_boundary_data<2>(mesh.get_structure());
    auto boundary_host_3 = get_boundary_data<3>(mesh.get_structure());

    auto boundary_buffer_1 =
            core::load_to_buffer(cc.context, boundary_host_1, false);
    auto boundary_buffer_2 =
            core::load_to_buffer(cc.context, boundary_host_2, false);
    auto boundary_buffer_3 =
            core::load_to_buffer(cc.context, boundary_host_3, false);

    auto kernel = program.get_kernel();

    //  run
    auto step = 0u;

    //  The preprocessor returns 'true' while it should be run.
    //  It also updates the mesh with new pressure values.
    for (; pre(queue, current, step) && keep_going; ++step) {
        //  set flag state to successful
        core::write_value(queue, error_flag_buffer, 0, id_success);

        //  run kernel
        kernel(cl::EnqueueArgs(queue,
                               cl::NDRange(mesh.get_structure()
                                                   .get_condensed_nodes()
                                                   .size())),
               previous,
               current,
               node_buffer,
               mesh.get_descriptor().dimensions,
               boundary_buffer_1,
               boundary_buffer_2,
               boundary_buffer_3,
               boundary_coefficients_buffer,
               error_flag_buffer);

        //  read out flag value
        if (const auto error_flag =
                    core::read_value<error_code>(queue, error_flag_buffer, 0)) {
            std::cerr << "[waveguide] error_flag=" << error_flag << '\n';
            const auto log_non_finite = [&](const char* label) {
                const auto report = [&](const char* which,
                                        const cl::Buffer& buffer) {
                    auto values =
                            core::read_from_buffer<float>(queue, buffer);
                    const auto it = std::find_if(
                            values.begin(), values.end(), [](float v) {
                                return !std::isfinite(v);
                            });
                    if (it != values.end()) {
                        const auto idx =
                                static_cast<size_t>(
                                        std::distance(values.begin(), it));
                        const auto boundary_type =
                                idx < nodes_host.size()
                                        ? nodes_host[idx].boundary_type
                                        : -1;
                        int boundary_count = boundary_type >= 0
                                                     ? __builtin_popcount(
                                                               static_cast<unsigned int>(
                                                                       boundary_type))
                                                     : 0;
                        std::vector<cl_uint> coefficient_indices;
                        const auto boundary_index =
                                idx < nodes_host.size()
                                        ? nodes_host[idx].boundary_index
                                        : 0u;
                        switch (boundary_count) {
                            case 1:
                                if (boundary_index < boundary_host_1.size()) {
                                    coefficient_indices.push_back(
                                            boundary_host_1[boundary_index]
                                                    .array[0]
                                                    .coefficient_index);
                                }
                                break;
                            case 2:
                                if (boundary_index < boundary_host_2.size()) {
                                    coefficient_indices.push_back(
                                            boundary_host_2[boundary_index]
                                                    .array[0]
                                                    .coefficient_index);
                                    coefficient_indices.push_back(
                                            boundary_host_2[boundary_index]
                                                    .array[1]
                                                    .coefficient_index);
                                }
                                break;
                            case 3:
                                if (boundary_index < boundary_host_3.size()) {
                                    coefficient_indices.push_back(
                                            boundary_host_3[boundary_index]
                                                    .array[0]
                                                    .coefficient_index);
                                    coefficient_indices.push_back(
                                            boundary_host_3[boundary_index]
                                                    .array[1]
                                                    .coefficient_index);
                                    coefficient_indices.push_back(
                                            boundary_host_3[boundary_index]
                                                    .array[2]
                                                    .coefficient_index);
                                }
                                break;
                            default: break;
                        }

                        auto dump_coeffs = [&]() {
                            std::string result;
                            for (size_t i = 0; i < coefficient_indices.size(); ++i) {
                                const auto coeff_index = coefficient_indices[i];
                                float b0 = std::numeric_limits<float>::quiet_NaN();
                                float a0 = std::numeric_limits<float>::quiet_NaN();
                                if (coeff_index < coefficients_host.size()) {
                                    b0 = static_cast<float>(
                                            coefficients_host[coeff_index].b[0]);
                                    a0 = static_cast<float>(
                                            coefficients_host[coeff_index].a[0]);
                                }
                                result += util::build_string(
                                        "[", coeff_index, ": b0=", b0, ", a0=", a0, "]");
                                if (i + 1 < coefficient_indices.size()) {
                                    result += ", ";
                                }
                            }
                            if (result.empty()) {
                                result = "[none]";
                            }
                            return result;
                        }();
                        const auto locator =
                                waveguide::compute_locator(
                                        mesh.get_descriptor(), idx);
                        const auto neighbors =
                                waveguide::compute_neighbors(
                                        mesh.get_descriptor(), idx);
                        std::cerr << "[waveguide] " << label << " (" << which
                                  << ") at step " << step << ", node " << idx
                                  << ", boundary_type " << boundary_type
                                  << " (count=" << boundary_count << ")"
                                  << ", coeffs " << dump_coeffs
                                  << ", locator (" << locator.x << ", "
                                  << locator.y << ", " << locator.z << ")"
                                  << ", boundary_index " << boundary_index
                                  << ", neighbors [" << neighbors[0] << ", "
                                  << neighbors[1] << ", " << neighbors[2]
                                  << ", " << neighbors[3] << ", "
                                  << neighbors[4] << ", " << neighbors[5]
                                  << "]"
                                  << ", value " << *it << '\n';
                    }
                };

                report("current", current);
                report("previous", previous);
            };

            if (error_flag & id_inf_error) {
                log_non_finite("INF");
                throw core::exceptions::value_is_inf(
                        "Pressure value is inf, check filter coefficients.");
            }

            if (error_flag & id_nan_error) {
                log_non_finite("NaN");
                throw core::exceptions::value_is_nan(
                        "Pressure value is nan, check filter coefficients.");
            }

            if (error_flag & id_outside_mesh_error) {
                throw std::runtime_error("Tried to read non-existant node.");
            }

            if (error_flag & id_suspicious_boundary_error) {
                throw std::runtime_error("Suspicious boundary read.");
            }
        }

        post(queue, current, step);

        std::swap(previous, current);
    }
    return step;
}

}  // namespace waveguide
}  // namespace wayverb
