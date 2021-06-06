#!/bin/sh

set -e
set -x

export CC=gcc-7
export CXX=g++-7
export CTEST_OUTPUT_ON_FAILURE=1

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DTEAKRA_TEST_ASSETS_DIR="$HOME/assets" -DTEAKRA_RUN_TESTS=ON
cmake --build . -- -j"$(nproc)"
ctest -C Release
