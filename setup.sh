#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

pushd "$ROOT_DIR" >/dev/null

if [[ "${KARMA_SETUP_FORGE:-}" == "1" ]]; then
	if [[ ! -d "third_party/the-forge/Common_3" ]]; then
		echo "[setup] The Forge not found; cloning..."
		git clone https://github.com/ConfettiFX/The-Forge.git third_party/the-forge
	fi
fi

if [[ ! -d "vcpkg" ]]; then
		echo "[setup] vcpkg/ not found; cloning..."
		git clone https://github.com/microsoft/vcpkg.git
fi

if [[ ! -x "./vcpkg/vcpkg" ]]; then
		echo "[setup] Bootstrapping vcpkg..."
		./vcpkg/bootstrap-vcpkg.sh
fi

echo "[setup] Installing vcpkg dependencies (vcpkg.json)..."
./vcpkg/vcpkg install

echo "[setup] Configuring CMake (Release) with vcpkg toolchain..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake"

echo "[setup] Setup complete. Build with: cmake --build build"

popd >/dev/null
