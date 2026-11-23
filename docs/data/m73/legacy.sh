#!/bin/sh
# Release smoke checks for the original Sandbox renderer.
set -eu
sandbox=build/m73-release/tiny2d_sandbox
check_draws() {
    expected=$1
    shift
    output=$("$sandbox" "$@" --frames 4)
    echo "$output"
    echo "$output" | rg "Draw calls in final frame: ${expected}$"
}
check_draws 1 --scene triangle
check_draws 500 --scene sprites --renderer immediate --sprite-count 500
check_draws 1 --scene sprites --renderer batch --sprite-count 500
check_draws 5 --scene multitexture --renderer batch --sprite-count 5000
