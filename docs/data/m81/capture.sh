#!/bin/sh
# From the repository root: sh docs/data/m81/capture.sh > docs/data/m81/captures.log 2>&1
# Finite CLI captures the rendered GL_BACK buffer after the HUD/Inspector, before swap.
set -eu
lab=build/m81-release/lab/tiny2d_lab
media=docs/media/m81
work=$(mktemp -d build/m81-motion.XXXXXX)
mkdir -p "$media"

capture() {
    file=$1
    ticks=$2
    shift 2
    echo "capture=$file ticks=$ticks" "$@"
    "$lab" --scene particles --seed 71 --size 800x450 --lighting on --view final \
        --particle-script demo --frames "$ticks" --gpu-timing off --inspector off \
        --capture "$file" "$@"
}

capture "$media/attractor.png" 160
capture "$media/repulsor.png" 340
capture "$media/vortex.png" 520
capture "$media/wind.png" 700
capture "$media/bloom-on.png" 880 --bloom on
capture "$media/bloom-off.png" 880 --bloom off
capture "$media/inspector.png" 880 --inspector on --gpu-timing on --size 1120x630

# Reconstruct a single trajectory from deterministic finite runs at ticks
# 3, 6, ..., 120. Adjacent displayed states are 3/60 s apart (20 fps).
tick=3
while [ "$tick" -le 120 ]; do
    filename=$(printf "%03d.png" "$tick")
    capture "$work/$filename" "$tick" --size 640x360 --particle-script none
    tick=$((tick+3))
done
img2webp -loop 0 -d 50 -lossy -m 4 -q 82 "$work"/*.png -o "$media/motion.webp"
webpmux -info "$media/motion.webp"
shasum -a 256 "$media"/*.png "$media/motion.webp"
