#!/usr/bin/env bash
# Initialise / update the vendor/raylib git submodule to the rev recorded by
# the parent repo.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
git -C "$ROOT_DIR" submodule update --init --recursive vendor/raylib
