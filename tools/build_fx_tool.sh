#!/bin/sh
# Builds the offline FX tool (optimised, no sanitizers) into build-tools/render_fx.
set -e
cd "$(dirname "$0")/.."
mkdir -p build-tools
g++ -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Icore/include -Icore/src \
    tools/render_fx.cpp core/src/*.cpp core/src/dsp/*.cpp -o build-tools/render_fx
echo "built build-tools/render_fx"
