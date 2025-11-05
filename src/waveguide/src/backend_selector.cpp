#include "waveguide/backend_selector.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>

namespace wayverb {
namespace waveguide {

waveguide_backend select_backend() {
    static const waveguide_backend backend = [] {
        if (const char* env = std::getenv("WAYVERB_WG_BACKEND")) {
            std::string value{env};
            std::transform(begin(value), end(value), begin(value), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (value == "bempp" || value == "bempp_cpu" || value == "bempp-cl") {
                std::cerr << "[waveguide] Selecting Bempp CPU backend (WAYVERB_WG_BACKEND="
                          << value << ")\n";
                return waveguide_backend::bempp_cpu;
            }
            if (value != "opencl") {
                std::cerr << "[waveguide] Unknown WAYVERB_WG_BACKEND value '" << value
                          << "'. Falling back to OpenCL backend.\n";
            }
        }
        return waveguide_backend::opencl;
    }();
    return backend;
}

}  // namespace waveguide
}  // namespace wayverb
