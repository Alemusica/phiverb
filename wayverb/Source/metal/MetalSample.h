#pragma once

#if JUCE_MAC

namespace wayverb::metal {

/** Runs a trivial Metal compute kernel to verify the GPU path works.
    Returns true on success. */
bool run_sample_kernel();

}  // namespace wayverb::metal

#endif  // JUCE_MAC

