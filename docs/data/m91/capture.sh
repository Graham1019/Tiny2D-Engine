#!/bin/sh
# From the repository root, in a macOS graphical session with a 2x display.
set -eu
app=${1:-build/m91-release/lab/tiny2d_lab}
media=docs/media/m91
mkdir -p "$media"
capture() {
    image=$1
    shift
    "$app" --scene map --size 800x450 --bloom on --gpu-timing off --capture "$media/$image.png" "$@"
    shasum -a 256 "$media/$image.png"
}
capture walk --map-size 64x40 --seed 71 --map-script walk --frames 96 --inspector off
capture overview --map-size 21x13 --seed 31337 --map-script none --frames 1 --inspector off
capture inspector --map-size 64x40 --seed 71 --map-script none --frames 4 --inspector on
