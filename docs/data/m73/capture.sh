#!/bin/sh
# Run from the repository root after building build/m73-release.
set -eu
lab=build/m73-release/lab/tiny2d_lab
media=docs/media/m73
work=$(mktemp -d build/m73-captures.XXXXXX)
for view in scene bright blur final; do
    "$lab" --scene neon --seed 71 --frames 4 --lighting off --view "$view" --bloom on --inspector off --gpu-timing off --capture "$work/$view.png"
    cmp "$work/$view.png" "docs/media/m71-$view.png"
    shasum -a 256 "$work/$view.png" "docs/media/m71-$view.png"
done
cp "$work/final.png" "$media/lighting-off.png"
capture() {
    name=$1
    frames=$2
    shift 2
    "$lab" --scene neon --seed 71 --size 800x450 --frames "$frames" --lighting on --gpu-timing off --capture "$media/$name.png" "$@"
}
capture lighting-on 4 --view final --bloom on
capture light-buffer 4 --view light --bloom on
capture bloom-off 4 --view final --bloom off
capture moving-bolt 18 --view final --bloom on
capture inspector 4 --view final --bloom on --inspector on --previews all
shasum -a 256 "$media"/*.png
