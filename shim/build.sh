#!/usr/bin/env bash
# Build raylib + the Mojo FFI shim and install both into $PREFIX/lib.
# Runs from $SRC_DIR which is a staged copy of the repo root, including the
# vendor/raylib submodule.

set -euo pipefail

if [ ! -f vendor/raylib/src/raylib.h ]; then
  echo "shim/build.sh: vendor/raylib not initialised — run \`git submodule update --init\` first." >&2
  exit 1
fi

# pixi-build re-stages the source on every invocation, which wipes the
# in-tree build_raylib. Park the cmake build directory in a stable per-user
# cache so the cmake configure step (a few seconds even with --build cache)
# only runs once per raylib revision.
RAYLIB_REV="$(cd vendor/raylib && git rev-parse HEAD 2>/dev/null || echo unknown)"
BUILD_RAYLIB_DIR="${HOME}/.cache/mojo-raylib/raylib-${RAYLIB_REV}-$(uname -m)-$(uname -s)"
mkdir -p "$BUILD_RAYLIB_DIR"

if [ ! -f "$BUILD_RAYLIB_DIR/CMakeCache.txt" ]; then
  # ${CMAKE_ARGS:-} carries conda-build's -DCMAKE_PREFIX_PATH=$PREFIX etc.,
  # which is what makes cmake's FindX11 look inside the host env.
  cmake -S vendor/raylib -B "$BUILD_RAYLIB_DIR" \
    ${CMAKE_ARGS:-} \
    -G Ninja \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_GAMES=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_LIBDIR=lib
fi

cmake --build "$BUILD_RAYLIB_DIR"
cmake --install "$BUILD_RAYLIB_DIR" --prefix "$PREFIX"

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
