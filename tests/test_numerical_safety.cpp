// Simple tests for numerical safety utilities
// These tests demonstrate the API without requiring full build infrastructure

#include "waveguide/numerical_safety.h"
#include <cmath>
#include <iostream>
#include <vector>

using namespace wayverb::waveguide;

int main() {
    bool all_passed = true;
    
    // Test 1: Safe division by zero
    {
        float result = numerical_safety::safe_divide(10.0f, 0.0f);
        if (result == 0.0f) {
            std::cout << "✓ Safe division by zero returns 0" << std::endl;
        } else {
            std::cerr << "✗ Safe division by zero failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 2: Safe division normal case
    {
        float result = numerical_safety::safe_divide(10.0f, 2.0f);
        if (std::abs(result - 5.0f) < 1e-5f) {
            std::cout << "✓ Safe division normal case works" << std::endl;
        } else {
            std::cerr << "✗ Safe division normal case failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 3: Sanitize NaN
    {
        float nan_value = std::numeric_limits<float>::quiet_NaN();
        float result = numerical_safety::sanitize(nan_value);
        if (result == 0.0f) {
            std::cout << "✓ Sanitize NaN returns default value" << std::endl;
        } else {
            std::cerr << "✗ Sanitize NaN failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 4: Sanitize Inf
    {
        float inf_value = std::numeric_limits<float>::infinity();
        float result = numerical_safety::sanitize(inf_value, 1.0f);
        if (result == 1.0f) {
            std::cout << "✓ Sanitize Inf returns custom default" << std::endl;
        } else {
            std::cerr << "✗ Sanitize Inf failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 5: Sanitize reflection coefficient
    {
        float too_large = 1.5f;
        float result = numerical_safety::sanitize_reflection_coefficient(too_large);
        if (result == numerical_safety::max_coefficient()) {
            std::cout << "✓ Reflection coefficient clamped to max" << std::endl;
        } else {
            std::cerr << "✗ Reflection coefficient clamping failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 6: Sanitize pressure field
    {
        std::vector<float> field = {
            1.0f, 2.0f, 
            std::numeric_limits<float>::quiet_NaN(), 
            4.0f,
            std::numeric_limits<float>::infinity(),
            5.0f
        };
        
        size_t nan_count = numerical_safety::sanitize_pressure_field(
            field.data(), field.size()
        );
        
        if (nan_count == 2 && 
            field[0] == 1.0f && 
            field[1] == 2.0f &&
            field[2] == 0.0f &&  // NaN replaced
            field[3] == 4.0f &&
            field[4] == 0.0f &&  // Inf replaced
            field[5] == 5.0f) {
            std::cout << "✓ Pressure field sanitization works" << std::endl;
        } else {
            std::cerr << "✗ Pressure field sanitization failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 7: Safe sin
    {
        float result = numerical_safety::safe_sin(0.5f);
        if (std::isfinite(result)) {
            std::cout << "✓ Safe sin returns finite value" << std::endl;
        } else {
            std::cerr << "✗ Safe sin failed" << std::endl;
            all_passed = false;
        }
    }
    
    if (all_passed) {
        std::cout << "\n✓ All tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "\n✗ Some tests failed!" << std::endl;
        return 1;
    }
}
