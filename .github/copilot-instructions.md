# GitHub Copilot Instructions for Phiverb/Wayverb

This repository contains Phiverb (a fork of Wayverb), a hybrid waveguide and ray-tracing room acoustic simulator with GPU acceleration, focusing on Apple Silicon optimization with Metal backend.

## Quick Reference

- **Primary Language**: C++ (40.9%), Objective-C++ (1.8% for Metal), Python (4% for tools)
- **Build System**: CMake + Xcode
- **Target Platform**: macOS (Apple Silicon priority, with OpenCL fallback)
- **Key Technologies**: Metal API, OpenCL, FDTD acoustic simulation, ray tracing, audio DSP

## Essential Reading

Before making changes, review:
1. `/AGENTS.md` - Complete agent operating instructions for this repository
2. `/docs/APPLE_SILICON.md` - Apple Silicon build instructions and status
3. `/docs/metal_port_plan.md` - Metal backend porting plan
4. `/readme.md` - Project overview and usage

## Core Principles

1. **Minimal Changes**: Make the smallest possible modifications to achieve the goal
2. **Preserve Stability**: Never break the OpenCL backend while adding Metal features
3. **Metal Feature Flag**: All Metal work must be behind `WAYVERB_ENABLE_METAL` flag
4. **Rich Logging**: Maintain detailed logging for offline debugging

## Build & Test Workflow

### Building
```bash
# Standard build
mkdir -p build && cd build
cmake .. -DWAYVERB_ENABLE_METAL=ON
make -j$(sysctl -n hw.ncpu)

# Or use Xcode
open wayverb/Builds/MacOS/wayverb.xcodeproj
```

### Testing
```bash
# Run tests
cd build && ctest

# Apple Silicon regression tests
./bin/apple_silicon_regression --scene geometri_wayverb/example.obj

# UI testing with logs
./tools/run_wayverb.sh  # Logs go to build/logs/app/wayverb-*.log
```

### Validation
- Always run tests after code changes
- Use `scripts/monitor_app_log.sh` to watch diagnostics
- Capture build provenance (git describe, branch, timestamp) in logs

## Key Environment Variables

### Runtime Configuration
- `WAYVERB_METAL=1` - Enable Metal backend (if compiled with Metal support)
- `WAYVERB_METAL=force-opencl` - Force OpenCL fallback
- `WAYVERB_DISABLE_VIZ=1` - Disable GPU→CPU pressure readbacks
- `WAYVERB_VIZ_DECIMATE=N` - Update pressures every N steps
- `WAYVERB_VOXEL_PAD=<int>` - Adjust voxel padding (default: 5)
- `WAYVERB_LOG_DIR` - Override log directory location

## Project Structure

```
phiverb/
├── src/
│   ├── core/           # Generic utilities, DSP helpers
│   ├── raytracer/      # Geometric acoustics (needs Metal porting)
│   ├── waveguide/      # FDTD simulation
│   ├── combined/       # Hybrid renderer
│   ├── metal/          # Metal backend implementation (in progress)
│   └── frequency_domain/ # FFT and filtering
├── wayverb/            # JUCE-based GUI application
├── bin/                # Command-line tools for testing
├── tests/              # Test suite
├── tools/              # Helper scripts (run_wayverb.sh, etc.)
├── scripts/            # Python/Octave prototypes, monitoring tools
└── docs/               # Documentation
```

## Coding Guidelines

### Style
- Follow existing code conventions in each file
- Use `.clang-format` for C++ formatting
- Keep comments consistent with existing style
- Avoid adding comments unless necessary for complex logic

### Security
- Never commit secrets or credentials
- Validate all mesh input files (must be watertight solids)
- Check for buffer overflows in audio processing
- Use Metal's validation layer during development

### Performance
- Profile changes with Xcode Instruments (Metal System Trace)
- Use `half` precision in Metal shaders when appropriate
- Leverage unified memory architecture on Apple Silicon
- Keep GPU compute kernels simple and well-commented

## Common Tasks

### Adding New Features
1. Add feature flag if it's experimental
2. Update documentation in relevant files
3. Add tests in `tests/` directory
4. Run full test suite before committing
5. Update AGENTS.md if workflow changes

### Metal Backend Work
1. Always preserve OpenCL fallback path
2. Test on actual Apple Silicon hardware
3. Use shared storage mode for CPU-GPU buffers
4. Follow Metal best practices in existing Metal code
5. Update `docs/metal_port_plan.md` status

### Debugging Rendering Issues
1. Launch via `tools/run_wayverb.sh` for proper logging
2. Check `build/logs/app/wayverb-*.log` for diagnostics
3. Use `bin/apple_silicon_regression` for headless testing
4. Validate mesh files are watertight (no holes, proper normals)
5. Check for NaN/Inf in audio output

## File Change Guidelines

### Never Modify Without Good Reason
- Working test files (unless fixing bugs in your changes)
- Build configuration (unless required for your feature)
- Third-party dependencies
- Documentation (unless directly related to your changes)

### Always Update When Changing
- Related tests when modifying functionality
- Documentation when changing behavior or adding features
- AGENTS.md when workflow or tools change
- Version/build provenance if build system changes

## Custom Agents

This repository has specialized agents in `.github/agents/`:
- **phiverb-metal-assistant**: Metal API expertise, audio DSP, performance optimization

Use custom agents for:
- Metal shader development and debugging
- Audio DSP implementation
- Performance profiling and optimization
- Mesh preprocessing issues

## Research Policy

For complex technical questions (Metal APIs, waveguide numerics, FDTD algorithms):
1. Check existing documentation first (`docs/`, `AGENTS.md`)
2. If needed, request maintainer to fetch external research via "@creator"
3. Wait for response before proceeding on critical paths
4. Document findings in relevant documentation files

## Commit Guidelines

- Use conventional commit format: `type(scope): description`
- Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`
- Keep commits focused and atomic
- Include issue/PR references where applicable
- Build provenance is automatic via CMake

## Testing Requirements

### Before Committing
- [ ] Code compiles without warnings
- [ ] Existing tests pass (`ctest`)
- [ ] New features have corresponding tests
- [ ] No security vulnerabilities introduced
- [ ] Documentation updated if behavior changed

### For Metal Changes
- [ ] Works with Metal backend enabled
- [ ] Falls back cleanly to OpenCL
- [ ] Performance comparable or better
- [ ] No memory leaks (use Instruments)

## Merge Blockers

Do not merge if:
- Any tests failing (unless unrelated and documented)
- Security vulnerabilities present
- OpenCL fallback broken
- Documentation out of date with changes
- Build provenance not captured
- Files in `.codex/NEED-INFO/` present (indicates blocked on external info)

## Getting Help

1. Review `AGENTS.md` for detailed guidance
2. Check `.github/agents/README.md` for custom agent usage
3. Use custom agents for specialized tasks
4. Ask maintainer via "@creator" for blockers

## References

- GitHub Copilot Best Practices: https://gh.io/copilot-coding-agent-tips
- Repository Agent Instructions: `/AGENTS.md`
- Apple Silicon Status: `/docs/APPLE_SILICON.md`
- Metal Port Plan: `/docs/metal_port_plan.md`
