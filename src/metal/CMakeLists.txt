if(NOT WAYVERB_ENABLE_METAL OR NOT APPLE)
  return()
endif()

# Generate embedded Metal kernels header at build time
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_waveguide_kernels.h
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_metal_kernels.py
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_metal_kernels.py
    ${CMAKE_SOURCE_DIR}/src/waveguide/include/waveguide/metal/layout_structs.metal
    ${CMAKE_SOURCE_DIR}/src/waveguide/include/waveguide/metal/common.metal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/waveguide_kernels.metal
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating embedded Metal kernel source with ${Python3_EXECUTABLE}"
  VERBATIM
)

# Add custom target to ensure generation happens
add_custom_target(generate_embedded_kernels
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_waveguide_kernels.h
)

add_library(wayverb_metal STATIC
  src/metal_context.mm
  src/waveguide_pipeline.mm
  src/waveguide_simulation.mm
  src/embedded_waveguide_kernels.h
)

# Ensure kernels are generated before compiling
add_dependencies(wayverb_metal generate_embedded_kernels)

set_source_files_properties(
  src/metal_context.mm
  src/waveguide_pipeline.mm
  src/waveguide_simulation.mm
  PROPERTIES
    OBJCXX_STANDARD 17
    OBJCXX_STANDARD_REQUIRED YES)

target_compile_options(wayverb_metal PRIVATE -std=gnu++17)

target_include_directories(wayverb_metal PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/waveguide/include
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${DEPENDENCY_INSTALL_PREFIX}/include
)

set_target_properties(wayverb_metal PROPERTIES
  FRAMEWORK TRUE
  POSITION_INDEPENDENT_CODE ON
)

find_library(METAL_FRAMEWORK Metal)
find_library(FOUNDATION_FRAMEWORK Foundation)

target_link_libraries(wayverb_metal
  PRIVATE
    ${METAL_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
    waveguide
)
