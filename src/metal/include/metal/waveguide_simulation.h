#pragma once

#if defined(__APPLE__)
#  include <TargetConditionals.h>
#endif

#include "metal/metal_context.h"
#include "metal/waveguide_pipeline.h"

#include "waveguide/mesh.h"

#include <atomic>
#include <functional>
#include <vector>

@class MTLBuffer;

namespace wayverb {
namespace metal {

class waveguide_simulation final {
public:
    waveguide_simulation(const context& ctx,
                         waveguide_pipeline& pipeline,
                         const waveguide::mesh& mesh);

    bool valid() const noexcept;

    using post_step_callback =
            std::function<void(const float* current_buffer,
                               size_t step,
                               size_t total_steps)>;
    using progress_callback =
            std::function<void(size_t step, size_t total_steps)>;

    size_t run(const std::vector<float>& source_signal,
               size_t source_node,
               size_t total_steps,
               const std::atomic_bool& keep_going,
               const post_step_callback& post_step,
               const progress_callback& progress_step);

    const waveguide::mesh_descriptor& descriptor() const noexcept {
        return descriptor_;
    }

private:
    bool upload_static_data(const waveguide::mesh& mesh);
    bool ensure_command_objects();
    bool check_error_flag(const char* stage) const;

    waveguide_pipeline* pipeline_{};

    waveguide::mesh_descriptor descriptor_{};
    size_t num_nodes_{};
    size_t boundary_count_1_{};
    size_t boundary_count_2_{};
    size_t boundary_count_3_{};

#if defined(__APPLE__)
    id<MTLBuffer> previous_{nil};
    id<MTLBuffer> current_{nil};
    id<MTLBuffer> previous_history_{nil};
    id<MTLBuffer> nodes_{nil};
    id<MTLBuffer> boundary_data_1_{nil};
    id<MTLBuffer> boundary_data_2_{nil};
    id<MTLBuffer> boundary_data_3_{nil};
    id<MTLBuffer> boundary_nodes_1_{nil};
    id<MTLBuffer> boundary_nodes_2_{nil};
    id<MTLBuffer> boundary_nodes_3_{nil};
    id<MTLBuffer> coefficients_{nil};
    id<MTLBuffer> error_flag_{nil};
    id<MTLBuffer> debug_info_{nil};
    id<MTLBuffer> trace_records_{nil};
    id<MTLBuffer> trace_head_{nil};
#endif
};

}  // namespace metal
}  // namespace wayverb
