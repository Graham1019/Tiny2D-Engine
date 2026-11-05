#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=$(cd "${1:-$root/build/m63c-release}" && pwd)
dist=${2:-$root/build/m63c-dist}
cmake --build "$build" --target Tiny2DSurvivor --parallel
mkdir -p "$dist"
# Preserve previous candidates and evidence, including on repeated invocations.
stage=$(mktemp -d "$dist/candidate-XXXXXX")
app="$stage/Tiny2D Survivor.app"
ditto "$build/Tiny2DSurvivor.app" "$app"
cmake -DAPP="$app" -P "$root/cmake/FixBundle.cmake"
find "$app/Contents" -name '*.dylib' -type f -print0 | while IFS= read -r -d '' lib; do
  codesign --force --sign - "$lib"
done
codesign --force --sign - "$app"
"$root/scripts/verify_macos_bundle.sh" "$app"
zip="$stage/Tiny2D-Survivor-v0.5.0-rc1-macos-arm64.zip"
ditto -c -k --sequesterRsrc --keepParent "$app" "$zip"
(cd "$stage" && shasum -a 256 "$(basename "$zip")" > SHA256SUMS && shasum -a 256 -c SHA256SUMS)
unpack=$(mktemp -d "${TMPDIR:-/tmp}/tiny2d-release-validation.XXXXXX")
ditto -x -k "$zip" "$unpack"
"$root/scripts/verify_macos_bundle.sh" "$unpack/Tiny2D Survivor.app"
printf 'Candidate: %s\nIndependent extraction: %s\n' "$stage" "$unpack"
