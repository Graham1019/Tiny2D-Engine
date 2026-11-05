#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
python3 "$root/scripts/verify_macos_bundle.py" "$1"
codesign --verify --deep --strict --verbose=2 "$1"
