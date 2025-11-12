# Usage Examples for New Utilities

This document provides examples of how to use the new numerical safety, geometry validation, and SIMD utilities added to the project.

## 1. Numerical Safety (waveguide/numerical_safety.h)

### Safe Division
```cpp
#include "waveguide/numerical_safety.h"

using wayverb::waveguide::numerical_safety;

// Avoid division by zero
float result = numerical_safety::safe_divide(10.0f, 0.0f);  // Returns 0.0f instead of Inf

// Returns finite result
float safe_result = numerical_safety::safe_divide(10.0f, 2.0f);  // Returns 5.0f
```

### Safe Trigonometry
```cpp
// Avoid singularities in sine calculations
float angle = 0.0f;  // Problematic angle
float safe_sin = numerical_safety::safe_sin(angle);  // Returns sin(epsilon) instead of sin(0)
```

### Sanitize Values
```cpp
// Replace NaN/Inf with safe defaults
float bad_value = std::numeric_limits<float>::quiet_NaN();
float sanitized = numerical_safety::sanitize(bad_value);  // Returns 0.0f

float custom_default = numerical_safety::sanitize(bad_value, 1.0f);  // Returns 1.0f
```

### Sanitize Reflection Coefficients
```cpp
// Clamp reflection coefficients to valid range [-0.999, 0.999]
float coeff = 1.5f;  // Out of valid range
float safe_coeff = numerical_safety::sanitize_reflection_coefficient(coeff);  // Returns 0.999f
```

### Sanitize Pressure Fields
```cpp
// Clean up an entire pressure field array
std::vector<float> pressure_field(1000);
// ... pressure_field may contain NaN/Inf values ...

size_t nan_count = numerical_safety::sanitize_pressure_field(
    pressure_field.data(), 
    pressure_field.size()
);

std::cout << "Found and fixed " << nan_count << " NaN/Inf values" << std::endl;
```

## 2. Geometry Validation (core/geometry_validator.h)

### Validate a Mesh
```cpp
#include "core/geometry_validator.h"
#include "core/scene_data.h"

using wayverb::core::geometry_validator;
using wayverb::core::generic_scene_data;

// Assuming you have a scene_data object
generic_scene_data<cl_float3, surface<simulation_bands>> scene = ...;

// Validate the geometry
auto report = geometry_validator::validate(scene);

if (!report.is_valid) {
    std::cout << "Geometry validation failed!" << std::endl;
    
    // Print errors
    for (const auto& error : report.errors) {
        std::cerr << "ERROR: " << error << std::endl;
    }
    
    // Print warnings
    for (const auto& warning : report.warnings) {
        std::cout << "WARNING: " << warning << std::endl;
    }
    
    // Check specific issues
    if (report.degenerate_triangles > 0) {
        std::cout << "Found " << report.degenerate_triangles 
                  << " degenerate triangles" << std::endl;
    }
} else {
    std::cout << "Geometry is valid!" << std::endl;
}
```

### Using in a Pipeline
```cpp
// Validate before processing
auto validation_result = geometry_validator::validate(scene);

if (!validation_result.is_valid) {
    throw std::runtime_error("Invalid geometry: " + 
        validation_result.errors.front());
}

// Proceed with simulation...
```

## 3. Apple Silicon SIMD (core/simd_apple.h)

This header is only available on Apple Silicon (ARM64) platforms.

### Process Pressure Field with SIMD
```cpp
#ifdef __APPLE__
#if TARGET_CPU_ARM64

#include "core/simd_apple.h"

using wayverb::core::simd_apple;

// Apply damping to a pressure field using SIMD
std::vector<float> pressure_field(10000);
// ... initialize pressure_field ...

float damping_factor = 0.99f;
simd_apple::process_pressure_field_simd(
    pressure_field.data(),
    pressure_field.size(),
    damping_factor
);

#endif  // TARGET_CPU_ARM64
#endif  // __APPLE__
```

### Add Scalar with SIMD
```cpp
// Add a scalar value to all elements using SIMD
std::vector<float> data(10000);
float offset = 1.0f;

simd_apple::add_scalar_simd(data.data(), data.size(), offset);
```

### Manual SIMD Operations
```cpp
// Low-level SIMD operations
float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
float output[4];

auto vec = simd_apple::load(input);
auto doubled = simd_apple::mul(vec, simd_apple::load((float[]){2.0f, 2.0f, 2.0f, 2.0f}));
simd_apple::store(output, doubled);

// Sum reduction
float total = simd_apple::sum(vec);  // Returns 10.0f
```

## Integration Examples

### Complete Waveguide Processing with Safety
```cpp
#include "waveguide/numerical_safety.h"
#include "core/geometry_validator.h"

using namespace wayverb;

// 1. Validate geometry before starting
auto validation = core::geometry_validator::validate(scene);
if (!validation.is_valid) {
    throw std::runtime_error("Invalid geometry");
}

// 2. During waveguide processing, sanitize pressure values
std::vector<float> pressures = ...;
size_t bad_values = waveguide::numerical_safety::sanitize_pressure_field(
    pressures.data(), pressures.size()
);

if (bad_values > 0) {
    std::cerr << "Warning: Sanitized " << bad_values << " bad pressure values" << std::endl;
}

// 3. When computing boundary coefficients, use safe division
float impedance_ratio = waveguide::numerical_safety::safe_divide(Z1, Z2);
float reflection = waveguide::numerical_safety::sanitize_reflection_coefficient(
    (impedance_ratio - 1.0f) / (impedance_ratio + 1.0f)
);
```

## Performance Considerations

### SIMD Performance
- The SIMD functions automatically handle alignment and remainder elements
- Best performance when array size is a multiple of 4
- On Apple Silicon, NEON provides significant speedup for array operations

### Numerical Safety Performance
- `sanitize_pressure_field` has minimal overhead (single pass through array)
- Individual sanitize calls are inlined and very fast
- Use `safe_divide` and `safe_sin` only where necessary to avoid performance impact

### Geometry Validation
- Run validation once during scene setup, not per-frame
- Validation is O(n) where n is the number of triangles
- Use warnings to detect potential performance issues before they occur
