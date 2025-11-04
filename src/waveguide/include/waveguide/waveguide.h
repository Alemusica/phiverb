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
#include <type_traits>
#include <optional>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <memory>

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
    std::optional<size_t> debug_node;
    if (const char* debug_env = std::getenv("WAYVERB_DEBUG_NODE")) {
        try {
            debug_node = static_cast<size_t>(std::stoull(debug_env));
        } catch (...) {
            debug_node.reset();
        }
    }
    const auto make_zeroed_buffer = [&] {
        auto ret = cl::Buffer{
                cc.context, CL_MEM_READ_WRITE, sizeof(cl_float) * num_nodes};
        auto kernel = program.get_zero_buffer_kernel();
        kernel(cl::EnqueueArgs{queue, cl::NDRange{num_nodes}}, ret);
        return ret;
    };

    auto previous = make_zeroed_buffer();
    auto current = make_zeroed_buffer();
    auto previous_history = make_zeroed_buffer();

    const cl_uint num_prev = static_cast<cl_uint>(num_nodes);

    const auto node_buffer = core::load_to_buffer(
            cc.context, mesh.get_structure().get_condensed_nodes(), true);

    const auto boundary_coefficients_buffer = core::load_to_buffer(
            cc.context, mesh.get_structure().get_coefficients(), true);

    cl::Buffer error_flag_buffer{cc.context, CL_MEM_READ_WRITE, sizeof(cl_int)};
    cl::Buffer debug_info_buffer{
            cc.context, CL_MEM_READ_WRITE, sizeof(cl_int) * 12};

    auto boundary_host_1 = get_boundary_data<1>(mesh.get_structure());
    auto boundary_host_2 = get_boundary_data<2>(mesh.get_structure());
    auto boundary_host_3 = get_boundary_data<3>(mesh.get_structure());

    auto boundary_nodes_host_1 =
            mesh.get_structure().get_boundary_node_indices<1>();
    auto boundary_nodes_host_2 =
            mesh.get_structure().get_boundary_node_indices<2>();
    auto boundary_nodes_host_3 =
            mesh.get_structure().get_boundary_node_indices<3>();

    auto boundary_buffer_1 =
            core::load_to_buffer(cc.context, boundary_host_1, false);
    auto boundary_buffer_2 =
            core::load_to_buffer(cc.context, boundary_host_2, false);
    auto boundary_buffer_3 =
            core::load_to_buffer(cc.context, boundary_host_3, false);

    auto boundary_nodes_buffer_1 =
            core::load_to_buffer(cc.context, boundary_nodes_host_1, true);
    auto boundary_nodes_buffer_2 =
            core::load_to_buffer(cc.context, boundary_nodes_host_2, true);
    auto boundary_nodes_buffer_3 =
            core::load_to_buffer(cc.context, boundary_nodes_host_3, true);

    auto kernel = program.get_kernel();
    auto update_boundary_1_kernel = program.get_update_boundary_1_kernel();
    auto update_boundary_2_kernel = program.get_update_boundary_2_kernel();
    auto update_boundary_3_kernel = program.get_update_boundary_3_kernel();
    const bool trace_enabled = std::getenv("WAYVERB_WG_TRACE") != nullptr;
    struct stage_trace_payload {
        std::string name;
        size_t global_work_size{};
    };
    struct stage_trace_logger {
        static void CL_CALLBACK on_complete(cl_event,
                                             cl_int status,
                                             void* user_data) {
            std::unique_ptr<stage_trace_payload> payload{
                    static_cast<stage_trace_payload*>(user_data)};
            if (!payload) {
                return;
            }
            std::fprintf(stderr,
                         "[waveguide][trace] stage '%s' complete "
                         "(status=%d, gws=%zu)\n",
                         payload->name.c_str(),
                         status,
                         payload->global_work_size);
        }
    };
    auto attach_trace = [&](const char* name,
                            size_t global_work_size,
                            cl::Event& event) {
        if (!trace_enabled || event() == nullptr) {
            return;
        }
        auto payload = std::make_unique<stage_trace_payload>();
        payload->name = name;
        payload->global_work_size = global_work_size;
        auto raw = payload.release();
        const cl_int err =
                event.setCallback(CL_COMPLETE,
                                  &stage_trace_logger::on_complete,
                                  raw);
        if (err != CL_SUCCESS) {
            delete raw;
            std::fprintf(stderr,
                         "[waveguide][trace] setCallback failed (%d) for %s\n",
                         err,
                         name);
        }
    };

    const char* max_steps_env = std::getenv("WAYVERB_MAX_STEPS");
    const size_t max_steps = max_steps_env != nullptr
            ? static_cast<size_t>(std::strtoull(max_steps_env, nullptr, 10))
            : std::numeric_limits<size_t>::max();
    //  run
    auto step = 0u;

    //  The preprocessor returns 'true' while it should be run.
    //  It also updates the mesh with new pressure values.
    for (; pre(queue, current, step) && keep_going && step < max_steps;
         ++step) {
        queue.enqueueCopyBuffer(
                previous, previous_history, 0, 0, sizeof(cl_float) * num_nodes);
        if (debug_node && step == 0) {
            const auto idx = *debug_node;
            auto probe_prev_kernel = program.get_probe_previous_kernel();
            cl::Buffer probe_prev_output{
                    cc.context, CL_MEM_WRITE_ONLY, sizeof(cl_float)};
            probe_prev_kernel(cl::EnqueueArgs{queue, cl::NDRange{1}},
                               previous,
                               static_cast<cl_uint>(idx),
                               probe_prev_output);
            queue.finish();
            const auto probed =
                    core::read_value<float>(queue, probe_prev_output, 0);
            std::cerr << "[waveguide] DEBUG probe-prev node " << idx
                      << " value=" << probed << '\n';
            const auto locator = waveguide::compute_locator(
                    mesh.get_descriptor(), idx);
            const auto neighbors =
                    waveguide::compute_neighbors(mesh.get_descriptor(), idx);
            const auto current_val = core::read_value<float>(queue, current, idx);
            const auto previous_val = core::read_value<float>(queue, previous, idx);
            const auto boundary_type_value = nodes_host[idx].boundary_type;
            const auto boundary_index = nodes_host[idx].boundary_index;
            const int boundary_faces = __builtin_popcount(
                    static_cast<unsigned int>(
                            boundary_type_value & ~id_inside));
            std::cerr << "[waveguide] DEBUG pre-step node " << idx
                      << " locator(" << locator.x << ", " << locator.y
                      << ", " << locator.z << ")"
                      << " current=" << current_val
                      << " previous=" << previous_val
                      << " boundary_type=" << boundary_type_value
                      << " boundary_index=" << boundary_index
                      << " neighbors=[" << neighbors[0] << ", "
                      << neighbors[1] << ", " << neighbors[2] << ", "
                      << neighbors[3] << ", " << neighbors[4] << ", "
                      << neighbors[5] << "]\n";
            std::cout << "  boundary_index=" << boundary_index
                      << " popcount(excl inside)=" << boundary_faces << '\n';
            std::cout << "  boundary_data sizes: b1=" << boundary_host_1.size()
                      << " b2=" << boundary_host_2.size()
                      << " b3=" << boundary_host_3.size() << '\n';
            if (boundary_faces == 3 && boundary_index < boundary_host_3.size()) {
                const auto& tri = boundary_host_3[boundary_index];
                using tri_type = std::decay_t<decltype(tri)>;
                for (size_t i = 0; i < tri_type::DIMENSIONS; ++i) {
                    const auto& bd = tri.array[i];
                    std::cout << "    tri[" << i
                              << "] coeff_index=" << bd.coefficient_index
                              << " memory[0]=" << bd.filter_memory.array[0]
                              << " memory[1]=" << bd.filter_memory.array[1]
                              << '\n';
                    if (bd.coefficient_index <
                        mesh.get_structure().get_coefficients().size()) {
                        const auto& coeff =
                                mesh.get_structure().get_coefficients()
                                        [bd.coefficient_index];
                        std::cout << "      coeff a0=" << coeff.a[0]
                                  << " b0=" << coeff.b[0] << '\n';
                    } else {
                        std::cout << "      coeff index out of range (size="
                                  << mesh.get_structure()
                                             .get_coefficients()
                                             .size()
                                  << ")\n";
                    }
                }
            }
            if (boundary_faces == 2 && boundary_index < boundary_host_2.size()) {
                const auto& edge = boundary_host_2[boundary_index];
                using edge_type = std::decay_t<decltype(edge)>;
                for (size_t i = 0; i < edge_type::DIMENSIONS; ++i) {
                    const auto& bd = edge.array[i];
                    std::cout << "    edge[" << i
                              << "] coeff_index=" << bd.coefficient_index
                              << " memory[0]=" << bd.filter_memory.array[0]
                              << " memory[1]=" << bd.filter_memory.array[1]
                              << '\n';
                    if (bd.coefficient_index <
                        mesh.get_structure().get_coefficients().size()) {
                        const auto& coeff =
                                mesh.get_structure().get_coefficients()
                                        [bd.coefficient_index];
                        std::cout << "      coeff a0=" << coeff.a[0]
                                  << " b0=" << coeff.b[0] << '\n';
                    }
                }
            }
            if (boundary_faces == 2 && boundary_index < boundary_host_2.size()) {
                const auto& edge = boundary_host_2[boundary_index];
                using edge_type = std::decay_t<decltype(edge)>;
                for (size_t i = 0; i < edge_type::DIMENSIONS; ++i) {
                    const auto& bd = edge.array[i];
                    std::cout << "    edge[" << i
                              << "] coeff_index=" << bd.coefficient_index
                              << " memory[0]=" << bd.filter_memory.array[0]
                              << " memory[1]=" << bd.filter_memory.array[1]
                              << '\n';
                    if (bd.coefficient_index <
                        mesh.get_structure().get_coefficients().size()) {
                        const auto& coeff =
                                mesh.get_structure().get_coefficients()
                                        [bd.coefficient_index];
                        std::cout << "      coeff a0=" << coeff.a[0]
                                  << " b0=" << coeff.b[0] << '\n';
                    }
                }
            }
            for (auto neighbor : neighbors) {
                if (neighbor != mesh_descriptor::no_neighbor) {
                    const auto neighbor_val =
                            core::read_value<float>(queue, current, neighbor);
                    std::cerr << "  neighbor " << neighbor
                              << " current=" << neighbor_val << '\n';
                }
            }

            const float courant_sq = 1.0f / 3.0f;
            const int boundary_count =
                    __builtin_popcount(static_cast<unsigned int>(
                            boundary_type_value & ~id_inside));
            const auto neighbor_for_port = [&](int port) -> cl_uint {
                switch (port) {
                    case 0: return neighbors[0];
                    case 1: return neighbors[1];
                    case 2: return neighbors[2];
                    case 3: return neighbors[3];
                    case 4: return neighbors[4];
                    case 5: return neighbors[5];
                    default: return mesh_descriptor::no_neighbor;
                }
            };

            const auto read_current = [&](cl_uint index_val) -> float {
                if (index_val == mesh_descriptor::no_neighbor) { return 0.0f; }
                return core::read_value<float>(queue, current, index_val);
            };

            auto get_inner_dirs = [&](int bt) -> std::array<int, 2> {
                switch (bt) {
                    case id_nx | id_ny: return {0, 2};
                    case id_nx | id_py: return {0, 3};
                    case id_px | id_ny: return {1, 2};
                    case id_px | id_py: return {1, 3};
                    case id_nx | id_nz: return {0, 4};
                    case id_nx | id_pz: return {0, 5};
                    case id_px | id_nz: return {1, 4};
                    case id_px | id_pz: return {1, 5};
                    case id_ny | id_nz: return {2, 4};
                    case id_ny | id_pz: return {2, 5};
                    case id_py | id_nz: return {3, 4};
                    case id_py | id_pz: return {3, 5};
                    default: return {-1, -1};
                }
            };

            auto get_surrounding_dirs = [&](const std::array<int, 2>& ind) {
                std::array<int, 2> result{0, 0};
                const bool has_x = (ind[0] == 0 || ind[0] == 1 || ind[1] == 0 || ind[1] == 1);
                const bool has_y = (ind[0] == 2 || ind[0] == 3 || ind[1] == 2 || ind[1] == 3);
                if (has_x && has_y) {
                    result = {4, 5};
                } else if (has_x) {
                    result = {2, 3};
                } else {
                    result = {0, 1};
                }
                return result;
            };

            const auto inner_dirs = get_inner_dirs(boundary_type_value & ~id_inside);
            auto surrounding_dirs = get_surrounding_dirs(inner_dirs);

            float sum_inner = 0.0f;
            for (int dir : inner_dirs) {
                if (dir >= 0) {
                    sum_inner += 2.0f * read_current(neighbor_for_port(dir));
                }
            }

            float sum_surround = 0.0f;
            for (int dir : surrounding_dirs) {
                sum_surround += read_current(neighbor_for_port(dir));
            }

            const float current_surrounding_weighting =
                    courant_sq * (sum_inner + sum_surround);

            const auto dump_boundary = [&](int count) {
                const auto log_entry = [&](const auto& boundary_host,
                                           const char* label) {
                    if (boundary_index >= boundary_host.size()) {
                        std::cerr << "  " << label
                                  << " index out of range; size="
                                  << boundary_host.size() << "\n";
                        return;
                    }
                    const auto& arr = boundary_host[boundary_index];
                    for (int entry = 0; entry < count; ++entry) {
                        const auto coeff_index = arr.array[entry].coefficient_index;
                        float a0 = std::numeric_limits<float>::quiet_NaN();
                        float b0 = std::numeric_limits<float>::quiet_NaN();
                        if (coeff_index < coefficients_host.size()) {
                            a0 = static_cast<float>(coefficients_host[coeff_index].a[0]);
                            b0 = static_cast<float>(coefficients_host[coeff_index].b[0]);
                        }
                        const float mem0 =
                                static_cast<float>(arr.array[entry].filter_memory.array[0]);
                        std::cerr << "  " << label << "[" << entry
                                  << "] coeff_index=" << coeff_index
                                  << " a0=" << a0 << " b0=" << b0
                                  << " mem0=" << mem0 << "\n";
                    }
                };
                switch (count) {
                    case 1: log_entry(boundary_host_1, "b1"); break;
                    case 2: log_entry(boundary_host_2, "b2"); break;
                    case 3: log_entry(boundary_host_3, "b3"); break;
                    default:
                        std::cerr << "  boundary_count=" << count
                                  << " (no boundary data vector)\n";
                        break;
                }
            };
            dump_boundary(boundary_count);

            float filter_weighting = 0.0f;
            if (!(boundary_type_value & id_inside)) {
                auto accumulate_filter_weighting = [&](const auto& arr) {
                    for (const auto& bd : arr.array) {
                        const auto ci = bd.coefficient_index;
                        if (ci < coefficients_host.size()) {
                            const float b0 = static_cast<float>(coefficients_host[ci].b[0]);
                            const float mem0 = static_cast<float>(bd.filter_memory.array[0]);
                            if (b0 != 0.0f) {
                                filter_weighting += mem0 / b0;
                            }
                        }
                    }
                };

                if (boundary_count == 1 && boundary_index < boundary_host_1.size()) {
                    accumulate_filter_weighting(boundary_host_1[boundary_index]);
                } else if (boundary_count == 2 && boundary_index < boundary_host_2.size()) {
                    accumulate_filter_weighting(boundary_host_2[boundary_index]);
                } else if (boundary_count == 3 && boundary_index < boundary_host_3.size()) {
                    accumulate_filter_weighting(boundary_host_3[boundary_index]);
                }
            }

            filter_weighting *= courant_sq;

            float coeff_weighting = 0.0f;
            if (boundary_count == 2 && boundary_index < boundary_host_2.size()) {
                const auto& arr = boundary_host_2[boundary_index];
                for (const auto& bd : arr.array) {
                    const auto ci = bd.coefficient_index;
                    if (ci < coefficients_host.size()) {
                        const float a0 = static_cast<float>(coefficients_host[ci].a[0]);
                        const float b0 = static_cast<float>(coefficients_host[ci].b[0]);
                        if (b0 != 0.0f) {
                            coeff_weighting += a0 / b0;
                        }
                    }
                }
            }

            const float prev_weighting = (coeff_weighting - 1.0f) * previous_val;
            const float numerator = current_surrounding_weighting + filter_weighting + prev_weighting;
            const float denominator = 1.0f + coeff_weighting;
            float approx_ret = numerator / denominator;

            std::cerr << "  current_surrounding_weighting=" << current_surrounding_weighting
                      << " filter_weighting=" << filter_weighting
                      << " coeff_weighting=" << coeff_weighting
                      << " prev_weighting=" << prev_weighting
                      << " numerator=" << numerator
                      << " denominator=" << denominator
                      << " approx_ret=" << approx_ret << '\n';
        }

        //  set flag state to successful
        core::write_value(queue, error_flag_buffer, 0, id_success);
        {
            const cl_int pattern = static_cast<cl_int>(0xCDCDCDCD);
            queue.enqueueFillBuffer(
                    debug_info_buffer,
                    pattern,
                    0,
                    sizeof(cl_int) * 12);
        }

        //  run kernel
        cl::Event pressure_event = kernel(
                cl::EnqueueArgs(queue,
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
                error_flag_buffer,
                debug_info_buffer,
                num_prev);
        attach_trace("pressure",
                     mesh.get_structure().get_condensed_nodes().size(),
                     pressure_event);
        if (pressure_event() != nullptr) {
            pressure_event.wait();
        } else {
            queue.finish();
        }

        auto check_error = [&](const char* stage) {
            if (const auto error_flag =
                        core::read_value<error_code>(queue,
                                                     error_flag_buffer,
                                                     0)) {
                std::cerr << "[waveguide][" << stage
                          << "] error_flag=" << error_flag << '\n';
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
                        const auto boundary_index =
                                idx < nodes_host.size()
                                        ? nodes_host[idx].boundary_index
                                        : 0u;

                        auto dump_boundary = [&](const char* label_prefix,
                                                  const auto& boundary_host) {
                            std::string result;
                            if (boundary_index < boundary_host.size()) {
                                const auto& arr = boundary_host[boundary_index];
                                using array_type = std::decay_t<decltype(arr)>;
                                for (size_t i = 0; i < array_type::DIMENSIONS; ++i) {
                                    const auto coeff_index = arr.array[i].coefficient_index;
                                    float b0 = std::numeric_limits<float>::quiet_NaN();
                                    float a0 = std::numeric_limits<float>::quiet_NaN();
                                    float mem0 = std::numeric_limits<float>::quiet_NaN();
                                    if (coeff_index < coefficients_host.size()) {
                                        b0 = static_cast<float>(
                                                coefficients_host[coeff_index].b[0]);
                                        a0 = static_cast<float>(
                                                coefficients_host[coeff_index].a[0]);
                                    }
                                    mem0 = static_cast<float>(
                                            arr.array[i].filter_memory.array[0]);
                                    if (!result.empty()) {
                                        result += ", ";
                                    }
                                    result += util::build_string(
                                            label_prefix,
                                            "[", i, ": coeff=", coeff_index,
                                            ", b0=", b0,
                                            ", a0=", a0,
                                            ", mem0=", mem0,
                                            "]");
                                }
                            }
                            if (result.empty()) {
                                result = util::build_string(label_prefix, "[none]");
                            }
                            return result;
                        };

                        std::string boundary_details;
                        switch (boundary_count) {
                            case 1:
                                boundary_details =
                                        dump_boundary("b1", boundary_host_1);
                                break;
                            case 2:
                                boundary_details =
                                        dump_boundary("b2", boundary_host_2);
                                break;
                            case 3:
                                boundary_details =
                                        dump_boundary("b3", boundary_host_3);
                                break;
                            default: boundary_details = "b[none]";
                        }
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
                                  << ", " << boundary_details
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
                auto debug_raw = core::read_from_buffer<cl_int>(
                        queue, debug_info_buffer);
                std::cerr << "  debug_raw[0..3]=";
                for (int i = 0; i < 4 && i < (int)debug_raw.size(); ++i) {
                    std::cerr << " 0x" << std::hex << debug_raw[i] << std::dec;
                }
                std::cerr << '\n';
                auto debug_info =
                        core::read_from_buffer<cl_int>(queue, debug_info_buffer);
                if (!debug_info.empty()) {
                    const auto bits_to_float = [](cl_int bits) {
                        union {
                            cl_uint u;
                            float f;
                        } converter{static_cast<cl_uint>(bits)};
                        return converter.f;
                    };
                    const auto safe_get = [&](size_t index) -> cl_int {
                        return debug_info.size() > index ? debug_info[index] : -1;
                    };
                    std::cerr << "[waveguide] nan-debug (size=" << debug_info.size()
                              << ") code=" << safe_get(0)
                              << " node=" << safe_get(1)
                              << " boundary_index=" << safe_get(2)
                              << " local_idx=" << safe_get(3)
                              << " coeff_index=" << safe_get(4)
                              << " filt_state_bits=" << safe_get(5)
                              << " a0_bits=" << safe_get(6)
                              << " b0_bits=" << safe_get(7)
                              << " diff_bits=" << safe_get(8)
                              << " filter_in_bits=" << safe_get(9)
                              << " prev_bits=" << safe_get(10)
                              << " next_bits=" << safe_get(11) << '\n';
                    if (debug_info.size() > 11) {
                        std::cerr << "  decoded: filt_state="
                                  << bits_to_float(debug_info[5])
                                  << " a0="
                                  << bits_to_float(debug_info[6])
                                  << " b0="
                                  << bits_to_float(debug_info[7])
                                  << " diff="
                                  << bits_to_float(debug_info[8])
                                  << " filter_input="
                                  << bits_to_float(debug_info[9])
                                  << " prev_pressure="
                                  << bits_to_float(debug_info[10])
                                  << " next_pressure="
                                  << bits_to_float(debug_info[11]) << '\n';
                    }
                }
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
        };

        check_error("pressure");

        const auto run_boundary_update =
                [&](auto& functor,
                    auto& boundary_buffer,
                    const cl::Buffer& node_map,
                    size_t count,
                    const char* stage_name) {
                    if (count == 0) {
                        return;
                    }
                    core::write_value(queue, error_flag_buffer, 0, id_success);
                    const cl_int pattern = static_cast<cl_int>(0xCDCDCDCD);
                    queue.enqueueFillBuffer(
                            debug_info_buffer,
                            pattern,
                            0,
                            sizeof(cl_int) * 12);
                    cl::Event stage_event =
                            functor(cl::EnqueueArgs(queue, cl::NDRange(count)),
                                    previous_history,
                                    current,
                                    previous,
                                    node_buffer,
                                    mesh.get_descriptor().dimensions,
                                    node_map,
                                    boundary_buffer,
                                    boundary_coefficients_buffer,
                                    error_flag_buffer,
                                    debug_info_buffer);
                    attach_trace(stage_name, count, stage_event);
                    if (stage_event() != nullptr) {
                        stage_event.wait();
                    } else {
                        queue.finish();
                    }
                    check_error(stage_name);
                };

        run_boundary_update(update_boundary_3_kernel,
                            boundary_buffer_3,
                            boundary_nodes_buffer_3,
                            boundary_host_3.size(),
                            "boundary_3");
        run_boundary_update(update_boundary_2_kernel,
                            boundary_buffer_2,
                            boundary_nodes_buffer_2,
                            boundary_host_2.size(),
                            "boundary_2");
        run_boundary_update(update_boundary_1_kernel,
                            boundary_buffer_1,
                            boundary_nodes_buffer_1,
                            boundary_host_1.size(),
                            "boundary_1");

        post(queue, current, step);

        std::swap(previous, current);
    }
    return step;
}

}  // namespace waveguide
}  // namespace wayverb
