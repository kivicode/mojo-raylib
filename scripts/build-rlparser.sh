#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$ROOT_DIR/build/bin"

cp "$ROOT_DIR/patch/rlparser.c" "$ROOT_DIR/vendor/raylib/tools/rlparser/rlparser.c"

"${CC:-clang}" -O2 -Wall -Wextra \
  -o "$ROOT_DIR/build/bin/rlparser" \
  "$ROOT_DIR/vendor/raylib/tools/rlparser/rlparser.c"
