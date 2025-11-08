set(name render_binaural)

add_executable(${name}
    main.cpp)

target_link_libraries(${name}
    combined
    core
    audio_file
    utilities)

set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${name})

