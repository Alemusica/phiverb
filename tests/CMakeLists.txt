add_executable(test_numerical_safety test_numerical_safety.cpp)

target_include_directories(test_numerical_safety
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src/waveguide/include
    ${CMAKE_SOURCE_DIR}/src/core/include
)

add_test(NAME test_numerical_safety COMMAND test_numerical_safety)

# OpenCL error handling tests
add_executable(test_cl_error_handling test_cl_error_handling.cpp)

target_include_directories(test_cl_error_handling
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/utilities/include
)

target_link_libraries(test_cl_error_handling
    PRIVATE
    OpenCL::OpenCL
)

add_test(NAME test_cl_error_handling COMMAND test_cl_error_handling)
