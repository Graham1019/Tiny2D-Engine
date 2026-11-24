#!/bin/sh
set -eu
release=${RELEASE_DIR:-build/m92-release}
lab=$release/lab/tiny2d_lab
media=docs/media/m92
capture() {
    image=$1
    shift
    "$lab" --scene navigation --seed 71 --size 800x450 --map-size 21x13 --inspector off \
        --gpu-timing off --capture "$media/$image.png" "$@"
    shasum -a 256 "$media/$image.png"
}
capture pursuit --nav-enemies 16 --frames 40
capture path --nav-enemies 16 --nav-debug all --frames 40
capture offer --nav-enemies 0 --nav-script walk --frames 600
capture applied --nav-enemies 0 --nav-script walk --nav-choice first --frames 601
# Also archive a normal survival run with the public M9.1-compatible frame schema.
work=$(mktemp -d)
trap 'rm -f "$work/survival.csv"; rmdir "$work"' EXIT
"$lab" --scene navigation --seed 71 --size 800x450 --map-size 64x40 --nav-enemies 16 \
    --nav-script walk --nav-choice first --frames 1320 --gpu-timing on --inspector off \
    --metrics "$work/survival.csv"
gzip -nc "$work/survival.csv" > docs/data/m92/survival.csv.gz
