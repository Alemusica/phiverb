#include "core/cl/common.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace wayverb {
namespace core {
namespace {

cl::Context get_context(device_type type) {
    std::vector<cl::Platform> platform;
    cl::Platform::get(&platform);

    cl_context_properties cps[] = {
            CL_CONTEXT_PLATFORM,
            reinterpret_cast<cl_context_properties>((platform.front())()),
            0,
    };

    const auto convert_device_type = [](auto type) {
        switch (type) {
            case device_type::cpu: return CL_DEVICE_TYPE_CPU;
            case device_type::gpu: return CL_DEVICE_TYPE_GPU;
        }
    };

    return cl::Context(convert_device_type(type), cps);
}

bool supports_double_precision(const cl::Device& device) {
    const auto fp_config = device.getInfo<CL_DEVICE_DOUBLE_FP_CONFIG>();
    const auto extensions = device.getInfo<CL_DEVICE_EXTENSIONS>();
    const auto has_extension = extensions.find("cl_khr_fp64") != std::string::npos ||
                               extensions.find("cl_APPLE_fp64_basic_ops") != std::string::npos;
    return fp_config != 0u || has_extension;
}

cl::Device get_device(const cl::Context& context) {
    auto devices = context.getInfo<CL_CONTEXT_DEVICES>();

    std::vector<cl::Device> available_devices;
    std::vector<cl::Device> fp64_devices;

    for (const auto& device : devices) {
        const auto available = device.getInfo<CL_DEVICE_AVAILABLE>() != 0u;
        if (!available) {
            std::cerr << "Skipping device \"" << device.getInfo<CL_DEVICE_NAME>()
                      << "\" - reported as unavailable.\n";
            continue;
        }

        const auto supports_fp64 = supports_double_precision(device);
        if (!supports_fp64) {
            std::cerr << "Device \"" << device.getInfo<CL_DEVICE_NAME>()
                      << "\" does not support double precision (fp64); falling back to single precision if selected.\n";
        }

        available_devices.push_back(device);
        if (supports_fp64) {
            fp64_devices.push_back(device);
        }
    }

    if (available_devices.empty()) {
        throw std::runtime_error("No suitable OpenCL devices available.");
    }

    const cl::Device* chosen = nullptr;
    if (!fp64_devices.empty()) {
        chosen = &fp64_devices.front();
    } else {
        std::cerr << "Warning: no available OpenCL device supports double precision. "
                     "Selecting first available device without fp64 support.\n";
        chosen = &available_devices.front();
    }

    std::cerr << "device selected: " << chosen->getInfo<CL_DEVICE_NAME>() << '\n';

    return *chosen;
}

}  // namespace

compute_context::compute_context() {
    //  I hate this. It is garbage.
    const auto type_to_string = [](device_type type) {
        switch (type) {
            case device_type::cpu: return "CPU";
            case device_type::gpu: return "GPU";
        }
        return "unknown";
    };

    for (const auto& type : {device_type::gpu, device_type::cpu}) {
        try {
            *this = compute_context{type};
            return;
        } catch (const std::exception& e) {
            std::cerr << "Falling back from " << type_to_string(type)
                      << " device initialisation: " << e.what() << '\n';
        }
    }
    throw std::runtime_error{"No OpenCL context contains a usable device."};
}

compute_context::compute_context(device_type type)
        : compute_context(core::get_context(type)) {}

compute_context::compute_context(const cl::Context& context)
        : compute_context(context, core::get_device(context)) {}

compute_context::compute_context(const cl::Context& context,
                                 const cl::Device& device)
        : context(context)
        , device(device) {}
}  // namespace core
}  // namespace wayverb
