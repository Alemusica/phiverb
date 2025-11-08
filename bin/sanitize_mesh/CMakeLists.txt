set(name sanitize_mesh)

add_executable(${name}
    main.cpp)

target_link_libraries(${name}
    core
    utilities)

set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${name})

