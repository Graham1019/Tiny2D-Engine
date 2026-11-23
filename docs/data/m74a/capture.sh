#!/bin/sh
# Run from repository root using the Release Lab and its executable-adjacent shaders.
set -eu
lab=build/m74a-release/lab/tiny2d_lab
media=docs/media/m74a
work=$(mktemp -d build/m74a-captures.XXXXXX)
snap() {
    file=$1
    shift
    "$lab" --scene neon --size 800x450 --seed 71 --gpu-timing off --inspector off --capture "$file" "$@"
}
compare() {
    cmp "$1" "$2"
    shasum -a 256 "$1" "$2"
}
for view in scene bright blur final; do
    snap "$work/$view.png" --frames 4 --lighting off --view "$view"
    compare "$work/$view.png" "docs/media/m71-$view.png"
done
for shot in lighting-on light-buffer bloom-off moving-bolt; do
    view=final
    bloom=on
    ticks=4
    case "$shot" in
        light-buffer) view=light ;;
        bloom-off) bloom=off ;;
        moving-bolt) ticks=18 ;;
    esac
    snap "$work/$shot.png" --frames "$ticks" --lighting on --view "$view" --bloom "$bloom"
    compare "$work/$shot.png" "docs/media/m73/$shot.png"
done
normal() {
    name=$1
    shift
    snap "$media/$name.png" --frames 4 --lighting on --normal-mapping on "$@"
}
normal flat --normal-material flat
normal flat-buffer --normal-material flat --view normal
normal procedural --normal-material tile
normal normal-buffer --view normal
normal inspector --inspector on --previews all
shasum -a 256 "$media/flat.png" "$media/flat-buffer.png" "$media/procedural.png" "$media/normal-buffer.png" "$media/inspector.png"
