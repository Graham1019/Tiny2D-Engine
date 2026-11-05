#!/bin/bash
# Non-GUI negative acceptance: copied app must not fall back to developer assets.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/tiny2d missing assets.XXXXXX")
ditto "$1" "$tmp/Test App.app"
asset="$tmp/Test App.app/Contents/Resources/assets/textures/sprite.png"
mv "$asset" "$asset.disabled"
ln -s "$tmp/Test App.app/Contents/MacOS/Tiny2DSurvivor" "$tmp/launch-link"
set +e
(cd / && "$tmp/launch-link") > "$tmp/output.txt" 2>&1
result=$?
set -e
cat "$tmp/output.txt"
[[ $result == 1 ]]
grep -F 'Missing Tiny2D asset:' "$tmp/output.txt"
grep -F '/Test App.app/Contents/Resources/assets/textures/sprite.png' "$tmp/output.txt"
printf 'Missing-asset diagnostic through symlink, spaces and unrelated cwd PASS (%s)\n' "$tmp"
