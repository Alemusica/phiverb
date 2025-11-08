set(name rt60)
add_executable(${name} ${name}.cpp)

target_link_libraries(${name}
    PRIVATE
        combined
        audio_file
        utilities
        core)
