# Dependency Management Migration

## Overview

This document describes the migration from vendored/ExternalProject-based dependencies to CPM.cmake (CMake Package Manager).

## What Changed

### Before
- Dependencies were downloaded at build time using CMake's `ExternalProject_Add`
- Manual management of dependency targets and install directories
- Complex configuration with many global variables
- Each dependency required explicit `add_dependencies()` calls

### After
- Most dependencies now use **CPM.cmake** for automatic management
- Simplified dependency declarations
- Automatic dependency resolution
- Built-in caching for faster subsequent builds
- No manual `add_dependencies()` calls needed for CPM-managed libraries

## CPM-Managed Dependencies

The following dependencies are now managed by CPM:

1. **glm** (0.9.8.5) - Header-only OpenGL Mathematics library
2. **assimp** (v5.4.3) - 3D model loading library
3. **googletest** (v1.14.0) - Testing framework
4. **cereal** (v1.3.2) - Serialization library (header-only)
5. **OpenCL-Headers** (v2023.12.14) - OpenCL C headers
6. **OpenCL-CLHPP** (v2023.12.14) - OpenCL C++ bindings
7. **modern_gl_utils** (master) - OpenGL utility library

## Still Using ExternalProject

These dependencies continue to use `ExternalProject_Add` because they use autotools instead of CMake:

1. **fftw3** (float and double) - Fast Fourier Transform library
2. **libsndfile** - Audio file I/O library
3. **libsamplerate** - Sample rate conversion library
4. **itpp** - IT++ mathematical library

## Benefits of CPM

1. **Simpler Configuration**: Clean, declarative dependency specifications
2. **Caching**: Dependencies cached in `build/.cpm-cache` for faster rebuilds
3. **Version Control**: Explicit version tags make dependency versions clear
4. **Reproducible Builds**: Same versions across different machines
5. **Less Boilerplate**: No need for manual `add_dependencies()` calls
6. **Better Integration**: CPM integrates seamlessly with CMake's FetchContent

## Migration Guide

### Adding New CPM Dependencies

To add a new dependency, use `CPMAddPackage` in `config/dependencies.cmake`:

```cmake
CPMAddPackage(
    NAME your_library
    GITHUB_REPOSITORY organization/repository
    GIT_TAG v1.0.0
    OPTIONS
        "BUILD_TESTS OFF"
        "BUILD_EXAMPLES OFF"
)
```

### Cache Management

CPM caches downloaded dependencies in `build/.cpm-cache`. To use a custom cache location:

```bash
export CPM_SOURCE_CACHE=/path/to/cache
cmake -S . -B build
```

Or set it in CMake:

```bash
cmake -S . -B build -DCPM_SOURCE_CACHE=/path/to/cache
```

### Cleaning Dependencies

To force CPM to re-download dependencies:

```bash
rm -rf build/.cpm-cache
```

## Resources

- [CPM.cmake Documentation](https://github.com/cpm-cmake/CPM.cmake)
- [CPM.cmake Best Practices](https://github.com/cpm-cmake/CPM.cmake/wiki/More-Snippets)
