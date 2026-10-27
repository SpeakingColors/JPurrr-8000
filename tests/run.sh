#!/bin/sh
# Core tests with address + undefined behaviour sanitizers.
set -e
cd "$(dirname "$0")/.."
cmake -S . -B build-test -G Ninja -DJPURRR_SANITIZE=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
cmake --build build-test
./build-test/tests/jpurrr_tests "$@"
