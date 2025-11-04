#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#endif

#include "../../JuceLibraryCode/JuceHeader.h"

#if JUCE_MAC

#include <array>

#include "MetalSample.h"

namespace wayverb::metal {

namespace {

inline void logNSError(const char* context, NSError* error) {
    if (error != nil) {
        juce::Logger::writeToLog(juce::String(context) + ": " +
                                 juce::String([error.localizedDescription UTF8String]));
    }
}

}  // namespace

bool run_sample_kernel() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            juce::Logger::writeToLog("Metal sample: no default device available.");
            return false;
        }

        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (queue == nil) {
            juce::Logger::writeToLog("Metal sample: failed to create command queue.");
            return false;
        }

        static NSString* const kSource = @"using namespace metal;"
                                         "kernel void add_one(device float* data [[buffer(0)]],"
                                         "                    uint tid [[thread_position_in_grid]]) {"
                                         "    data[tid] = data[tid] + 1.0f;"
                                         "}";

        NSError* error = nil;
        MTLCompileOptions* options = [MTLCompileOptions new];
        id<MTLLibrary> library = [device newLibraryWithSource:kSource
                                                      options:options
                                                        error:&error];
        if (library == nil) {
            logNSError("Metal sample: failed to compile library", error);
            return false;
        }

        id<MTLFunction> function = [library newFunctionWithName:@"add_one"];
        if (function == nil) {
            juce::Logger::writeToLog("Metal sample: failed to find Metal function.");
            return false;
        }

        id<MTLComputePipelineState> pipeline =
                [device newComputePipelineStateWithFunction:function error:&error];
        if (pipeline == nil) {
            logNSError("Metal sample: failed to create pipeline state", error);
            return false;
        }

        std::array<float, 4> values{0.0f, 1.0f, 2.0f, 3.0f};
        id<MTLBuffer> buffer = [device newBufferWithBytes:values.data()
                                                   length:values.size() * sizeof(float)
                                                  options:MTLResourceStorageModeShared];
        if (buffer == nil) {
            juce::Logger::writeToLog("Metal sample: failed to create buffer.");
            return false;
        }

        id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
        [encoder setComputePipelineState:pipeline];
        [encoder setBuffer:buffer offset:0 atIndex:0];

        const NSUInteger gridSize = values.size();
        const NSUInteger threadGroupSize = std::min<NSUInteger>(pipeline.maxTotalThreadsPerThreadgroup, gridSize);
        MTLSize threadsPerThreadgroup = MTLSizeMake(threadGroupSize, 1, 1);
        MTLSize threadsPerGrid = MTLSizeMake(gridSize, 1, 1);
        [encoder dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
        [encoder endEncoding];

        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];

        float* result = static_cast<float*>(buffer.contents);
        juce::String message = "Metal sample results: ";
        for (NSUInteger i = 0; i < gridSize; ++i) {
            message += juce::String(result[i]) + (i + 1 == gridSize ? "" : ", ");
        }
        juce::Logger::writeToLog(message);

        return true;
    }
}

}  // namespace wayverb::metal

#endif  // JUCE_MAC
