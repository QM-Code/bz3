#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
shader_dir="$repo_root/data/forge/shaders"

if [[ ! -d "$shader_dir" ]]; then
  echo "Forge shader directory not found: $shader_dir" >&2
  exit 1
fi

find_glslang() {
  if command -v glslangValidator >/dev/null 2>&1; then
    command -v glslangValidator
    return 0
  fi
  if [[ -n "${VULKAN_SDK:-}" ]]; then
    if [[ -x "$VULKAN_SDK/bin/glslangValidator" ]]; then
      echo "$VULKAN_SDK/bin/glslangValidator"
      return 0
    fi
  fi
  return 1
}

GLSLANG_BIN=""
if GLSLANG_BIN="$(find_glslang)"; then
  :
else
  echo "glslangValidator not found." >&2
  echo "Install it and re-run this script." >&2
  echo "Common installs:" >&2
  echo "  Ubuntu/Debian: sudo apt-get install glslang-tools" >&2
  echo "  Fedora:        sudo dnf install glslang" >&2
  echo "  Arch:          sudo pacman -S glslang" >&2
  echo "  macOS (brew):  brew install glslang" >&2
  exit 1
fi

shopt -s nullglob

compile_shader() {
  local src="$1"
  local dst="$2"
  if [[ ! -f "$dst" || "$src" -nt "$dst" ]]; then
    echo "[forge-shaders] $src -> $dst"
    "$GLSLANG_BIN" -V "$src" -o "$dst"
  fi
}

for src in "$shader_dir"/*.vert "$shader_dir"/*.frag; do
  dst="$src.spv"
  compile_shader "$src" "$dst"
  done

echo "[forge-shaders] done"
