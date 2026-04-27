#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RLPARSER="$ROOT_DIR/build/bin/rlparser"

if [[ ! -x "$RLPARSER" ]]; then
  echo "rlparser binary not found at $RLPARSER" >&2
  exit 1
fi

"$RLPARSER" \
  -i "$ROOT_DIR/vendor/raylib/src/raylib.h" \
  -d RLAPI \
  -f CODE \
  -o "$ROOT_DIR"

"$RLPARSER" \
  -i "$ROOT_DIR/vendor/raylib/src/raymath.h" \
  -d RMAPI \
  -f CODE \
  -o "$ROOT_DIR"
