#include "waveguide/program.h"

#include "gtest/gtest.h"

using namespace wayverb::waveguide;
using namespace wayverb::core;

TEST(build_program, waveguide) {
    bool initialized_device = false;
    for (const auto& dev : {device_type::cpu, device_type::gpu}) {
        try {
            const compute_context cc{dev};
            ASSERT_NO_THROW(program{cc});
            initialized_device = true;
        } catch (const std::exception& e) {
            GTEST_LOG_(WARNING) << "Skipping device " << static_cast<int>(dev)
                                << " (" << e.what() << ')';
        }
    }
    if (!initialized_device) {
        GTEST_LOG_(WARNING)
                << "No usable OpenCL device available for program test; skipping.";
    }
}
