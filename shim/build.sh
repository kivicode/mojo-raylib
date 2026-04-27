#!/usr/bin/env bash
# Build raylib + the Mojo FFI shim and install both into $PREFIX/lib.
# Runs from $SRC_DIR which is a staged copy of the repo root, including the
# vendor/raylib submodule.

set -euo pipefail

if [ ! -f vendor/raylib/src/raylib.h ]; then
  echo "shim/build.sh: vendor/raylib not initialised — run \`git submodule update --init\` first." >&2
  exit 1
fi

cmake -S vendor/raylib -B build_raylib \
  -G Ninja \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_GAMES=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DCMAKE_INSTALL_LIBDIR=lib

cmake --build build_raylib
cmake --install build_raylib

case "$(uname -s)" in
  Darwin)
    clang -dynamiclib -fPIC \
      -I vendor/raylib/src \
      native/mojo_raylib_shim.c \
      -L"$PREFIX/lib" -lraylib \
      -Wl,-install_name,@rpath/libmojo_raylib_shim.dylib \
      -o "$PREFIX/lib/libmojo_raylib_shim.dylib"
    ;;
  Linux)
    clang -shared -fPIC \
      -I vendor/raylib/src \
      native/mojo_raylib_shim.c \
      -L"$PREFIX/lib" -lraylib \
      -Wl,-rpath,'$ORIGIN' \
      -o "$PREFIX/lib/libmojo_raylib_shim.so"
    ;;
  *)
    echo "shim/build.sh: unsupported platform $(uname -s)" >&2
    exit 1
    ;;
esac
