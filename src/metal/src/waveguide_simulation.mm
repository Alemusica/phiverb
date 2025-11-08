#if defined(__APPLE__)
#  import <Metal/Metal.h>
#endif

#include "metal/waveguide_simulation.h"

#include <optional>

#include "waveguide/layout_info.h"
#include "waveguide/mesh_descriptor.h"
#include "waveguide/setup.h"

#include "core/environment.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace wayverb {
namespace metal {

namespace {

#if defined(__APPLE__)
struct Int3 final {
    int x;
    int y;
    int z;
};
#endif

#if defined(__APPLE__)
constexpr MTLResourceOptions kStorageMode =
        MTLResourceStorageModeShared | MTLResourceCPUCacheModeDefaultCache;

template <typename T, typename Container>
id<MTLBuffer> make_buffer(id<MTLDevice> device,
                          const Container& container,
                          const char* label) {
    const size_t size = container.size() * sizeof(T);
    id<MTLBuffer> buffer = [device newBufferWithLength:size options:kStorageMode];
    if (!buffer) {
        std::cerr << "[metal] failed to allocate buffer (" << label << ")\n";
        return nil;
    }
    if (size > 0) {
        std::memcpy([buffer contents], container.data(), size);
    }
    return buffer;
}

template <typename T>
id<MTLBuffer> make_buffer_with_value(id<MTLDevice> device,
                                     const T& value,
                                     const char* label) {
    id<MTLBuffer> buffer = [device newBufferWithLength:sizeof(T) options:kStorageMode];
    if (!buffer) {
        std::cerr << "[metal] failed to allocate buffer (" << label << ")\n";
        return nil;
    }
    std::memcpy([buffer contents], &value, sizeof(T));
    return buffer;
}

inline NSUInteger threads_per_threadgroup(id<MTLComputePipelineState> pso,
                                          NSUInteger preferred = 256) {
    return std::min(preferred, pso.maxTotalThreadsPerThreadgroup);
}
#endif

}  // namespace

waveguide_simulation::waveguide_simulation(const context& ctx,
                                           waveguide_pipeline& pipeline,
                                           const waveguide::mesh& mesh)
        : pipeline_{&pipeline}
        , descriptor_{mesh.get_descriptor()} {
#if defined(__APPLE__)
    (void)ctx;
    if (!pipeline.validate_layout()) {
        std::cerr << "[metal] layout validation failed; Metal backend disabled\n";
        return;
    }
    if (!upload_static_data(mesh)) {
        return;
    }
    if (!ensure_command_objects()) {
        return;
    }
#else
    (void)mesh;
#endif
}

bool waveguide_simulation::valid() const noexcept {
#if defined(__APPLE__)
    return previous_ != nil && current_ != nil && previous_history_ != nil &&
           nodes_ != nil && coefficients_ != nil;
#else
    return false;
#endif
}

size_t waveguide_simulation::run(const std::vector<float>& source_signal,
                                 size_t source_node,
                                 size_t total_steps,
                                 const std::atomic_bool& keep_going,
                                 const post_step_callback& post_step,
                                 const progress_callback& progress_step) {
#if defined(__APPLE__)
    if (!valid()) {
        return 0;
    }

    id<MTLDevice> device = pipeline_->device();
    id<MTLCommandQueue> queue = pipeline_->command_queue();
    if (!device || !queue) {
        std::cerr << "[metal] missing device or queue\n";
        return 0;
    }

    auto* prev_ptr = static_cast<float*>([previous_ contents]);
    auto* curr_ptr = static_cast<float*>([current_ contents]);
    auto* prev_hist_ptr = static_cast<float*>([previous_history_ contents]);
    auto* error_ptr = static_cast<int*>([error_flag_ contents]);
    auto* debug_ptr = static_cast<int*>([debug_info_ contents]);
    auto* trace_head_ptr = static_cast<std::atomic_uint*>([trace_head_ contents]);

    const size_t node_bytes = num_nodes_ * sizeof(float);
    std::memset(prev_ptr, 0, node_bytes);
    std::memset(curr_ptr, 0, node_bytes);
    std::memset(prev_hist_ptr, 0, node_bytes);
    *error_ptr = 0;
    std::memset(debug_ptr, 0, sizeof(int) * 12);
    if (trace_head_ptr) {
        trace_head_ptr->store(0, std::memory_order_relaxed);
    }

    const size_t steps = source_signal.size();
    const size_t actual_steps = std::min(total_steps, steps);

    NSUInteger condensed_tg = threads_per_threadgroup(pipeline_->condensed_waveguide_pso());
    NSUInteger boundary_tg =  threads_per_threadgroup(pipeline_->update_boundary_pso(1));

    for (size_t step = 0; step < actual_steps && keep_going;
         ++step) {
        curr_ptr[source_node] = source_signal[step];

        std::memcpy(prev_hist_ptr, prev_ptr, node_bytes);
        *error_ptr = 0;
        std::memset(debug_ptr, 0, sizeof(int) * 12);

        id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
        if (!command_buffer) {
            std::cerr << "[metal] failed to create command buffer\n";
            return step;
        }

        {
            id<MTLComputeCommandEncoder> enc =
                    [command_buffer computeCommandEncoder];
            [enc setComputePipelineState:pipeline_->condensed_waveguide_pso()];
            [enc setBuffer:previous_ offset:0 atIndex:0];
            [enc setBuffer:current_ offset:0 atIndex:1];
            [enc setBuffer:nodes_ offset:0 atIndex:2];
            Int3 dims{descriptor_.dimensions.s[0],
                      descriptor_.dimensions.s[1],
                      descriptor_.dimensions.s[2]};
            [enc setBytes:&dims length:sizeof(dims) atIndex:3];
            [enc setBuffer:boundary_data_1_ offset:0 atIndex:4];
            [enc setBuffer:boundary_data_2_ offset:0 atIndex:5];
            [enc setBuffer:boundary_data_3_ offset:0 atIndex:6];
            [enc setBuffer:coefficients_ offset:0 atIndex:7];
            [enc setBuffer:error_flag_ offset:0 atIndex:8];
            [enc setBuffer:debug_info_ offset:0 atIndex:9];
            uint32_t num_prev_uint = static_cast<uint32_t>(num_nodes_);
            [enc setBytes:&num_prev_uint length:sizeof(num_prev_uint) atIndex:10];
            uint32_t trace_target = std::numeric_limits<uint32_t>::max();
            [enc setBytes:&trace_target length:sizeof(trace_target) atIndex:11];
            [enc setBuffer:trace_records_ offset:0 atIndex:12];
            [enc setBuffer:trace_head_ offset:0 atIndex:13];
            uint32_t trace_capacity = 0;
            uint32_t trace_enabled = 0;
            uint32_t step_index = static_cast<uint32_t>(step);
            [enc setBytes:&trace_capacity length:sizeof(trace_capacity) atIndex:14];
            [enc setBytes:&trace_enabled length:sizeof(trace_enabled) atIndex:15];
            [enc setBytes:&step_index length:sizeof(step_index) atIndex:16];

            NSUInteger threadsPerGroup = condensed_tg;
            MTLSize threads = MTLSizeMake(static_cast<NSUInteger>(num_nodes_), 1, 1);
            MTLSize tg = MTLSizeMake(threadsPerGroup, 1, 1);
            [enc dispatchThreads:threads threadsPerThreadgroup:tg];
            [enc endEncoding];
        }

        auto run_boundary_kernel = [&](id<MTLComputePipelineState> pso,
                                       id<MTLBuffer> boundary_buffer,
                                       id<MTLBuffer> boundary_nodes_buffer,
                                       size_t boundary_count,
                                       const char* stage) {
            if (boundary_count == 0 || !pso) return;
            *error_ptr = 0;
            std::memset(debug_ptr, 0, sizeof(int) * 12);
            id<MTLComputeCommandEncoder> enc =
                    [command_buffer computeCommandEncoder];
            [enc setComputePipelineState:pso];
            [enc setBuffer:previous_history_ offset:0 atIndex:0];
            [enc setBuffer:current_ offset:0 atIndex:1];
            [enc setBuffer:previous_ offset:0 atIndex:2];
            [enc setBuffer:nodes_ offset:0 atIndex:3];
            Int3 dims{descriptor_.dimensions.s[0],
                      descriptor_.dimensions.s[1],
                      descriptor_.dimensions.s[2]};
            [enc setBytes:&dims length:sizeof(dims) atIndex:4];
            [enc setBuffer:boundary_nodes_buffer offset:0 atIndex:5];
            [enc setBuffer:boundary_buffer offset:0 atIndex:6];
            [enc setBuffer:coefficients_ offset:0 atIndex:7];
            [enc setBuffer:error_flag_ offset:0 atIndex:8];
            [enc setBuffer:debug_info_ offset:0 atIndex:9];
            uint32_t trace_target = std::numeric_limits<uint32_t>::max();
            [enc setBytes:&trace_target length:sizeof(trace_target) atIndex:10];
            [enc setBuffer:trace_records_ offset:0 atIndex:11];
            [enc setBuffer:trace_head_ offset:0 atIndex:12];
            uint32_t trace_capacity = 0;
            uint32_t trace_enabled = 0;
            uint32_t step_index = static_cast<uint32_t>(step);
            [enc setBytes:&trace_capacity length:sizeof(trace_capacity) atIndex:13];
            [enc setBytes:&trace_enabled length:sizeof(trace_enabled) atIndex:14];
            [enc setBytes:&step_index length:sizeof(step_index) atIndex:15];

            NSUInteger threadsPerGroup =
                    std::min(boundary_tg,
                             pso.maxTotalThreadsPerThreadgroup);
            MTLSize threads = MTLSizeMake(static_cast<NSUInteger>(boundary_count), 1, 1);
            MTLSize tg = MTLSizeMake(threadsPerGroup, 1, 1);
            [enc dispatchThreads:threads threadsPerThreadgroup:tg];
            [enc endEncoding];

            (void)stage;
        };

        run_boundary_kernel(pipeline_->update_boundary_pso(3),
                            boundary_data_3_,
                            boundary_nodes_3_,
                            boundary_count_3_,
                            "boundary3");
        run_boundary_kernel(pipeline_->update_boundary_pso(2),
                            boundary_data_2_,
                            boundary_nodes_2_,
                            boundary_count_2_,
                            "boundary2");
        run_boundary_kernel(pipeline_->update_boundary_pso(1),
                            boundary_data_1_,
                            boundary_nodes_1_,
                            boundary_count_1_,
                            "boundary1");

        [command_buffer commit];
        [command_buffer waitUntilCompleted];

        if (!check_error_flag("waveguide")) {
            return step;
        }

        if (post_step) {
            post_step(curr_ptr, step, total_steps);
        }
        if (progress_step) {
            progress_step(step + 1, total_steps);
        }

        std::swap(previous_, current_);
        std::swap(prev_ptr, curr_ptr);
    }

    return actual_steps;
#else
    (void)source_signal;
    (void)source_node;
    (void)total_steps;
    (void)keep_going;
    (void)post_step;
    (void)progress_step;
    return 0;
#endif
}

bool waveguide_simulation::upload_static_data(const waveguide::mesh& mesh) {
#if defined(__APPLE__)
    id<MTLDevice> device = pipeline_->device();
    if (!device) {
        return false;
    }

    const auto& structure = mesh.get_structure();
    const auto& nodes = structure.get_condensed_nodes();
    num_nodes_ = nodes.size();

    const auto boundary_data_1_host = get_boundary_data<1>(structure);
    const auto boundary_data_2_host = get_boundary_data<2>(structure);
    const auto boundary_data_3_host = get_boundary_data<3>(structure);
    boundary_count_1_ = boundary_data_1_host.size();
    boundary_count_2_ = boundary_data_2_host.size();
    boundary_count_3_ = boundary_data_3_host.size();

    const auto boundary_nodes_1_host =
            structure.get_boundary_node_indices<1>();
    const auto boundary_nodes_2_host =
            structure.get_boundary_node_indices<2>();
    const auto boundary_nodes_3_host =
            structure.get_boundary_node_indices<3>();

    const auto& coefficients_host = structure.get_coefficients();

    previous_ = [device newBufferWithLength:num_nodes_ * sizeof(float)
                                    options:kStorageMode];
    current_ = [device newBufferWithLength:num_nodes_ * sizeof(float)
                                   options:kStorageMode];
    previous_history_ =
            [device newBufferWithLength:num_nodes_ * sizeof(float)
                                 options:kStorageMode];
    if (!previous_ || !current_ || !previous_history_) {
        std::cerr << "[metal] failed to allocate pressure buffers\n";
        return false;
    }

    nodes_ = make_buffer<waveguide::condensed_node>(
            device, nodes, "nodes");
    boundary_data_1_ = make_buffer<waveguide::boundary_data_array_1>(
            device, boundary_data_1_host, "boundary_data_1");
    boundary_data_2_ = make_buffer<waveguide::boundary_data_array_2>(
            device, boundary_data_2_host, "boundary_data_2");
    boundary_data_3_ = make_buffer<waveguide::boundary_data_array_3>(
            device, boundary_data_3_host, "boundary_data_3");
    boundary_nodes_1_ = make_buffer<uint>(
            device, boundary_nodes_1_host, "boundary_nodes_1");
    boundary_nodes_2_ = make_buffer<uint>(
            device, boundary_nodes_2_host, "boundary_nodes_2");
    boundary_nodes_3_ = make_buffer<uint>(
            device, boundary_nodes_3_host, "boundary_nodes_3");
    coefficients_ = make_buffer<waveguide::coefficients_canonical>(
            device, coefficients_host, "coefficients");

    if (!nodes_ || !coefficients_) {
        return false;
    }

    error_flag_ = make_buffer_with_value<int>(device, 0, "error_flag");
    debug_info_ = [device newBufferWithLength:sizeof(int) * 12
                                       options:kStorageMode];
    trace_records_ = [device newBufferWithLength:sizeof(float) * 16
                                         options:kStorageMode];
    trace_head_ = make_buffer_with_value<uint>(device, 0, "trace_head");

    if (!error_flag_ || !debug_info_ || !trace_records_ || !trace_head_) {
        return false;
    }

    std::memset([debug_info_ contents], 0, sizeof(int) * 12);
    return true;
#else
    (void)mesh;
    return false;
#endif
}

bool waveguide_simulation::ensure_command_objects() {
#if defined(__APPLE__)
    return pipeline_->condensed_waveguide_pso() != nil &&
           pipeline_->update_boundary_pso(1) != nil;
#else
    return false;
#endif
}

bool waveguide_simulation::check_error_flag(const char* stage) const {
#if defined(__APPLE__)
    const auto* error_ptr = static_cast<const int*>([error_flag_ contents]);
    if (!error_ptr) {
        return false;
    }
    if (*error_ptr != 0) {
        std::cerr << "[metal] error flag raised during " << stage << ": "
                  << *error_ptr << "\n";
        const auto* debug_ptr =
                static_cast<const int*>([debug_info_ contents]);
        if (debug_ptr) {
            std::cerr << "  debug info:";
            for (int i = 0; i < 12; ++i) {
                std::cerr << " " << debug_ptr[i];
            }
            std::cerr << "\n";
        }
        return false;
    }
    const_cast<int*>(error_ptr)[0] = 0;
    return true;
#else
    (void)stage;
    return false;
#endif
}

}  // namespace metal
}  // namespace wayverb
