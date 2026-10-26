#!/bin/sh
# Builds the offline synth tools (optimised, no sanitizers) into build-tools/.
set -e
cd "$(dirname "$0")/.."
mkdir -p build-tools
g++ -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Icore/include -Icore/src \
    tools/render_synth.cpp core/src/*.cpp core/src/dsp/*.cpp -o build-tools/render_synth
echo "built build-tools/render_synth"
