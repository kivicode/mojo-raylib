#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

mkdir -p "$TMP_DIR/mojo_raylib/raw" "$TMP_DIR/native"
cp "$ROOT_DIR"/mojo_raylib/__init__.mojo "$TMP_DIR/mojo_raylib/__init__.mojo"
cp "$ROOT_DIR"/mojo_raylib/types.mojo "$TMP_DIR/mojo_raylib/types.mojo"
cp "$ROOT_DIR"/mojo_raylib/safe.mojo "$TMP_DIR/mojo_raylib/safe.mojo"
cp "$ROOT_DIR"/mojo_raylib/raymath_safe.mojo "$TMP_DIR/mojo_raylib/raymath_safe.mojo"
cp "$ROOT_DIR"/mojo_raylib/raw/"__init__.mojo" "$TMP_DIR/mojo_raylib/raw/__init__.mojo"
cp "$ROOT_DIR"/mojo_raylib/raw/types.mojo "$TMP_DIR/mojo_raylib/raw/types.mojo"
cp "$ROOT_DIR"/mojo_raylib/raw/raylib.mojo "$TMP_DIR/mojo_raylib/raw/raylib.mojo"
cp "$ROOT_DIR"/mojo_raylib/raw/raymath.mojo "$TMP_DIR/mojo_raylib/raw/raymath.mojo"
cp "$ROOT_DIR"/native/mojo_raylib_shim.c "$TMP_DIR/native/mojo_raylib_shim.c"

bash "$ROOT_DIR/scripts/generate-bindings.sh"

diff -ru "$TMP_DIR/mojo_raylib" "$ROOT_DIR/mojo_raylib"
diff -ru "$TMP_DIR/native" "$ROOT_DIR/native"
