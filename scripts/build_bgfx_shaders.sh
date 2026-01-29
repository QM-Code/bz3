#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
shader_dir="$repo_root/data/bgfx/shaders"
bin_dir="$shader_dir/bin/vk"
brightness_dir="$bin_dir/brightness"

if [[ ! -d "$shader_dir" ]]; then
  echo "BGFX shader directory not found: $shader_dir" >&2
  exit 1
fi

find_shaderc() {
  if [[ -n "${BGFX_SHADERC:-}" && -x "$BGFX_SHADERC" ]]; then
    echo "$BGFX_SHADERC"
    return 0
  fi
  if [[ -n "${VCPKG_ROOT:-}" ]]; then
    local candidate="$VCPKG_ROOT/installed/x64-linux/tools/bgfx/shaderc"
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  fi
  local vcpkg_candidate
  if [[ -n "${BZ3_BUILD_DIR:-}" ]]; then
    vcpkg_candidate="$BZ3_BUILD_DIR/vcpkg_installed/x64-linux/tools/bgfx/shaderc"
    if [[ -x "$vcpkg_candidate" ]]; then
      echo "$vcpkg_candidate"
      return 0
    fi
  fi
  vcpkg_candidate="$(find "$repo_root" -maxdepth 2 -type f -path "*/vcpkg_installed/*/tools/bgfx/shaderc" 2>/dev/null | head -n 1)"
  if [[ -n "$vcpkg_candidate" && -x "$vcpkg_candidate" ]]; then
    echo "$vcpkg_candidate"
    return 0
  fi
  if command -v shaderc >/dev/null 2>&1; then
    command -v shaderc
    return 0
  fi
  if [[ -n "${BGFX_TOOLS:-}" ]]; then
    local candidate="$BGFX_TOOLS/shaderc"
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  fi
  return 1
}

SHADERC_BIN=""
if SHADERC_BIN="$(find_shaderc)"; then
  :
else
  echo "shaderc not found." >&2
  echo "Set BGFX_SHADERC or install bgfx tools and add shaderc to PATH." >&2
  exit 1
fi

mkdir -p "$brightness_dir"

compile_shader() {
  local src="$1"
  local dst="$2"
  local type="$3"
  if [[ ! -f "$dst" || "$src" -nt "$dst" ]]; then
    echo "[bgfx-shaders] $src -> $dst"
    "$SHADERC_BIN" -f "$src" -o "$dst" \
      --type "$type" --platform linux --profile spirv \
      --varyingdef "$shader_dir/brightness/varying.def.sc" \
      -i "$shader_dir"
  fi
}

compile_shader "$shader_dir/brightness/vs_brightness.sc" "$brightness_dir/vs_brightness.bin" v
compile_shader "$shader_dir/brightness/fs_brightness.sc" "$brightness_dir/fs_brightness.bin" f

echo "[bgfx-shaders] done"
