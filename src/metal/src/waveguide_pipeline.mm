#if defined(__APPLE__)
#  import <Metal/Metal.h>
#  import <Foundation/Foundation.h>
#endif

#include "metal/metal_context.h"
#include "metal/waveguide_pipeline.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace wayverb {
namespace metal {

namespace {

std::string read_file_to_string(const std::string& path) {
    std::ifstream file{path};
    if (!file) {
        return {};
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::string load_kernel_source() {
#if defined(__APPLE__)
    @autoreleasepool {
        NSString* source_file = [NSString stringWithUTF8String:__FILE__];
        NSString* source_dir = [source_file stringByDeletingLastPathComponent];
        NSString* path =
                [source_dir stringByAppendingPathComponent:@"waveguide_kernels.metal"];
        NSError* error = nil;
        NSString* src =
                [NSString stringWithContentsOfFile:path
                                          encoding:NSUTF8StringEncoding
                                             error:&error];
        if (!src) {
            std::cerr << "[metal] warning: could not read "
                      << (path ? [path UTF8String] : "<unknown path>")
                      << "; falling back to inline Metal source";
            if (error) {
                std::cerr << " (" << [[error localizedDescription] UTF8String]
                          << ")";
            }
            std::cerr << "\n";
            return {};
        }
        std::string source = [src UTF8String] ? std::string([src UTF8String]) : std::string{};
        std::string root = [source_dir UTF8String] ? std::string([source_dir UTF8String]) : std::string{};
        auto inline_include = [&](const char* token,
                                  const std::string& full_path) {
            const std::string include_token = std::string("#include \"") + token + "\"";
            const auto pos = source.find(include_token);
            if (pos == std::string::npos) {
                return;
            }
            const std::string replacement = read_file_to_string(full_path);
            if (replacement.empty()) {
                std::cerr << "[metal] warning: failed to inline "
                          << full_path << "\n";
                return;
            }
            size_t erase_len = include_token.size();
            if (pos + erase_len < source.size() && source[pos + erase_len] == '\n') {
                ++erase_len;
            }
            source.erase(pos, erase_len);
            source.insert(0, replacement + "\n");
            std::cerr << "[metal] inlined " << token << " from " << full_path << "\n";
        };

        const std::string base = root.empty() ? std::string{} : root + "/../..";
        inline_include("waveguide/metal/layout_structs.metal",
                       base + "/waveguide/include/waveguide/metal/layout_structs.metal");
        inline_include("waveguide/metal/common.metal",
                       base + "/waveguide/include/waveguide/metal/common.metal");
        return source;
    }
#else
    return {};
#endif
}

const char* fallback_kernel_source() {
    return R"(#include <metal_stdlib>
using namespace metal;

namespace wayverb_metal {

constexpr constant uint BIQUAD_ORDER = 2;
constexpr constant uint BIQUAD_SECTIONS = 3;
constexpr constant uint CANONICAL_FILTER_ORDER = BIQUAD_ORDER * BIQUAD_SECTIONS;
constexpr constant uint CANONICAL_FILTER_STORAGE = CANONICAL_FILTER_ORDER + 2;

enum boundary_type : int {
    id_none = 0,
    id_inside = 1 << 0,
    id_nx = 1 << 1,
    id_px = 1 << 2,
    id_ny = 1 << 3,
    id_py = 1 << 4,
    id_nz = 1 << 5,
    id_pz = 1 << 6,
    id_reentrant = 1 << 7,
};

enum PortDirection : uint {
    id_port_nx = 0,
    id_port_px = 1,
    id_port_ny = 2,
    id_port_py = 3,
    id_port_nz = 4,
    id_port_pz = 5,
};

struct condensed_node {
    int boundary_type;
    uint boundary_index;
};

struct mesh_descriptor {
    float3 min_corner;
    int3 dimensions;
    float spacing;
};

struct memory_canonical {
    float array[CANONICAL_FILTER_STORAGE];
};

struct coefficients_canonical {
    float b[CANONICAL_FILTER_STORAGE];
    float a[CANONICAL_FILTER_STORAGE];
};

struct boundary_data {
    memory_canonical filter_memory;
    uint coefficient_index;
    uint _pad[7];
};

struct boundary_data_array_1 {
    boundary_data array[1];
};

struct boundary_data_array_2 {
    boundary_data array[2];
};

struct boundary_data_array_3 {
    boundary_data array[3];
};

struct trace_record {
    uint kind;
    uint step;
    uint global_index;
    uint face;
    float prev_pressure;
    float current_pressure;
    float next_pressure;
    float fs0_in;
    float fs0_out;
    float diff;
    float a0;
    float b0;
};

struct layout_info {
    uint sz_memory_canonical;
    uint sz_coefficients_canonical;
    uint sz_boundary_data;
    uint sz_boundary_data_array_3;
    uint off_bd_filter_memory;
    uint off_bd_coefficient_index;
    uint off_b3_data0;
    uint off_b3_data1;
    uint off_b3_data2;
};

} // namespace wayverb_metal

struct Params { uint count; };

kernel void fill_zero(device float* outBuffer [[buffer(0)]],
                      constant Params& params [[buffer(1)]],
                      uint gid [[thread_position_in_grid]]) {
    if (gid < params.count) { outBuffer[gid] = 0.0f; }
}

kernel void layout_probe(device wayverb_metal::layout_info* outInfo [[buffer(0)]],
                         uint tid [[thread_position_in_grid]]) {
    if (tid != 0) return;
    using namespace wayverb_metal;
    layout_info info;
    info.sz_memory_canonical = sizeof(memory_canonical);
    info.sz_coefficients_canonical = sizeof(coefficients_canonical);
    info.sz_boundary_data = sizeof(boundary_data);
    info.sz_boundary_data_array_3 = sizeof(boundary_data_array_3);
    info.off_bd_filter_memory = offsetof(boundary_data, filter_memory);
    info.off_bd_coefficient_index = offsetof(boundary_data, coefficient_index);
    info.off_b3_data0 = offsetof(boundary_data_array_3, array[0]);
    info.off_b3_data1 = offsetof(boundary_data_array_3, array[1]);
    info.off_b3_data2 = offsetof(boundary_data_array_3, array[2]);
    outInfo[0] = info;
}
)";
}

id<MTLLibrary> make_library(id<MTLDevice> dev) {
    std::string source = load_kernel_source();
    if (source.empty()) {
        source = fallback_kernel_source();
    }

    NSError* err = nil;
    NSString* ns_source = [NSString stringWithUTF8String:source.c_str()];
    id<MTLLibrary> lib =
            [dev newLibraryWithSource:ns_source options:nil error:&err];
    if (!lib) {
        std::cerr << "[metal] library compile failed: "
                  << (err ? [[err localizedDescription] UTF8String] : "unknown")
                  << "\n";
    }
    return lib;
}

}  // namespace

waveguide_pipeline::waveguide_pipeline(const context& ctx) {
#if defined(__APPLE__)
    device_ = ctx.device;
    queue_  = ctx.command_queue;
    id<MTLDevice> dev = (__bridge id<MTLDevice>)device_;
    id<MTLCommandQueue> q = (__bridge id<MTLCommandQueue>)queue_;
    (void)q;

    id<MTLLibrary> lib = make_library(dev);
    if (!lib) return;
    library_ = (__bridge_retained void*)lib;

    NSError* err = nil;
    id<MTLFunction> fill_fn = [lib newFunctionWithName:@"fill_zero"];
    if (!fill_fn) {
        std::cerr << "[metal] missing fill_zero function in library\n";
        return;
    }
    id<MTLComputePipelineState> fill_pso =
            [dev newComputePipelineStateWithFunction:fill_fn error:&err];
    if (!fill_pso) {
        std::cerr << "[metal] fill_zero pipeline init failed: "
                  << (err ? [[err localizedDescription] UTF8String] : "unknown")
                  << "\n";
        return;
    }
    fill_zero_pso_ = (__bridge_retained void*)fill_pso;

    err = nil;
    id<MTLFunction> zero_fn = [lib newFunctionWithName:@"zero_buffer"];
    if (!zero_fn) {
        std::cerr << "[metal] missing zero_buffer function in library\n";
        return;
    }
    id<MTLComputePipelineState> zero_pso =
            [dev newComputePipelineStateWithFunction:zero_fn error:&err];
    if (!zero_pso) {
        std::cerr << "[metal] zero_buffer pipeline init failed: "
                  << (err ? [[err localizedDescription] UTF8String] : "unknown")
                  << "\n";
        return;
    }
    zero_buffer_pso_ = (__bridge_retained void*)zero_pso;

    auto make_kernel = [&](NSString* name,
                           void** storage,
                           const char* label) {
        NSError* fnErr = nil;
        id<MTLFunction> fn = [lib newFunctionWithName:name];
        if (!fn) {
            std::cerr << "[metal] missing " << label << " function in library\n";
            return false;
        }
        id<MTLComputePipelineState> pso =
                [dev newComputePipelineStateWithFunction:fn error:&fnErr];
        if (!pso) {
            std::cerr << "[metal] " << label << " pipeline init failed: "
                      << (fnErr ? [[fnErr localizedDescription] UTF8String]
                                : "unknown")
                      << "\n";
            return false;
        }
        *storage = (__bridge_retained void*)pso;
        return true;
    };

    if (!make_kernel(@"condensed_waveguide",
                     &condensed_waveguide_pso_,
                     "condensed_waveguide")) {
        return;
    }
    if (!make_kernel(@"update_boundary_1",
                     &update_boundary1_pso_,
                     "update_boundary_1")) {
        return;
    }
    if (!make_kernel(@"update_boundary_2",
                     &update_boundary2_pso_,
                     "update_boundary_2")) {
        return;
    }
    if (!make_kernel(@"update_boundary_3",
                     &update_boundary3_pso_,
                     "update_boundary_3")) {
        return;
    }

    err = nil;
    id<MTLFunction> layout_fn = [lib newFunctionWithName:@"layout_probe"];
    if (!layout_fn) {
        std::cerr << "[metal] warning: layout_probe function not found; layout validation disabled\n";
    } else {
        id<MTLComputePipelineState> layout_pso =
                [dev newComputePipelineStateWithFunction:layout_fn error:&err];
        if (!layout_pso) {
            std::cerr << "[metal] layout_probe pipeline init failed: "
                      << (err ? [[err localizedDescription] UTF8String]
                              : "unknown")
                      << "\n";
        } else {
            layout_probe_pso_ = (__bridge_retained void*)layout_pso;
        }
    }

    err = nil;
    id<MTLFunction> probe_prev_fn = [lib newFunctionWithName:@"probe_previous"];
    if (!probe_prev_fn) {
        std::cerr << "[metal] warning: probe_previous function not found; debug probe disabled\n";
    } else {
        id<MTLComputePipelineState> probe_prev_pso =
                [dev newComputePipelineStateWithFunction:probe_prev_fn error:&err];
        if (!probe_prev_pso) {
            std::cerr << "[metal] probe_previous pipeline init failed: "
                      << (err ? [[err localizedDescription] UTF8String]
                              : "unknown")
                      << "\n";
        } else {
            probe_previous_pso_ = (__bridge_retained void*)probe_prev_pso;
        }
    }
#endif
}

waveguide_pipeline::~waveguide_pipeline() {
#if defined(__APPLE__)
    if (probe_previous_pso_) {
        id<MTLComputePipelineState> pso =
                (__bridge_transfer id<MTLComputePipelineState>)probe_previous_pso_;
        (void)pso;
        probe_previous_pso_ = nullptr;
    }
    if (layout_probe_pso_) {
        id<MTLComputePipelineState> pso =
                (__bridge_transfer id<MTLComputePipelineState>)layout_probe_pso_;
        (void)pso;
        layout_probe_pso_ = nullptr;
    }
    if (update_boundary3_pso_) {
        id<MTLComputePipelineState> pso =
                (__bridge_transfer id<MTLComputePipelineState>)update_boundary3_pso_;
        (void)pso;
        update_boundary3_pso_ = nullptr;
    }
    if (update_boundary2_pso_) {
        id<MTLComputePipelineState> pso =
                (__bridge_transfer id<MTLComputePipelineState>)update_boundary2_pso_;
        (void)pso;
        update_boundary2_pso_ = nullptr;
    }
    if (update_boundary1_pso_) {
        id<MTLComputePipelineState> pso =
                (__bridge_transfer id<MTLComputePipelineState>)update_boundary1_pso_;
        (void)pso;
        update_boundary1_pso_ = nullptr;
    }
    if (condensed_waveguide_pso_) {
        id<MTLComputePipelineState> pso =
                (__bridge_transfer id<MTLComputePipelineState>)condensed_waveguide_pso_;
        (void)pso;
        condensed_waveguide_pso_ = nullptr;
    }
    if (zero_buffer_pso_) {
        id<MTLComputePipelineState> pso =
                (__bridge_transfer id<MTLComputePipelineState>)zero_buffer_pso_;
        (void)pso;
        zero_buffer_pso_ = nullptr;
    }
    if (fill_zero_pso_) {
        id<MTLComputePipelineState> pso =
                (__bridge_transfer id<MTLComputePipelineState>)fill_zero_pso_;
        (void)pso;
        fill_zero_pso_ = nullptr;
    }
    if (library_) {
       id<MTLLibrary> lib = (__bridge_transfer id<MTLLibrary>)library_;
       (void)lib;
       library_ = nullptr;
   }
#endif
}

id<MTLDevice> waveguide_pipeline::device() const {
#if defined(__APPLE__)
    return (__bridge id<MTLDevice>)device_;
#else
    return nil;
#endif
}

id<MTLCommandQueue> waveguide_pipeline::command_queue() const {
#if defined(__APPLE__)
    return (__bridge id<MTLCommandQueue>)queue_;
#else
    return nil;
#endif
}

id<MTLComputePipelineState> waveguide_pipeline::zero_buffer_pso() const {
#if defined(__APPLE__)
    return (__bridge id<MTLComputePipelineState>)zero_buffer_pso_;
#else
    return nil;
#endif
}

id<MTLComputePipelineState> waveguide_pipeline::condensed_waveguide_pso() const {
#if defined(__APPLE__)
    return (__bridge id<MTLComputePipelineState>)condensed_waveguide_pso_;
#else
    return nil;
#endif
}

id<MTLComputePipelineState> waveguide_pipeline::update_boundary_pso(int dims) const {
#if defined(__APPLE__)
    switch (dims) {
        case 1: return (__bridge id<MTLComputePipelineState>)update_boundary1_pso_;
        case 2: return (__bridge id<MTLComputePipelineState>)update_boundary2_pso_;
        case 3: return (__bridge id<MTLComputePipelineState>)update_boundary3_pso_;
        default: return nil;
    }
#else
    (void)dims;
    return nil;
#endif
}

id<MTLComputePipelineState> waveguide_pipeline::layout_probe_pso() const {
#if defined(__APPLE__)
    return (__bridge id<MTLComputePipelineState>)layout_probe_pso_;
#else
    return nil;
#endif
}

id<MTLComputePipelineState> waveguide_pipeline::probe_previous_pso() const {
#if defined(__APPLE__)
    return (__bridge id<MTLComputePipelineState>)probe_previous_pso_;
#else
    return nil;
#endif
}

double waveguide_pipeline::fill_zero(std::size_t count) const {
#if defined(__APPLE__)
    if (!device_ || !queue_ || !fill_zero_pso_) return -1.0;
    id<MTLDevice> dev = (__bridge id<MTLDevice>)device_;
    id<MTLCommandQueue> q = (__bridge id<MTLCommandQueue>)queue_;
    id<MTLComputePipelineState> pso =
            (__bridge id<MTLComputePipelineState>)fill_zero_pso_;

    id<MTLBuffer> out = [dev newBufferWithLength:count * sizeof(float)
                                          options:MTLResourceStorageModePrivate];
    struct Params { uint32_t count; } params{ static_cast<uint32_t>(count) };
    id<MTLBuffer> paramBuf = [dev newBufferWithBytes:&params length:sizeof(params) options:MTLResourceStorageModeShared];

    id<MTLCommandBuffer> cb = [q commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:pso];
    [enc setBuffer:out offset:0 atIndex:0];
    [enc setBuffer:paramBuf offset:0 atIndex:1];

    const NSUInteger tgs = 256;
    NSUInteger grid = ((NSUInteger)count + tgs - 1) / tgs * tgs;
    MTLSize tgCount = MTLSizeMake(tgs, 1, 1);
    MTLSize tgGrid  = MTLSizeMake(grid, 1, 1);
    [enc dispatchThreads:tgGrid threadsPerThreadgroup:tgCount];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];

