#!/bin/sh
# From the repo root, after building tiny2d_lab. Output must be a new path.
set -eu
build=${M73_BUILD:-build/m73-release}
c++ -O2 -DNDEBUG -std=c++17 -fobjc-arc -Ilab/src -Iengine/include -Iengine/src/Renderer \
    -I/opt/homebrew/include docs/data/m73/record-demo.mm "$build/libtiny2d_lab_core.a" \
    "$build/libtiny2d_engine.a" "$build/libtiny2d_ui.a" "$build/libtiny2d_game.a" \
    -L/opt/homebrew/lib -lglfw -lpng -framework OpenGL -framework Foundation \
    -framework AVFoundation -framework CoreMedia -framework CoreVideo -o "$build/lab/m73-record-demo"
"$build/lab/m73-record-demo" "${1:-docs/media/m73/demo.mp4}"
