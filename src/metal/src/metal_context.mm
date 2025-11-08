#if defined(__APPLE__)
#  import <Metal/Metal.h>
#  import <Foundation/Foundation.h>
#endif

#include "metal/metal_context.h"
#include <utility>

namespace wayverb {
namespace metal {

context::context() {
#if defined(__APPLE__)
    @autoreleasepool {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (dev) {
            id<MTLCommandQueue> q = [dev newCommandQueue];
            device = (__bridge_retained void*)dev;
            command_queue = (__bridge_retained void*)q;
        }
    }
#endif
}

context::~context() {
#if defined(__APPLE__)
    if (command_queue) {
        id<MTLCommandQueue> q = (__bridge_transfer id<MTLCommandQueue>)command_queue;
        (void)q;
        command_queue = nullptr;
    }
    if (device) {
        id<MTLDevice> d = (__bridge_transfer id<MTLDevice>)device;
        (void)d;
        device = nullptr;
    }
#endif
}

context::context(context&& o) noexcept { *this = std::move(o); }
context& context::operator=(context&& o) noexcept {
    if (this != &o) {
        device = o.device; o.device = nullptr;
        command_queue = o.command_queue; o.command_queue = nullptr;
    }
    return *this;
}

bool context::valid() const noexcept { return device != nullptr && command_queue != nullptr; }

} // namespace metal
} // namespace wayverb
