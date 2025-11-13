#pragma once

#include "core/cl/traits.h"

#include <tuple>

namespace wayverb {
namespace raytracer {

struct alignas(1 << 4) reflection final {
    cl_float3 position;   //  position of the secondary source
    cl_uint triangle;     //  triangle which contains source
    cl_float scatter_probability;
    cl_char keep_going;   //  whether or not this is the terminator for this
                          //  path (like a \0 in a char*)
    cl_char receiver_visible;  //  whether or not the receiver is visible from
                               //  this point
    cl_char sampled_diffuse;   //  whether the outgoing direction was diffuse
    cl_char padding;           //  explicit padding to keep the following floats aligned
    cl_float sample_pdf;       //  pdf used for the sampled outgoing direction
    cl_float cos_theta;        //  abs(dot(normal, outgoing)) used for throughput
};

constexpr auto to_tuple(const reflection& x) {
    return std::tie(x.position,
                    x.triangle,
                    x.scatter_probability,
                    x.keep_going,
                    x.receiver_visible,
                    x.sampled_diffuse,
                    x.sample_pdf,
                    x.cos_theta);
}

constexpr bool operator==(const reflection& a, const reflection& b) {
    return to_tuple(a) == to_tuple(b);
}

constexpr bool operator!=(const reflection& a, const reflection& b) {
    return !(a == b);
}

}  // namespace raytracer
}  // namespace wayverb
