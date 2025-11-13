cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0063)
    cmake_policy(SET CMP0063 NEW)
endif()

project(WAYVERB VERSION 0.1 LANGUAGES CXX)
option(WAYVERB_ENABLE_METAL "Build experimental Metal backend (Apple only)" OFF)

set(ONLY_BUILD_DOCS false CACHE BOOL "skip configuring the build and just build docs")

find_package(Git QUIET)

if(WAYVERB_ENABLE_METAL AND APPLE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()

set(WAYVERB_GIT_DESC "unknown")
set(WAYVERB_GIT_BRANCH "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE WAYVERB_GIT_DESC
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE WAYVERB_GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

string(TIMESTAMP WAYVERB_BUILD_TIMESTAMP UTC "%Y-%m-%dT%H:%M:%SZ")

foreach(var WAYVERB_GIT_DESC WAYVERB_GIT_BRANCH WAYVERB_BUILD_TIMESTAMP)
    string(REPLACE "\"" "\\\"" ${var} "${${var}}")
endforeach()

if(ONLY_BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
        add_custom_target(doc ALL
            ${DOXYGEN_EXECUTABLE}
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY
            ${CMAKE_CURRENT_BINARY_DIR})
    endif(DOXYGEN_FOUND)
else()
    set(CMAKE_VERBOSE_MAKEFILE OFF)

    add_definitions(-DWAYVERB_BUILD_GIT_DESC="${WAYVERB_GIT_DESC}")
    add_definitions(-DWAYVERB_BUILD_GIT_BRANCH="${WAYVERB_GIT_BRANCH}")
    add_definitions(-DWAYVERB_BUILD_TIMESTAMP="${WAYVERB_BUILD_TIMESTAMP}")

    include(${CMAKE_CURRENT_SOURCE_DIR}/config/dependencies.cmake)

    set(CMAKE_BUILD_TYPE RelWithDebugInfo)

    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN true)

    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -ftemplate-depth=1024)

    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

    #set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    #set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

    if(APPLE)
        add_definitions(-DWAYVERB_FORCE_SINGLE_PRECISION=1)
        if(WAYVERB_ENABLE_METAL)
            add_definitions(-DWAYVERB_ENABLE_METAL=1)
        endif()
    endif()

    enable_testing()
    add_subdirectory(src)
    if(WAYVERB_ENABLE_METAL AND APPLE)
        enable_language(OBJCXX)
        add_subdirectory(src/metal)
    endif()
    add_subdirectory(bin)
    add_subdirectory(tests)
endif()
