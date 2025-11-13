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
    std::optional<size_t> trace_node;
    if (const char* trace_env = std::getenv("WAYVERB_TRACE_NODE")) {
        try {
            trace_node = static_cast<size_t>(std::stoull(trace_env));
        } catch (...) {
            trace_node.reset();
        }
    }
    if (!trace_node && debug_node) {
        trace_node = debug_node;
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

    struct trace_record {
        cl_uint kind;
        cl_uint step;
        cl_uint global_index;
        cl_uint face;
        cl_float prev_pressure;
        cl_float current_pressure;
        cl_float next_pressure;
        cl_float fs0_in;
        cl_float fs0_out;
        cl_float diff;
        cl_float a0;
        cl_float b0;
    };

    const bool trace_enabled = trace_node.has_value();
    const size_t trace_capacity = trace_enabled ? static_cast<size_t>(16384) : static_cast<size_t>(1);
    cl::Buffer trace_buffer{
            cc.context,
            CL_MEM_READ_WRITE,
            sizeof(trace_record) * trace_capacity};
    cl::Buffer trace_head_buffer{cc.context, CL_MEM_READ_WRITE, sizeof(cl_uint)};
    {
        const cl_uint zero = 0;
        queue.enqueueFillBuffer(trace_head_buffer, zero, 0, sizeof(cl_uint));
    }

    const cl_uint trace_target = trace_enabled
            ? static_cast<cl_uint>(*trace_node)
            : std::numeric_limits<cl_uint>::max();
    const cl_uint trace_capacity_uint = static_cast<cl_uint>(trace_capacity);
    const cl_uint trace_enabled_flag = trace_enabled ? 1u : 0u;
    bool trace_dumped = false;
    auto dump_trace = [&](const char* reason) {
        if (!trace_enabled || trace_dumped) {
            return;
        }
        trace_dumped = true;
        queue.finish();
        const cl_uint used = core::read_value<cl_uint>(queue, trace_head_buffer, 0);
        std::cerr << "[waveguide] trace captured " << used
                  << " record(s) for node " << *trace_node;
        if (reason != nullptr) {
            std::cerr << " (" << reason << ")";
        }
        std::cerr << '\n';
        if (std::getenv("WAYVERB_TRACE_DUMP")) {
            auto records = core::read_from_buffer<trace_record>(queue, trace_buffer);
            const size_t count = std::min<size_t>(used, records.size());
            for (size_t i = 0; i < count; ++i) {
                const auto& rec = records[i];
                std::cerr << "  trace[" << i << "] kind=" << rec.kind
                          << " step=" << rec.step
                          << " face=" << rec.face
                          << " prev=" << rec.prev_pressure
                          << " curr=" << rec.current_pressure
                          << " next=" << rec.next_pressure
                          << " fs0_in=" << rec.fs0_in
                          << " fs0_out=" << rec.fs0_out
                          << " diff=" << rec.diff
                          << " a0=" << rec.a0
                          << " b0=" << rec.b0 << '\n';
            }
        }
    };

    const cl_uint num_prev = static_cast<cl_uint>(num_nodes);

    const auto node_buffer = core::load_to_buffer(
            cc.context, mesh.get_structure().get_condensed_nodes(), true);

    cl::Buffer error_flag_buffer{cc.context, CL_MEM_READ_WRITE, sizeof(cl_int)};
    cl::Buffer debug_info_buffer{
            cc.context, CL_MEM_READ_WRITE, sizeof(cl_int) * 12};
    const auto& boundary_layout = mesh.get_structure().get_boundary_layout();
    const auto boundary_count = boundary_layout.headers.size();

    auto boundary_headers_buffer =
            core::load_to_buffer(cc.context, boundary_layout.headers, false);
    auto boundary_sdf_distance_buffer =
            core::load_to_buffer(cc.context, boundary_layout.sdf_distance, false);
    util::aligned::vector<cl_float3> boundary_normals;
    boundary_normals.reserve(boundary_layout.sdf_normal.size());
    for (const auto& n : boundary_layout.sdf_normal) {
        boundary_normals.emplace_back(core::to_cl_float3{}(n));
    }
    auto boundary_sdf_normal_buffer =
            core::load_to_buffer(cc.context, boundary_normals, false);
    auto boundary_coeff_offsets_buffer =
            core::load_to_buffer(cc.context,
                                 boundary_layout.coeff_block_offsets,
                                 false);
    auto boundary_coeff_blocks_buffer =
            core::load_to_buffer(
                    cc.context, boundary_layout.coeff_blocks, false);
    auto boundary_filter_memories_buffer =
            core::load_to_buffer(cc.context,
                                 boundary_layout.filter_memories,
                                 true);
    auto boundary_lookup_buffer = core::load_to_buffer(
            cc.context, boundary_layout.node_lookup, true);
    auto boundary_node_indices_buffer =
            core::load_to_buffer(cc.context,
                                 boundary_layout.node_indices,
                                 true);

    auto kernel = program.get_kernel();
    auto update_boundary_kernel = program.get_update_boundary_kernel();
    const bool stage_trace_enabled = std::getenv("WAYVERB_WG_TRACE") != nullptr;
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
        if (!stage_trace_enabled || event() == nullptr) {
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
            cl::Buffer probe_prev_output{cc.context, CL_MEM_WRITE_ONLY, sizeof(cl_float)};
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
            std::cerr << "[waveguide] DEBUG pre-step node " << idx
                      << " locator(" << locator.x << ", " << locator.y
                      << ", " << locator.z << ")"
                      << " current=" << current_val
                      << " previous=" << previous_val
                      << " boundary_type=" << boundary_type_value
                      << " neighbors=[" << neighbors[0] << ", "
                      << neighbors[1] << ", " << neighbors[2] << ", "
                      << neighbors[3] << ", " << neighbors[4] << ", "
                      << neighbors[5] << "]\n";
            const auto layout_index =
                    idx < boundary_layout.node_lookup.size()
                            ? boundary_layout.node_lookup[idx]
                            : std::numeric_limits<uint32_t>::max();
            if (layout_index != std::numeric_limits<uint32_t>::max()) {
                const auto& header = boundary_layout.headers[layout_index];
                const auto normal = boundary_layout.sdf_normal[layout_index];
                std::cout << "  boundary_layout idx=" << layout_index
                          << " guard=0x" << std::hex << header.guard << std::dec
                          << " dif=0x" << std::hex << header.dif << std::dec
                          << " material=" << header.material_index << '\n';
                std::cout << "  sdf_distance="
                          << boundary_layout.sdf_distance[layout_index]
                          << " normal=(" << normal.x << ", " << normal.y
                          << ", " << normal.z << ")\n";
            } else {
                std::cout << "  node not registered in boundary layout\n";
            }
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
                boundary_headers_buffer,
                boundary_sdf_distance_buffer,
                boundary_sdf_normal_buffer,
                boundary_coeff_offsets_buffer,
                boundary_coeff_blocks_buffer,
                boundary_filter_memories_buffer,
                boundary_lookup_buffer,
                error_flag_buffer,
                debug_info_buffer,
                num_prev,
                trace_target,
                trace_buffer,
                trace_head_buffer,
                trace_capacity_uint,
                trace_enabled_flag,
                static_cast<cl_uint>(step));
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
                dump_trace(stage);
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
                            const auto idx = static_cast<size_t>(
                                    std::distance(values.begin(), it));
                            const auto boundary_type =
                                    idx < nodes_host.size()
                                            ? nodes_host[idx].boundary_type
                                            : -1;
                            const auto layout_index =
                                    idx < boundary_layout.node_lookup.size()
                                            ? boundary_layout.node_lookup[idx]
                                            : std::numeric_limits<uint32_t>::max();
                            std::cerr << "[waveguide] " << label
                                      << " (" << which << ") non-finite at node "
                                      << idx << " boundary_type=" << boundary_type
                                      << " layout_index=" << layout_index << '\n';
                        }
                    };
                    report("current", current);
                    report("previous", previous);
                };
