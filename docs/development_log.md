# Wayverb Apple Silicon Migration Log

This file tracks iterative progress while porting and validating Wayverb on Apple Silicon Macs. New entries should be appended to the top so the latest work is easiest to find.

## 2025-03-08

- Forced OpenCL single-precision on Apple Silicon via `WAYVERB_FORCE_SINGLE_PRECISION`, guarded JUCE inline asm, and ensured the Metal sample kernel is linked and invoked safely at startup.
- Added a minimal Metal compute smoke test (`wayverb/Source/metal/MetalSample.*`) and wired it into the macOS app.
- Removed legacy QTKit linkage and linked Metal.framework in the Xcode project to fix linker issues on arm64.
- Copied reference geometry `pyramid_twisted_minor` into `assets/test_geometry/` for repeatable scene tests.
- Verified `wayverb (App)` builds with `xcodebuild` and launches cleanly from the shell with `CL_LOG_ERRORS=stdout`.

