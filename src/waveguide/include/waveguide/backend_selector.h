#pragma once

#include <string>

namespace wayverb {
namespace waveguide {

enum class waveguide_backend {
    opencl,
    bempp_cpu
};

/// Inspect environment/configuration and return the backend to use.
waveguide_backend select_backend();

inline const char* backend_name(waveguide_backend backend) {
    switch (backend) {
        case waveguide_backend::opencl: return "opencl";
        case waveguide_backend::bempp_cpu: return "bempp_cpu";
        default: return "unknown";
    }
}

}  // namespace waveguide
}  // namespace wayverb

