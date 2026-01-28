#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${FILAMENT_DOCKER_IMAGE:-ubuntu:22.04}"
CLEAN_INSTALL="${CLEAN_INSTALL:-0}"
FILAMENT_REF="${FILAMENT_REF:-}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

HOST_UID="$(id -u)"
HOST_GID="$(id -g)"

cat <<MSG
Filament Docker build
  Image:       ${IMAGE}
  Root:        ${ROOT_DIR}
  Build type:  ${BUILD_TYPE}
  Clean:       ${CLEAN_INSTALL}
  Filament ref:${FILAMENT_REF:-<default>}
MSG

DOCKER_CMD=$(cat <<'CMD'
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  build-essential \
  ca-certificates \
  clang-15 \
  curl \
  file \
  git \
  ninja-build \
  pkg-config \
  python3 \
  rsync \
  libgl1-mesa-dev \
  libx11-dev \
  libxrandr-dev \
  libxi-dev \
  libxinerama-dev \
  libxcursor-dev \
  libxrender-dev \
  libxext-dev \
  libxfixes-dev \
  libxkbcommon-dev \
  libwayland-dev \
  wayland-protocols

# Ensure a modern CMake (>= 3.22.1) for Filament.
CM_VERSION=$(cmake --version 2>/dev/null | head -n1 | awk '{print $3}' || true)
NEED_CMAKE=1
if [ -n "$CM_VERSION" ]; then
  MIN_VERSION="3.22.1"
  LOWEST=$(printf '%s\n' "$MIN_VERSION" "$CM_VERSION" | sort -V | head -n1)
  if [ "$LOWEST" = "$MIN_VERSION" ]; then
    NEED_CMAKE=0
  fi
fi
if [ "$NEED_CMAKE" = "1" ]; then
  echo "Installing newer CMake..."
  curl -fsSL -o /tmp/cmake.tar.gz https://github.com/Kitware/CMake/releases/download/v3.27.9/cmake-3.27.9-linux-x86_64.tar.gz
  tar -C /opt -xzf /tmp/cmake.tar.gz
  export PATH="/opt/cmake-3.27.9-linux-x86_64/bin:${PATH}"
fi

cd /work
git config --global --add safe.directory /work/third_party/filament
# Avoid building backend_test_linux (not needed for SDK install).
python3 - <<'PY'
from pathlib import Path

path = Path("/work/third_party/filament/filament/backend/CMakeLists.txt")
data = path.read_text()
marker = "add_executable(backend_test_linux"
if marker in data and "EXCLUDE_FROM_ALL" not in data:
    data = data.replace(
        marker,
        marker,
    )
    inject = (
        "    set_target_properties(backend_test_linux PROPERTIES EXCLUDE_FROM_ALL TRUE)\n"
    )
    if inject not in data:
        data = data.replace(
            "set_target_properties(backend_test_linux PROPERTIES FOLDER Tests)\n",
            "set_target_properties(backend_test_linux PROPERTIES FOLDER Tests)\n" + inject,
        )
    path.write_text(data)
PY
export CC=clang-15
export CXX=clang++-15
export CLEAN_INSTALL
export FILAMENT_REF
export BUILD_TYPE
export FILAMENT_LINUX_IS_MOBILE=0
./scripts/build_filament.sh

git -C /work/third_party/filament checkout -- filament/backend/CMakeLists.txt || true
chown -R ${HOST_UID}:${HOST_GID} /work/third_party/filament /work/libs/filament-sdk
CMD
)

docker run --rm -t \
  -e HOST_UID="${HOST_UID}" \
  -e HOST_GID="${HOST_GID}" \
  -e CLEAN_INSTALL="${CLEAN_INSTALL}" \
  -e FILAMENT_REF="${FILAMENT_REF}" \
  -e BUILD_TYPE="${BUILD_TYPE}" \
  -v "${ROOT_DIR}":/work \
  -w /work \
  "${IMAGE}" \
  bash -lc "${DOCKER_CMD}"
