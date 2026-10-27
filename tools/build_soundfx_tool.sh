#!/bin/sh
# Builds the Sound page effects render tool (optimised, no sanitizers) into build-tools/render_soundfx.
# Usage: build-tools/render_soundfx [outdir] [--float]
set -e
cd "$(dirname "$0")/.."
mkdir -p build-tools
g++ -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Icore/include -Icore/src \
    tools/render_soundfx.cpp core/src/params_gen.cpp core/src/dsp/soundfx.cpp core/src/dsp/soundfx_multi.cpp -o build-tools/render_soundfx
echo "built build-tools/render_soundfx"
