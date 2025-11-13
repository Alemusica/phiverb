#pragma once

#include "core/cl/cl_error.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace wayverb {
namespace core {

/// Custom OpenCL exception with detailed error information
class cl_exception : public std::runtime_error {
public:
    cl_exception(cl_int error_code, const std::string& message)
        : std::runtime_error(message)
        , error_code_(error_code) {}
    
    cl_int error_code() const { return error_code_; }
    
private:
    cl_int error_code_;
};

/// Check an OpenCL error code and throw an exception with detailed information
/// if the error is not CL_SUCCESS.
///
/// This function is designed to be used with the CL_CHECK macro, which
/// automatically captures file and line information.
inline void check_cl_error(cl_int error,
                          const char* call,
                          const char* file,
                          int line) {
    if (error != CL_SUCCESS) {
        std::ostringstream ss;
        ss << "[OpenCL Error] " << get_cl_error_string(error) 
           << " (" << error << ")\n"
           << "  at " << file << ":" << line << "\n"
           << "  in call: " << call;
        
        // Log to stderr for debugging
        std::string msg = ss.str();
        std::cerr << msg << std::endl;
        
        // Throw exception with detailed message
        throw cl_exception(error, msg);
    }
}

}  // namespace core
}  // namespace wayverb

/// Macro to check OpenCL error codes with automatic file/line capture.
/// 
/// Usage:
///   CL_CHECK(clEnqueueNDRangeKernel(...));
///   
/// This will check the return value and throw a detailed exception if it's
/// not CL_SUCCESS, including:
/// - The error name (e.g., "CL_INVALID_KERNEL_ARGS")
/// - The error code number
/// - The file and line where the error occurred
/// - The actual OpenCL call that failed
#define CL_CHECK(call) \
    do { \
        cl_int _cl_err = (call); \
        ::wayverb::core::check_cl_error(_cl_err, #call, __FILE__, __LINE__); \
    } while (0)
