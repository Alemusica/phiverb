cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED WAYVERB_SOURCE_DIR)
    message(FATAL_ERROR "WAYVERB_SOURCE_DIR is required")
endif()

if(NOT DEFINED WAYVERB_PATCH_SENTINEL)
    set(WAYVERB_PATCH_SENTINEL
        "${WAYVERB_SOURCE_DIR}/.wayverb_fftw_docs_patched")
endif()

if(EXISTS "${WAYVERB_PATCH_SENTINEL}")
    message(STATUS "[wayverb] fftw doc patch sentinel present, skipping")
    return()
endif()

set(MAKEFILE_AM "${WAYVERB_SOURCE_DIR}/Makefile.am")
set(MAKEFILE_IN "${WAYVERB_SOURCE_DIR}/Makefile.in")

foreach(path IN LISTS MAKEFILE_AM MAKEFILE_IN)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Expected ${path} to exist for fftw patching")
    endif()
endforeach()

file(READ "${MAKEFILE_AM}" MAKEFILE_AM_CONTENTS)
set(_am_patched "${MAKEFILE_AM_CONTENTS}")
string(REPLACE "DOCDIR=doc" "DOCDIR=" _am_patched "${_am_patched}")
string(REPLACE "libbench2 $(CHICKEN_EGG) tests mpi $(DOCDIR) tools m4"
               "libbench2 $(CHICKEN_EGG) tests mpi tools m4"
               _am_patched "${_am_patched}")
set(AM_MODIFIED FALSE)
if(NOT _am_patched STREQUAL MAKEFILE_AM_CONTENTS)
    set(AM_MODIFIED TRUE)
    file(WRITE "${MAKEFILE_AM}" "${_am_patched}")
endif()

file(READ "${MAKEFILE_IN}" MAKEFILE_IN_CONTENTS)
set(_in_patched "${MAKEFILE_IN_CONTENTS}")
string(REPLACE "@BUILD_DOC_TRUE@DOCDIR = doc" "@BUILD_DOC_TRUE@DOCDIR = "
               _in_patched "${_in_patched}")
string(REPLACE "libbench2 $(CHICKEN_EGG) tests mpi $(DOCDIR) tools m4"
               "libbench2 $(CHICKEN_EGG) tests mpi tools m4"
               _in_patched "${_in_patched}")
set(IN_MODIFIED FALSE)
if(NOT _in_patched STREQUAL MAKEFILE_IN_CONTENTS)
    set(IN_MODIFIED TRUE)
    file(WRITE "${MAKEFILE_IN}" "${_in_patched}")
endif()

if(AM_MODIFIED OR IN_MODIFIED)
    message(STATUS "[wayverb] disabled fftw doc build")
else()
    message(STATUS "[wayverb] fftw doc patch already applied")
endif()

file(WRITE "${WAYVERB_PATCH_SENTINEL}" "patched\n")
