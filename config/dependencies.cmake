# This is where we download, configure and build all the dependencies.
# Now using CPM (CMake Package Manager) for better dependency management.
# CPM.cmake: https://github.com/cpm-cmake/CPM.cmake
#
# For dependencies that use autotools (fftw, sndfile, samplerate, itpp),
# we still use ExternalProject_Add as they don't have CMake support.

include(ExternalProject)
include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)
find_package(Git REQUIRED)
find_package(ZLIB REQUIRED)

# Set CPM cache directory
set(CPM_SOURCE_CACHE "${CMAKE_BINARY_DIR}/.cpm-cache" CACHE PATH "CPM source cache directory")

# ExternalProject settings for autotools-based dependencies
set(WAYVERB_PATCH_FFTW_DOCS_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/patch_fftw_docs.cmake)
set(DEPENDENCY_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/dependencies)
set_directory_properties(PROPERTIES EP_PREFIX ${DEPENDENCY_INSTALL_PREFIX})
set_property(DIRECTORY PROPERTY EP_BASE "${DEPENDENCY_INSTALL_PREFIX}")

set(ENV_SETTINGS "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
set(CUSTOM_MAKE_COMMAND BUILD_COMMAND make ${ENV_SETTINGS})

# Configure for macOS deployment
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET ${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()

# glm ##########################################################################
# Header-only OpenGL Mathematics library

CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 0.9.8.5
    OPTIONS
        "GLM_TEST_ENABLE OFF"
)

# assimp #######################################################################
# 3D model loading library

CPMAddPackage(
    NAME assimp
    GITHUB_REPOSITORY assimp/assimp
    GIT_TAG v5.3.1
    OPTIONS
        "ASSIMP_BUILD_TESTS OFF"
        "ASSIMP_BUILD_ASSIMP_TOOLS OFF"
        "ASSIMP_BUILD_SAMPLES OFF"
        "ASSIMP_INSTALL OFF"
        "ASSIMP_BUILD_ZLIB OFF"
        "BUILD_SHARED_LIBS OFF"
)

# fftw3 float ##################################################################

ExternalProject_Add(
    fftwf_external
    URL http://fftw.org/fftw-3.3.5.tar.gz
    PATCH_COMMAND
        ${CMAKE_COMMAND}
        -DWAYVERB_SOURCE_DIR:PATH=<SOURCE_DIR>
        -DWAYVERB_PATCH_SENTINEL:PATH=<SOURCE_DIR>/.wayverb_fftw_docs_patched
        -P ${WAYVERB_PATCH_FFTW_DOCS_SCRIPT}
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-float --enable-shared=no
    ${CUSTOM_MAKE_COMMAND}
)

add_library(fftwf UNKNOWN IMPORTED)
add_dependencies(fftwf fftwf_external) 
set_property(TARGET fftwf PROPERTY IMPORTED_LOCATION ${DEPENDENCY_INSTALL_PREFIX}/lib/libfftw3f.a)

# sndfile ######################################################################

ExternalProject_Add(
    sndfile_external
    DOWNLOAD_COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch 1.0.27 https://github.com/erikd/libsndfile.git sndfile_external
    CONFIGURE_COMMAND <SOURCE_DIR>/autogen.sh && <SOURCE_DIR>/configure --disable-external-libs --prefix=<INSTALL_DIR> --enable-shared=no
    ${CUSTOM_MAKE_COMMAND}
)

add_library(sndfile UNKNOWN IMPORTED)
add_dependencies(sndfile sndfile_external) 
set_property(TARGET sndfile PROPERTY IMPORTED_LOCATION ${DEPENDENCY_INSTALL_PREFIX}/lib/libsndfile.a)

# samplerate ###################################################################

# The default libsamplerate distribution uses a deprecated path to Carbon, so
# we have to manually patch it to modify the path before building.

ExternalProject_Add(
    samplerate_external
    DOWNLOAD_COMMAND ${GIT_EXECUTABLE} clone --depth 1 https://github.com/erikd/libsamplerate.git samplerate_external
    CONFIGURE_COMMAND <SOURCE_DIR>/autogen.sh && <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --disable-dependency-tracking --enable-shared=no
    ${CUSTOM_MAKE_COMMAND}
)

add_library(samplerate UNKNOWN IMPORTED)
add_dependencies(samplerate samplerate_external) 
set_property(TARGET samplerate PROPERTY IMPORTED_LOCATION ${DEPENDENCY_INSTALL_PREFIX}/lib/libsamplerate.a)

# gtest ########################################################################
# Google Test framework

CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.14.0
    OPTIONS
        "INSTALL_GTEST OFF"
        "gtest_hide_internal_symbols ON"
)

