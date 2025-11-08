#pragma once

#if defined(__APPLE__)
#  include <TargetConditionals.h>
#endif

namespace wayverb {
namespace metal {

// Minimal forward declarations to avoid including ObjC headers here.
struct context final {
    // Created only on Apple platforms; elsewhere remains inert.
    context();
    ~context();

    // non-copyable, movable
    context(const context&) = delete;
    context& operator=(const context&) = delete;
    context(context&&) noexcept;
    context& operator=(context&&) noexcept;

    // True if a Metal device/queue are available.
    bool valid() const noexcept;

    // Opaque pointers to avoid leaking ObjC types in headers.
    void* device = nullptr;       // id<MTLDevice>
    void* command_queue = nullptr; // id<MTLCommandQueue>
};

} // namespace metal
} // namespace wayverb

