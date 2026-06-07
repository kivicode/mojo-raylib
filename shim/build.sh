#!/usr/bin/env bash
# Build raylib + the Mojo FFI shim and install both into $PREFIX/lib.
# Runs from $SRC_DIR which is a staged copy of the repo root, including the
# vendor/raylib submodule.

set -euo pipefail

if [ ! -f vendor/raylib/src/raylib.h ]; then
  echo "shim/build.sh: vendor/raylib not initialised — run \`git submodule update --init\` first." >&2
  exit 1
fi

# pixi-build re-stages all of raylib's source into a fresh work dir on every
# invocation, so the old in-tree/$HOME cache never survived (inside the sandbox
# $HOME is the throwaway work dir) and raylib recompiled every single build.
raylib_hash() {
  if command -v sha1sum >/dev/null 2>&1; then sha1sum
  elif command -v shasum >/dev/null 2>&1; then shasum
  else cksum; fi
}
RAYLIB_KEY="$(
  find vendor/raylib/src vendor/raylib/CMakeLists.txt vendor/raylib/cmake \
    -type f -print0 2>/dev/null | sort -z | xargs -0 cat 2>/dev/null \
    | raylib_hash | cut -d' ' -f1 | cut -c1-16
)"
CACHE_ROOT="${MOJO_RAYLIB_CACHE:-${TMPDIR:-/tmp}/mojo-raylib-cache-$(id -u)}"
RAYLIB_CACHE="${CACHE_ROOT}/raylib-${RAYLIB_KEY:-unknown}-$(uname -m)-$(uname -s)"
BUILD_RAYLIB_DIR="${RAYLIB_CACHE}/build"
RAYLIB_INSTALL="${RAYLIB_CACHE}/install"

if [ ! -f "$RAYLIB_CACHE/.complete" ]; then
  rm -rf "$BUILD_RAYLIB_DIR" "$RAYLIB_INSTALL"   # drop any partial build
  # ${CMAKE_ARGS:-} carries conda-build's -DCMAKE_PREFIX_PATH=$PREFIX etc.,
  # which is what makes cmake's FindX11 look inside the host env (the X11 deps
  # the recipe pulls into `host`). GLFW_BUILD_X11=ON / WAYLAND=OFF pins the
  # backend to match those deps (raylib's current defaults; pinned defensively).
  cmake -S vendor/raylib -B "$BUILD_RAYLIB_DIR" \
    ${CMAKE_ARGS:-} \
    -G Ninja \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_GAMES=OFF \
    -DGLFW_BUILD_X11=ON \
    -DGLFW_BUILD_WAYLAND=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_LIBDIR=lib
  cmake --build "$BUILD_RAYLIB_DIR"
  cmake --install "$BUILD_RAYLIB_DIR" --prefix "$RAYLIB_INSTALL"
  touch "$RAYLIB_CACHE/.complete"
fi

# Publish the cached raylib (lib/, include/, ...) into this build's prefix.
# raylib's install_name/soname is @rpath/soname-relative, so the copy stays
# loadable regardless of the prefix it was originally built under.
cp -a "$RAYLIB_INSTALL/." "$PREFIX/"

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