;

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
                const auto debug_info =
                        core::read_from_buffer<cl_int>(queue, debug_info_buffer);
                if (debug_info.size() >= 7) {
                    std::cerr << "[waveguide] outside-mesh debug: node="
                              << debug_info[1] << " locator=(" << debug_info[2]
                              << ", " << debug_info[3] << ", " << debug_info[4]
                              << ") direction=" << debug_info[5]
                              << " slot=" << debug_info[6] << '\n';
                }
                throw std::runtime_error("Tried to read non-existant node.");
            }

            if (error_flag & id_suspicious_boundary_error) {
                throw std::runtime_error("Suspicious boundary read.");
            }
            }
        };

        check_error("pressure");

        const auto run_boundary_update = [&](const char* stage_name) {
            if (boundary_count == 0) {
                return;
            }
            core::write_value(queue, error_flag_buffer, 0, id_success);
            const cl_int pattern = static_cast<cl_int>(0xCDCDCDCD);
            queue.enqueueFillBuffer(
                    debug_info_buffer, pattern, 0, sizeof(cl_int) * 12);
            cl::Event stage_event =
                    update_boundary_kernel(cl::EnqueueArgs(
                                                   queue,
                                                   cl::NDRange(boundary_count)),
                                           previous_history,
                                           current,
                                           previous,
                                           node_buffer,
                                           mesh.get_descriptor().dimensions,
                                           boundary_node_indices_buffer,
                                           boundary_headers_buffer,
                                           boundary_coeff_offsets_buffer,
                                           boundary_coeff_blocks_buffer,
                                           boundary_filter_memories_buffer,
                                           boundary_lookup_buffer,
                                           error_flag_buffer,
                                           debug_info_buffer,
                                           trace_target,
                                           trace_buffer,
                                           trace_head_buffer,
                                           trace_capacity_uint,
                                           trace_enabled_flag,
                                           static_cast<cl_uint>(step),
                                           static_cast<cl_uint>(boundary_count));
            attach_trace(stage_name, boundary_count, stage_event);
            if (stage_event() != nullptr) {
                stage_event.wait();
            } else {
                queue.finish();
            }
            check_error(stage_name);
        };

        run_boundary_update("boundary");

        post(queue, current, step);

        std::swap(previous, current);
    }
    dump_trace("completed");
    return step;
}

}  // namespace waveguide
}  // namespace wayverb
