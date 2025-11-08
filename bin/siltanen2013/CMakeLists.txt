set(name siltanen2013)
file(GLOB_RECURSE sources "*.cpp")
add_executable(${name} ${sources})

target_link_libraries(${name}
    PRIVATE
        box
        combined
        audio_file
        utilities
        core)
