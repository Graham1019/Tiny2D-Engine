#!/bin/sh
# Run from the repository root after the Release build.
set -eux
lab=./build/m72-release/lab/tiny2d_lab
sandbox=./build/m72-release/tiny2d_sandbox
for view in scene bright blur final; do
    "$lab" --scene neon --seed 71 --frames 4 --view "$view" --bloom on --inspector off --gpu-timing off --capture "/tmp/m72-$view.png"
    cmp "/tmp/m72-$view.png" "docs/media/m71-$view.png"
done
"$lab" --scene neon --seed 71 --frames 4 --view final --bloom on --inspector on --previews all --gpu-timing on --capture docs/data/m72/buffers.png
./build/m72-release/lab/tiny2d_m72_gl docs/data/m72
"$sandbox" --scene triangle --frames 4
"$sandbox" --scene sprites --renderer immediate --sprite-count 500 --frames 4
"$sandbox" --scene sprites --renderer batch --sprite-count 500 --frames 4
"$sandbox" --scene multitexture --renderer batch --sprite-count 5000 --frames 4