    if ([cb error]) return -2.0;
    if (@available(macOS 10.15, *)) {
        if ([cb respondsToSelector:@selector(GPUStartTime)] &&
            [cb respondsToSelector:@selector(GPUEndTime)]) {
            const double st = [cb GPUStartTime];
            const double et = [cb GPUEndTime];
            if (st > 0 && et > 0 && et >= st) return (et - st) * 1000.0;
        }
    }
    return 0.0;
#else
    return -1.0;
#endif
}

bool waveguide_pipeline::query_device_layout(waveguide::layout_info& out) const {
#if defined(__APPLE__)
    if (!device_ || !queue_ || !layout_probe_pso_) return false;

    id<MTLDevice> dev = (__bridge id<MTLDevice>)device_;
    id<MTLCommandQueue> q = (__bridge id<MTLCommandQueue>)queue_;
    id<MTLComputePipelineState> pso =
            (__bridge id<MTLComputePipelineState>)layout_probe_pso_;

    id<MTLBuffer> out_buf =
            [dev newBufferWithLength:sizeof(waveguide::layout_info)
                              options:MTLResourceStorageModeShared];
    if (!out_buf) {
        std::cerr << "[metal] failed to allocate layout probe buffer\n";
        return false;
    }

    id<MTLCommandBuffer> cb = [q commandBuffer];
    if (!cb) {
        std::cerr << "[metal] failed to create command buffer for layout probe\n";
        return false;
    }

    id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:pso];
    [enc setBuffer:out_buf offset:0 atIndex:0];
    const MTLSize one = MTLSizeMake(1, 1, 1);
    [enc dispatchThreads:one threadsPerThreadgroup:one];
    [enc endEncoding];

