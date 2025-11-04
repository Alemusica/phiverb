#!/bin/sh
# used by juce project to make sure all libraries are up-to-date

mkdir -p build
cd build

clang_tidy_options=-DCMAKE_CXX_CLANG_TIDY:STRING="clang-tidy;-checks=-*,clang-*,-clang-analyzer-alpha*,performance-*,readability-*"

#iwyu_options=-DCMAKE_CXX_INCLUDE_WHAT_YOU_USE:STRING=iwyu

detected_arch="$(uname -m)"
cmake_arch="${CMAKE_OSX_ARCHITECTURES:-$detected_arch}"

cmake_deployment_target="${CMAKE_OSX_DEPLOYMENT_TARGET:-10.13}"

cmake -DCMAKE_OSX_ARCHITECTURES="${cmake_arch}" -DCMAKE_OSX_DEPLOYMENT_TARGET="${cmake_deployment_target}" .. && cmake --build .
