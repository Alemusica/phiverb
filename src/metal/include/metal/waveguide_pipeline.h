#pragma once

#if defined(__APPLE__)
#  include <TargetConditionals.h>
#endif

#include "waveguide/layout_info.h"

#include <cstddef>
namespace wayverb {
namespace metal {

struct context;

// Minimal Metal compute pipeline used for bring-up and profiling.
// Currently exposes a simple fill_zero() kernel plus layout validation helpers
// to guarantee parity between host/OpenCL and Metal data representations.
class waveguide_pipeline final {
public:
    explicit waveguide_pipeline(const context&);
    ~waveguide_pipeline();

    waveguide_pipeline(const waveguide_pipeline&) = delete;
    waveguide_pipeline& operator=(const waveguide_pipeline&) = delete;

    // Dispatch a trivial compute: fill 'count' floats with 0.0f.
    // Returns GPU time in milliseconds (if available), or <0 on failure.
    double fill_zero(std::size_t count) const;

    // Accessors to reuse the compiled kernel pipelines.
    id<MTLDevice> device() const;
    id<MTLCommandQueue> command_queue() const;

    id<MTLComputePipelineState> zero_buffer_pso() const;
    id<MTLComputePipelineState> condensed_waveguide_pso() const;
    id<MTLComputePipelineState> update_boundary_pso(int dims) const;
    id<MTLComputePipelineState> probe_previous_pso() const;
    id<MTLComputePipelineState> layout_probe_pso() const;

    // Query the device for structure layout information via the Metal probe
    // kernel. Returns true on success and writes the result to 'out'.
    bool query_device_layout(waveguide::layout_info& out) const;

    // Compare the device's layout against the host/OpenCL layout; logs
    // differences and returns false if any field mismatches.
    bool validate_layout() const;

private:
    void* device_ = nullptr;             // id<MTLDevice>
    void* queue_ = nullptr;              // id<MTLCommandQueue>
    void* library_ = nullptr;            // id<MTLLibrary>
    void* fill_zero_pso_ = nullptr;      // id<MTLComputePipelineState>
    void* zero_buffer_pso_ = nullptr;    // id<MTLComputePipelineState>
    void* condensed_waveguide_pso_ = nullptr; // id<MTLComputePipelineState>
    void* update_boundary1_pso_ = nullptr;    // id<MTLComputePipelineState>
    void* update_boundary2_pso_ = nullptr;    // id<MTLComputePipelineState>
    void* update_boundary3_pso_ = nullptr;    // id<MTLComputePipelineState>
    void* layout_probe_pso_ = nullptr;        // id<MTLComputePipelineState>
    void* probe_previous_pso_ = nullptr;      // id<MTLComputePipelineState>
};

}  // namespace metal
}  // namespace wayverb
