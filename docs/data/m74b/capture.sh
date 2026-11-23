#!/bin/sh
# Run at repository root after building Release; all PNGs are GL back-buffer readbacks.
set -eu
lab=build/m74b-release/lab/tiny2d_lab
media=docs/media/m74b
work=$(mktemp -d build/m74b-captures.XXXXXX)
snap() {
    file=$1
    shift
    "$lab" --size 800x450 --seed 71 --frames 4 --inspector off --gpu-timing off --capture "$file" "$@"
}
compare() {
    cmp "$1" "$2"
    shasum -a 256 "$1" "$2"
}
for view in scene bright blur final; do
    snap "$work/$view.png" --view "$view"
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
    snap "$work/$shot.png" --lighting on --view "$view" --bloom "$bloom" --frames "$ticks"
    compare "$work/$shot.png" "docs/media/m73/$shot.png"
done
for shot in flat flat-buffer procedural normal-buffer; do
    view=final
    material=tile
    case "$shot" in flat|flat-buffer) material=flat ;; esac
    case "$shot" in *-buffer) view=normal ;; esac
    snap "$work/$shot.png" --lighting on --normal-mapping on --normal-material "$material" --view "$view" --shadows off
    compare "$work/$shot.png" "docs/media/m74a/$shot.png"
done
snap "$work/inspector.png" --lighting on --normal-mapping on --shadows off --inspector on --previews all
compare "$work/inspector.png" "docs/media/m74a/inspector.png"
build/m74b-release/lab/tiny2d_m74a_gl "$work"
for side in left right; do
    compare "$work/direction-$side.png" "docs/media/m74a/direction-$side.png"
done
cp "$work/procedural.png" "$media/shadows-off.png"
scene() {
    file=$1
    shift
    snap "$media/$file.png" --lighting on --shadows on --normal-mapping on --shadow-lights 4 --occluders demo "$@"
}
scene shadows-on
scene radial --normal-mapping off
scene shadow-mask --view shadow
scene inspector --inspector on --previews all --gpu-timing on --size 1120x630
mkdir -p "$work/frames"
build/m74b-release/lab/tiny2d_m74b_gl "$media" "$work/frames"
img2webp -loop 0 -d 80 -lossy -q 82 "$work"/frames/*.png -o "$media/player-motion.webp"
