#!/bin/sh
# Build and run host tests. Extra args go to ctest, e.g. test/run.sh -R gestures
set -e
cd "$(dirname "$0")/.."
cmake -S test -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build build-host
ctest --test-dir build-host --output-on-failure "$@"
