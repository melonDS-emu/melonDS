#!/bin/sh

set -e
set -x
set -o pipefail

export MACOSX_DEPLOYMENT_TARGET=10.14
export CTEST_OUTPUT_ON_FAILURE=1

mkdir build && cd build
cmake .. -GXcode -DCMAKE_BUILD_TYPE=Release -DTEAKRA_TEST_ASSETS_DIR="$HOME/assets" -DTEAKRA_RUN_TESTS=ON
xcodebuild -configuration Release -jobs 2
ctest -C Release