if(googletest_ADDED)
    # Alias for compatibility
    add_library(gtest ALIAS gtest_main)
endif()

# cereal #######################################################################
# C++ serialization library (header-only)

CPMAddPackage(
    NAME cereal
    GITHUB_REPOSITORY USCiLab/cereal
    GIT_TAG v1.3.2
    OPTIONS
        "JUST_INSTALL_CEREAL ON"
        "SKIP_PORTABILITY_TEST ON"
)

# itpp #########################################################################

ExternalProject_Add(
    fftw_external
    URL http://fftw.org/fftw-3.3.5.tar.gz
    PATCH_COMMAND
        ${CMAKE_COMMAND}
        -DWAYVERB_SOURCE_DIR:PATH=<SOURCE_DIR>
        -DWAYVERB_PATCH_SENTINEL:PATH=<SOURCE_DIR>/.wayverb_fftw_docs_patched
        -P ${WAYVERB_PATCH_FFTW_DOCS_SCRIPT}
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
    ${CUSTOM_MAKE_COMMAND}
)

add_library(fftw UNKNOWN IMPORTED)
add_dependencies(fftw fftw_external)
set_property(TARGET fftw PROPERTY IMPORTED_LOCATION ${DEPENDENCY_INSTALL_PREFIX}/lib/libfftw3.a)

# The only tag, r4.3.0, has a bunch of build warnings, so we just get the most
# recent one and hope that it's less broken and compatible.
# We also hobble the library by hiding some packages from it, which makes
# linking a bit simpler.

ExternalProject_Add(
    itpp_external
    DOWNLOAD_COMMAND ${GIT_EXECUTABLE} clone --depth 1 https://git.code.sf.net/p/itpp/git itpp_external
    PATCH_COMMAND ${CMAKE_COMMAND} -E rm -f version
    CMAKE_ARGS ${GLOBAL_DEPENDENCY_CMAKE_FLAGS} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DITPP_SHARED_LIB=off -DHTML_DOCS=off #-DCMAKE_DISABLE_FIND_PACKAGE_BLAS=on -DCMAKE_DISABLE_FIND_PACKAGE_LAPACK=on
)

add_dependencies(itpp_external fftw_external)

add_library(itpp UNKNOWN IMPORTED)
add_dependencies(itpp itpp_external)
set_property(TARGET itpp PROPERTY IMPORTED_LOCATION ${DEPENDENCY_INSTALL_PREFIX}/lib/libitpp_static.a)

find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
set(ITPP_LIBRARIES itpp fftw ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES})

# opencl #######################################################################
# OpenCL C++ headers

CPMAddPackage(
    NAME OpenCL-CLHPP
    GITHUB_REPOSITORY KhronosGroup/OpenCL-CLHPP
    GIT_TAG v2023.12.14
    OPTIONS
        "BUILD_DOCS OFF"
        "BUILD_EXAMPLES OFF"
        "BUILD_TESTING OFF"
)

find_package(OpenCL REQUIRED)

################################################################################
# APP STUFF ####################################################################
################################################################################

# modern_gl_utils ##############################################################

CPMAddPackage(
    NAME modern_gl_utils
    GITHUB_REPOSITORY reuk/modern_gl_utils
    GIT_TAG master
)
