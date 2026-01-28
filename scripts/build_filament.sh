#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

FILAMENT_DIR="${FILAMENT_DIR:-$ROOT_DIR/third_party/filament}"
FILAMENT_REF="${FILAMENT_REF:-main}"
INSTALL_DIR="${INSTALL_DIR:-$ROOT_DIR/libs/filament-sdk}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN_INSTALL="${CLEAN_INSTALL:-0}"

case "${BUILD_TYPE,,}" in
  release|debug)
    BUILD_TYPE_LOWER="${BUILD_TYPE,,}"
    ;;
  *)
    echo "ERROR: BUILD_TYPE must be Release or Debug (got: $BUILD_TYPE)"
    exit 1
    ;;
esac
CLEAN_INSTALL="${CLEAN_INSTALL:-0}"

echo "Filament source: $FILAMENT_DIR"
echo "Filament ref:    $FILAMENT_REF"
echo "Install dir:     $INSTALL_DIR"
echo "Build type:      $BUILD_TYPE_LOWER"

if [ ! -d "$FILAMENT_DIR" ]; then
  echo "Cloning Filament..."
  git clone --depth 1 https://github.com/google/filament.git "$FILAMENT_DIR"
fi

if [ -n "$FILAMENT_REF" ]; then
  git -C "$FILAMENT_DIR" fetch --depth 1 origin "$FILAMENT_REF" || true
  git -C "$FILAMENT_DIR" checkout "$FILAMENT_REF"
fi


if [ ! -x "$FILAMENT_DIR/build.sh" ]; then
  echo "ERROR: Filament build.sh not found at $FILAMENT_DIR."
  echo "Please ensure Filament source is checked out correctly."
  exit 1
fi

export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
export MAKEFLAGS="${MAKEFLAGS:--j2}"

echo "Using CC=$CC CXX=$CXX"
if [ "$CLEAN_INSTALL" = "1" ] && [ -d "$INSTALL_DIR" ]; then
  echo "Removing existing SDK at $INSTALL_DIR"
  rm -rf "$INSTALL_DIR"
fi
echo "Building Filament..."

BUILD_DIR="$FILAMENT_DIR/out/cmake-${BUILD_TYPE_LOWER}"
INSTALL_PREFIX="$FILAMENT_DIR/out/${BUILD_TYPE_LOWER}/filament"

if [ "$CLEAN_INSTALL" = "1" ]; then
  rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

GENERATOR="Unix Makefiles"
if command -v ninja >/dev/null 2>&1; then
  GENERATOR="Ninja"
fi

(
  cd "$FILAMENT_DIR"
  cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DFILAMENT_SUPPORTS_VULKAN=ON \
    -DFILAMENT_SUPPORTS_WAYLAND=ON \
    -DUSE_STATIC_LIBCXX=OFF
  cmake --build "$BUILD_DIR"
  cmake --build "$BUILD_DIR" --target install
)

# Filament's build script installs into out/ by default; sync to our SDK path.
if [ -d "$INSTALL_PREFIX" ]; then
  mkdir -p "$INSTALL_DIR"
  rsync -a --delete "$INSTALL_PREFIX/" "$INSTALL_DIR/"
fi

# Filament's install step doesn't include BlueVK headers; copy them explicitly.
if [ -d "$FILAMENT_DIR/libs/bluevk/include/bluevk" ]; then
  mkdir -p "$INSTALL_DIR/include/bluevk"
  rsync -a --delete "$FILAMENT_DIR/libs/bluevk/include/bluevk/" "$INSTALL_DIR/include/bluevk/"
fi

echo "Filament SDK installed to $INSTALL_DIR"