    [cb commit];
    [cb waitUntilCompleted];

    if ([cb error]) {
        std::cerr << "[metal] layout probe command buffer failed: "
                  << [[[cb error] localizedDescription] UTF8String] << "\n";
        return false;
    }

    std::memcpy(&out, [out_buf contents], sizeof(out));
    return true;
#else
    (void)out;
    return false;
#endif
}

bool waveguide_pipeline::validate_layout() const {
#if defined(__APPLE__)
    waveguide::layout_info device_info{};
    if (!query_device_layout(device_info)) {
        std::cerr << "[metal] layout validation unavailable (probe failed)\n";
        return false;
    }

    const auto host_info = waveguide::make_host_layout_info();
    bool ok = true;

    const auto check = [&](const char* name, std::uint32_t a, std::uint32_t b) {
        if (a != b) {
            std::cerr << "[metal] layout mismatch: " << name
                      << " device=" << a << " host=" << b << "\n";
            ok = false;
        }
    };

    check("sz_memory_canonical",
          device_info.sz_memory_canonical,
          host_info.sz_memory_canonical);
    check("sz_coefficients_canonical",
          device_info.sz_coefficients_canonical,
          host_info.sz_coefficients_canonical);
    check("sz_boundary_data",
          device_info.sz_boundary_data,
          host_info.sz_boundary_data);
    check("sz_boundary_data_array_3",
          device_info.sz_boundary_data_array_3,
          host_info.sz_boundary_data_array_3);
    check("off_bd_filter_memory",
          device_info.off_bd_filter_memory,
          host_info.off_bd_filter_memory);
    check("off_bd_coefficient_index",
          device_info.off_bd_coefficient_index,
          host_info.off_bd_coefficient_index);
    check("off_b3_data0",
          device_info.off_b3_data0,
          host_info.off_b3_data0);
    check("off_b3_data1",
          device_info.off_b3_data1,
          host_info.off_b3_data1);
    check("off_b3_data2",
          device_info.off_b3_data2,
          host_info.off_b3_data2);

    if (ok) {
        std::cerr << "[metal] layout validation succeeded\n";
    }
    return ok;
#else
    (void)sizeof(*this);
    return false;
#endif
}

} // namespace metal
} // namespace wayverb
