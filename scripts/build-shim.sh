#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RAYLIB_BUILD_DIR="$ROOT_DIR/build/vendor/raylib"
NATIVE_BUILD_DIR="$ROOT_DIR/build/native"

cmake -S "$ROOT_DIR/vendor/raylib" -B "$RAYLIB_BUILD_DIR" \
  -G Ninja \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_GAMES=OFF \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "$RAYLIB_BUILD_DIR"

mkdir -p "$NATIVE_BUILD_DIR"

case "$(uname -s)" in
  Darwin)
    clang -dynamiclib -fPIC \
      -I"$ROOT_DIR/vendor/raylib/src" \
      "$ROOT_DIR/native/mojo_raylib_shim.c" \
      -L"$RAYLIB_BUILD_DIR/raylib" -lraylib \
      -o "$NATIVE_BUILD_DIR/libmojo_raylib_shim.dylib"
    ;;
  Linux)
    clang -shared -fPIC \
      -I"$ROOT_DIR/vendor/raylib/src" \
      "$ROOT_DIR/native/mojo_raylib_shim.c" \
      -L"$RAYLIB_BUILD_DIR/raylib" -lraylib \
      -o "$NATIVE_BUILD_DIR/libmojo_raylib_shim.so"
    ;;
  *)
    echo "Unsupported host platform: $(uname -s)" >&2
    exit 1
    ;;
esac
