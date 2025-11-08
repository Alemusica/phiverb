if(NOT WAYVERB_ENABLE_METAL OR NOT APPLE)
  return()
endif()

add_library(wayverb_metal STATIC
  src/metal_context.mm
  src/waveguide_pipeline.mm
  src/waveguide_simulation.mm
)

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
