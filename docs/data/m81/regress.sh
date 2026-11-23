#!/bin/sh
# Run in the repository root. Set RELEASE_DIR to compare a newer build against the same images.
set -eu
release=${RELEASE_DIR:-build/m81-release}
lab=$release/lab/tiny2d_lab
work=$(mktemp -d "$release/m81-history.XXXXXX")
compare() {
    cmp "$1" "$2"
    shasum -a 256 "$1" "$2"
}
snap() {
    destination=$1
    shift
    "$lab" --size 800x450 --seed 71 --frames 4 --inspector off --gpu-timing off --capture "$destination" "$@"
}
for view in scene bright blur final; do
    snap "$work/$view.png" --view "$view"
    compare "$work/$view.png" "docs/media/m71-$view.png"
done
for shot in lighting-on light-buffer bloom-off moving-bolt; do
    view=final; bloom=on; ticks=4
    case "$shot" in light-buffer) view=light ;; bloom-off) bloom=off ;; moving-bolt) ticks=18 ;; esac
    snap "$work/$shot.png" --scene neon --lighting on --view "$view" --bloom "$bloom" --frames "$ticks"
    compare "$work/$shot.png" "docs/media/m73/$shot.png"
done
for shot in flat flat-buffer procedural normal-buffer inspector; do
    view=final; material=tile
    case "$shot" in flat|flat-buffer) material=flat ;; esac
    case "$shot" in *-buffer) view=normal ;; esac
    if [ "$shot" = inspector ]; then set -- --inspector on --previews all; else set --; fi
    snap "$work/$shot.png" --lighting on --normal-mapping on --normal-material "$material" --view "$view" "$@"
    compare "$work/$shot.png" "docs/media/m74a/$shot.png"
done
"$release/lab/tiny2d_m74a_gl" "$work"
for side in left right; do compare "$work/direction-$side.png" "docs/media/m74a/direction-$side.png"; done
echo 'Passed all 15 M7 PNG byte comparisons.'
check_draws() {
    expected=$1; shift
    output=$("$release/tiny2d_sandbox" "$@" --frames 4)
    echo "$output" | rg "Draw calls in final frame: ${expected}$"
}
check_draws 1 --scene triangle
check_draws 500 --scene sprites --renderer immediate --sprite-count 500
check_draws 1 --scene sprites --renderer batch --sprite-count 500
check_draws 5 --scene multitexture --renderer batch --sprite-count 5000
swift -module-cache-path "$release/swift-cache" docs/data/m73/window-smoke.swift "$release/Tiny2DSurvivor.app/Contents/MacOS/Tiny2DSurvivor"
