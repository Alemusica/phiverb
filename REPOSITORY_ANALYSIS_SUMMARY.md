# Repository Analysis Summary

**Analysis Date:** November 10, 2025  
**Full Analysis (Italian):** See [ANALISI_REPOSITORY.md](./ANALISI_REPOSITORY.md)

## Quick Overview

This repository contains **Wayverb**, a hybrid waveguide and ray-tracing room acoustic simulator with GPU acceleration.

### What is Wayverb?

Wayverb produces **Room Impulse Responses (RIRs)** by combining:
- **Image-source** for high-frequency early reflections
- **Stochastic ray-tracing** for high-frequency late reflections  
- **Rectilinear waveguide mesh** for all low-frequency content

The output can be used with convolution reverbs to create realistic auralizations of virtual spaces.

### Current Status

- ✅ **Working version** on macOS with OpenCL
- 🚧 **Metal port** in progress for Apple Silicon (branch: `feature/metal-apple-silicon`)
- 📊 ~5,500 lines of C++ code
- 🎯 Focus: stability, performance, and diagnostics

### Key Components

```
src/
├── core/              - Generic utilities (DSP, data structures)
├── raytracer/         - Geometric acoustics components
├── waveguide/         - FDTD simulation
├── combined/          - Integration of raytracer + waveguide
├── metal/             - Metal backend (experimental)
└── ...

wayverb/               - JUCE GUI application
bin/                   - CLI tools for testing
docs/                  - Documentation
```

### Quick Start

**Build (OpenCL):**
```bash
./build.sh
```

**Build (Metal - Apple Silicon):**
```bash
tools/build_metal.sh
```

**Run with logging:**
```bash
tools/run_wayverb.sh > build/logs/app/wayverb-$(date +%Y%m%d-%H%M%S).log 2>&1
```

**Test regression:**
```bash
bin/apple_silicon_regression --scene test.obj
```

### Important Documentation

- **Main README:** [readme.md](./readme.md)
- **Apple Silicon Guide:** [docs/APPLE_SILICON.md](./docs/APPLE_SILICON.md)
- **Development Log:** [docs/development_log.md](./docs/development_log.md)
- **Action Plan:** [docs/action_plan.md](./docs/action_plan.md)
- **Agent Instructions:** [AGENTS.md](./AGENTS.md)
- **Full Analysis (IT):** [ANALISI_REPOSITORY.md](./ANALISI_REPOSITORY.md)

### Requirements

- **OS:** macOS 10.10+
- **GPU:** With double-precision support
- **Build:** Clang with C++14/C++17, CMake 3.0+

### Performance Tips

```bash
# Disable GPU→CPU visualization readbacks
WAYVERB_DISABLE_VIZ=1

# Update UI every N steps
WAYVERB_VIZ_DECIMATE=10

# Reduce voxel padding
WAYVERB_VOXEL_PAD=3
```

### Contributing

1. Read [AGENTS.md](./AGENTS.md) for operational guidelines
2. Follow [docs/action_plan.md](./docs/action_plan.md) for structured tasks
3. Use provided logging tools for debugging
4. Maintain OpenCL compatibility during Metal development
5. Test with validated watertight scenes

### Metal Backend Status

**Build flag:** `-DWAYVERB_ENABLE_METAL=ON`  
**Runtime:** `WAYVERB_METAL=1` (use Metal) or `WAYVERB_METAL=force-opencl` (force fallback)

**Current:** Scaffold complete, kernels translated to MSL, falls back to OpenCL  
**Next:** Complete runtime execution path, performance tuning

### License

Software provided "as is" without warranty. See [LICENSE](./LICENSE) for details.

---

For a comprehensive analysis in Italian (730 lines covering all aspects of the project), see [ANALISI_REPOSITORY.md](./ANALISI_REPOSITORY.md).
