file(GLOB_RECURSE sources "src/*.cpp")
file(GLOB_RECURSE public_headers "include/*.h")
file(GLOB_RECURSE private_headers "src/*.h")

set(outfile ${CMAKE_CURRENT_BINARY_DIR}/hrtf_entries.h)

set(_candidate_hrtf_dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/IRC_SADIE_D2
    ${CMAKE_CURRENT_SOURCE_DIR}/data/IRC_1002_C
)

set(hrtf_data_dir "")
foreach(_candidate ${_candidate_hrtf_dirs})
    if(IS_DIRECTORY ${_candidate} AND hrtf_data_dir STREQUAL "")
        set(hrtf_data_dir ${_candidate})
    endif()
endforeach()

if(NOT hrtf_data_dir STREQUAL "")
    message(STATUS "Generating HRTF table from dataset at ${hrtf_data_dir}")
    file(GLOB hrtf_dataset_files "${hrtf_data_dir}/*.wav")
    add_custom_command(
        OUTPUT ${outfile}
        COMMAND write_hrtf ${hrtf_data_dir} > ${outfile}
        DEPENDS write_hrtf ${hrtf_dataset_files}
    )
else()
    message(WARNING "No HRTF dataset found; building with empty HRTF stub.")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/data/hrtf_entries_stub.h.in ${outfile} @ONLY)
endif()

add_library(core STATIC ${sources} ${public_headers} ${private_headers} ${outfile})

add_dependencies(core glm_external opencl_cpp_external cereal_external)

target_include_directories(core
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${WAYVERB_SOURCE_DIR}/include
    PRIVATE
    src
    ${DEPENDENCY_INSTALL_PREFIX}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(core
    ${OpenCL_LIBRARIES}
    assimp
    ZLIB::ZLIB
    fftwf 
    audio_file 
    frequency_domain
    utilities
    hrtf
)

add_subdirectory(tests)
