#!/bin/sh
# used by juce project to make sure all libraries are up-to-date

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
echo "[build.sh] invoked from $(pwd)"

"${ROOT_DIR}/tools/write_build_id_header.sh"

BUILD_DIR="${WAYVERB_BUILD_DIR:-build}"
echo "[build.sh] using build dir ${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

clang_tidy_options=-DCMAKE_CXX_CLANG_TIDY:STRING="clang-tidy;-checks=-*,clang-*,-clang-analyzer-alpha*,performance-*,readability-*"

#iwyu_options=-DCMAKE_CXX_INCLUDE_WHAT_YOU_USE:STRING=iwyu

detected_arch="$(uname -m)"
cmake_arch="${CMAKE_OSX_ARCHITECTURES:-$detected_arch}"

cmake_deployment_target="${CMAKE_OSX_DEPLOYMENT_TARGET:-10.13}"

if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    cmake -S . -B "${BUILD_DIR}" \
          -DCMAKE_OSX_ARCHITECTURES="${cmake_arch}" \
          -DCMAKE_OSX_DEPLOYMENT_TARGET="${cmake_deployment_target}" \
          -DCMAKE_POLICY_VERSION_MINIMUM=3.5
fi

cmake --build "${BUILD_DIR}"
