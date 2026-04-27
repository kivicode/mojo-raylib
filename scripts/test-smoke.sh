#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RAYLIB_BUILD_DIR="$ROOT_DIR/build/vendor/raylib"
NATIVE_BUILD_DIR="$ROOT_DIR/build/native"
SMOKE_DIR="$ROOT_DIR/build/tests"
mkdir -p "$SMOKE_DIR"

case "$(uname -s)" in
  Darwin)
    SHIM_LIB="$NATIVE_BUILD_DIR/libmojo_raylib_shim.dylib"
    export DYLD_LIBRARY_PATH="$NATIVE_BUILD_DIR:$RAYLIB_BUILD_DIR/raylib:${DYLD_LIBRARY_PATH:-}"
    ;;
  Linux)
    SHIM_LIB="$NATIVE_BUILD_DIR/libmojo_raylib_shim.so"
    export LD_LIBRARY_PATH="$NATIVE_BUILD_DIR:$RAYLIB_BUILD_DIR/raylib:${LD_LIBRARY_PATH:-}"
    ;;
  *)
    echo "Unsupported host platform: $(uname -s)" >&2
    exit 1
    ;;
esac

clang \
  -I"$ROOT_DIR/vendor/raylib/src" \
  "$ROOT_DIR/tests/shim_smoke.c" \
  "$SHIM_LIB" \
  -L"$RAYLIB_BUILD_DIR/raylib" -lraylib \
  -o "$SMOKE_DIR/shim_smoke"

"$SMOKE_DIR/shim_smoke"

if command -v mojo >/dev/null 2>&1; then
  mojo build "$ROOT_DIR/tests/smoke_raymath.mojo" \
    -I "$ROOT_DIR" \
    -o "$SMOKE_DIR/smoke_raymath" \
    -Xlinker "$SHIM_LIB" \
    -Xlinker -L"$RAYLIB_BUILD_DIR/raylib" \
    -Xlinker -lraylib
  "$SMOKE_DIR/smoke_raymath"

  mojo build "$ROOT_DIR/examples/basic_window.mojo" \
    -I "$ROOT_DIR" \
    -o "$SMOKE_DIR/basic_window" \
    -Xlinker "$SHIM_LIB" \
    -Xlinker -L"$RAYLIB_BUILD_DIR/raylib" \
    -Xlinker -lraylib

  mojo build "$ROOT_DIR/examples/random_values.mojo" \
    -I "$ROOT_DIR" \
    -o "$SMOKE_DIR/random_values" \
    -Xlinker "$SHIM_LIB" \
    -Xlinker -L"$RAYLIB_BUILD_DIR/raylib" \
    -Xlinker -lraylib

  mojo build "$ROOT_DIR/examples/basic_shapes.mojo" \
    -I "$ROOT_DIR" \
    -o "$SMOKE_DIR/basic_shapes" \
    -Xlinker "$SHIM_LIB" \
    -Xlinker -L"$RAYLIB_BUILD_DIR/raylib" \
    -Xlinker -lraylib
fi
