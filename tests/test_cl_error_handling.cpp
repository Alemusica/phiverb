// Test for OpenCL error handling utilities

#include "core/cl/cl_error.h"
#include "core/cl/cl_check.h"
#include <iostream>
#include <string>

using namespace wayverb::core;

int main() {
    bool all_passed = true;
    
    // Test 1: Error string conversion for common errors
    {
        const char* success_str = get_cl_error_string(CL_SUCCESS);
        if (std::string(success_str) == "CL_SUCCESS") {
            std::cout << "✓ CL_SUCCESS string conversion works" << std::endl;
        } else {
            std::cerr << "✗ CL_SUCCESS string conversion failed: got '" 
                     << success_str << "'" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 2: Device not found error
    {
        const char* error_str = get_cl_error_string(CL_DEVICE_NOT_FOUND);
        if (std::string(error_str) == "CL_DEVICE_NOT_FOUND") {
            std::cout << "✓ CL_DEVICE_NOT_FOUND string conversion works" << std::endl;
        } else {
            std::cerr << "✗ CL_DEVICE_NOT_FOUND string conversion failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 3: Invalid value error
    {
        const char* error_str = get_cl_error_string(CL_INVALID_VALUE);
        if (std::string(error_str) == "CL_INVALID_VALUE") {
            std::cout << "✓ CL_INVALID_VALUE string conversion works" << std::endl;
        } else {
            std::cerr << "✗ CL_INVALID_VALUE string conversion failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 4: Out of resources error
    {
        const char* error_str = get_cl_error_string(CL_OUT_OF_RESOURCES);
        if (std::string(error_str) == "CL_OUT_OF_RESOURCES") {
            std::cout << "✓ CL_OUT_OF_RESOURCES string conversion works" << std::endl;
        } else {
            std::cerr << "✗ CL_OUT_OF_RESOURCES string conversion failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 5: Build program failure
    {
        const char* error_str = get_cl_error_string(CL_BUILD_PROGRAM_FAILURE);
        if (std::string(error_str) == "CL_BUILD_PROGRAM_FAILURE") {
            std::cout << "✓ CL_BUILD_PROGRAM_FAILURE string conversion works" << std::endl;
        } else {
            std::cerr << "✗ CL_BUILD_PROGRAM_FAILURE string conversion failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 6: Unknown error code
    {
        const char* error_str = get_cl_error_string(-9999);
        if (std::string(error_str) == "CL_UNKNOWN_ERROR") {
            std::cout << "✓ Unknown error code returns CL_UNKNOWN_ERROR" << std::endl;
        } else {
            std::cerr << "✗ Unknown error code handling failed" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 7: check_cl_error with success (should not throw)
    {
        try {
            check_cl_error(CL_SUCCESS, "test_call()", __FILE__, __LINE__);
            std::cout << "✓ check_cl_error does not throw on CL_SUCCESS" << std::endl;
        } catch (...) {
            std::cerr << "✗ check_cl_error threw on CL_SUCCESS" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 8: check_cl_error with error (should throw)
    {
        try {
            check_cl_error(CL_INVALID_VALUE, "test_call()", __FILE__, __LINE__);
            std::cerr << "✗ check_cl_error did not throw on error" << std::endl;
            all_passed = false;
        } catch (const cl_exception& e) {
            std::string msg(e.what());
            if (msg.find("CL_INVALID_VALUE") != std::string::npos &&
                msg.find("test_call()") != std::string::npos &&
                e.error_code() == CL_INVALID_VALUE) {
                std::cout << "✓ check_cl_error throws with detailed message" << std::endl;
            } else {
                std::cerr << "✗ check_cl_error message incomplete: " << msg << std::endl;
                all_passed = false;
            }
        } catch (...) {
            std::cerr << "✗ check_cl_error threw wrong exception type" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 9: CL_CHECK macro with success
    {
        try {
            cl_int success_code = CL_SUCCESS;
            CL_CHECK(success_code);
            std::cout << "✓ CL_CHECK macro works with success" << std::endl;
        } catch (...) {
            std::cerr << "✗ CL_CHECK threw on success" << std::endl;
            all_passed = false;
        }
    }
    
    // Test 10: CL_CHECK macro with error
    {
        try {
            cl_int error_code = CL_INVALID_KERNEL;
            CL_CHECK(error_code);
            std::cerr << "✗ CL_CHECK did not throw on error" << std::endl;
            all_passed = false;
        } catch (const cl_exception& e) {
            std::string msg(e.what());
            if (msg.find("CL_INVALID_KERNEL") != std::string::npos &&
                e.error_code() == CL_INVALID_KERNEL) {
                std::cout << "✓ CL_CHECK macro throws with error details" << std::endl;
            } else {
                std::cerr << "✗ CL_CHECK error message incomplete" << std::endl;
                all_passed = false;
            }
        } catch (...) {
            std::cerr << "✗ CL_CHECK threw wrong exception type" << std::endl;
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
