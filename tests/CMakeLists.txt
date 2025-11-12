add_executable(test_numerical_safety test_numerical_safety.cpp)

target_include_directories(test_numerical_safety
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src/waveguide/include
    ${CMAKE_SOURCE_DIR}/src/core/include
)

add_test(NAME test_numerical_safety COMMAND test_numerical_safety)
