# OpenCL Error Handling

This document describes the improved OpenCL error handling system implemented in Wayverb.

## Overview

The new error handling system provides detailed, human-readable error messages for OpenCL API calls, making debugging significantly easier.

## Components

### 1. Error String Conversion (`cl_error.h`)

Converts OpenCL error codes to human-readable strings.

```cpp
#include "core/cl/cl_error.h"

const char* error_name = wayverb::core::get_cl_error_string(CL_INVALID_KERNEL);
// Returns: "CL_INVALID_KERNEL"
```

Supports all standard OpenCL error codes from versions 1.0 through 2.2.

### 2. Error Checking (`cl_check.h`)

Provides the `CL_CHECK()` macro and `cl_exception` class for automated error checking.

```cpp
#include "core/cl/cl_check.h"

// Check OpenCL return codes automatically
CL_CHECK(clEnqueueNDRangeKernel(...));

// If the call fails, you get:
// [OpenCL Error] CL_INVALID_KERNEL_ARGS (-52)
//   at src/waveguide/waveguide.cpp:342
//   in call: clEnqueueNDRangeKernel(...)
```

### 3. Custom Exception (`cl_exception`)

A custom exception class that extends `std::runtime_error` and stores the OpenCL error code:

```cpp
try {
    CL_CHECK(some_opencl_call());
} catch (const wayverb::core::cl_exception& e) {
    std::cerr << "Caught OpenCL error: " << e.what() << std::endl;
    std::cerr << "Error code: " << e.error_code() << std::endl;
}
```

## Usage

### Basic Usage

Instead of checking error codes manually:

```cpp
// Old way
cl_int err = clEnqueueNDRangeKernel(...);
if (err != CL_SUCCESS) {
    std::cerr << "OpenCL error: " << err << std::endl;
    throw std::runtime_error("Kernel execution failed");
}
```

Use the `CL_CHECK()` macro:

```cpp
// New way
CL_CHECK(clEnqueueNDRangeKernel(...));
```

### Integration with Existing Code

The error handling system is already integrated into:

1. **program_wrapper.h** - Kernel creation now provides detailed error messages
2. **waveguide.h** - Event callback errors now include error name strings

### Error Message Format

When an error occurs, you'll see:

```
[OpenCL Error] CL_INVALID_KERNEL_ARGS (-52)
  at src/waveguide/src/waveguide.cpp:342
  in call: clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global_size, nullptr, 0, nullptr, nullptr)
```

This includes:
- The error name (e.g., `CL_INVALID_KERNEL_ARGS`)
- The error code number (-52)
- The exact file and line where the error occurred
- The actual OpenCL call that failed

## Benefits

1. **Faster Debugging**: Immediately identify which OpenCL call failed and why
2. **Better Error Messages**: Human-readable error names instead of cryptic numbers
3. **Precise Location**: File and line information pinpoints the error source
4. **Minimal Overhead**: Zero overhead when no errors occur
5. **Easy Integration**: Simple macro-based interface

## Testing

Comprehensive tests are provided in `tests/test_cl_error_handling.cpp`:

```bash
cd build
make test_cl_error_handling
./tests/test_cl_error_handling
```

All tests should pass, validating:
- Error string conversion for all common error codes
- Exception throwing behavior
- Macro functionality
- Error code preservation

## Migration Guide

To update existing code:

1. Add include at the top of your file:
   ```cpp
   #include "core/cl/cl_check.h"
   ```

2. Wrap OpenCL C API calls with `CL_CHECK()`:
   ```cpp
   CL_CHECK(clEnqueueNDRangeKernel(...));
   ```

3. For OpenCL C++ API that returns error codes via out-parameter:
   ```cpp
   cl_int error;
   auto kernel = cl::Kernel(program, "kernel_name", &error);
   CL_CHECK(error);
   ```

4. Catch exceptions using `cl_exception`:
   ```cpp
   try {
       CL_CHECK(some_call());
   } catch (const wayverb::core::cl_exception& e) {
       // Handle error
   }
   ```

## Performance

- The error checking adds negligible overhead
- The string conversion only runs when an error occurs
- Release builds can disable exceptions if desired (not recommended for debugging)

## Compatibility

- Works with OpenCL 1.0 through 3.0
- Compatible with both OpenCL C and C++ APIs
- No changes required to existing error-free code paths
