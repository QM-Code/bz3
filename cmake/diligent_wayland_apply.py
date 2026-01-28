#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys


def replace_or_fail(path: pathlib.Path, old: str, new: str, label: str) -> None:
    text = path.read_text()
    if old not in text:
        print(f"[diligent-wayland] expected pattern not found in {path}: {label}", file=sys.stderr)
        sys.exit(1)
    path.write_text(text.replace(old, new))


def insert_wayland_member(path: pathlib.Path) -> None:
    text = path.read_text()
    if "pWaylandSurface" in text:
        return
    needle = "    void*  pXCBConnection  DEFAULT_INITIALIZER(nullptr);\n"
    if needle not in text:
        needle = "    void* pXCBConnection DEFAULT_INITIALIZER(nullptr);\n"
    if needle not in text:
        print(f"[diligent-wayland] could not locate pXCBConnection member in {path}", file=sys.stderr)
        sys.exit(1)
    insert = needle + "    void*  pWaylandSurface DEFAULT_INITIALIZER(nullptr);\n"
    path.write_text(text.replace(needle, insert))


def patch_swapchain(path: pathlib.Path) -> None:
    text = path.read_text()
    if "pWaylandSurface" in text and "surfaceCreateInfo.surface" in text:
        return
    new_line = "        surfaceCreateInfo.surface = reinterpret_cast<struct wl_surface*>(m_Window.pWaylandSurface);\n"
    # Replace any assignment to Surface/surface with nullptr.
    pattern = r"^\s*surfaceCreateInfo\.(Surface|surface)\s*=\s*reinterpret_cast<struct wl_surface\*>\(nullptr\);\s*$"
    lines = text.splitlines(keepends=True)
    changed = False
    for i, line in enumerate(lines):
        if re.match(pattern, line):
            lines[i] = new_line
            changed = True
    if not changed:
        print(f"[diligent-wayland] expected Wayland surface assignment not found in {path}", file=sys.stderr)
        sys.exit(1)
    path.write_text("".join(lines))


def patch_vulkan_cmake(path: pathlib.Path) -> None:
    text = path.read_text()
    if "VK_USE_PLATFORM_WAYLAND_KHR=1" in text:
        return
    old = "    set(PRIVATE_COMPILE_DEFINITIONS VK_USE_PLATFORM_XCB_KHR=1 VK_USE_PLATFORM_XLIB_KHR=1 DILIGENT_USE_VOLK=1)\n"
    new = "    set(PRIVATE_COMPILE_DEFINITIONS VK_USE_PLATFORM_XCB_KHR=1 VK_USE_PLATFORM_XLIB_KHR=1 VK_USE_PLATFORM_WAYLAND_KHR=1 DILIGENT_USE_VOLK=1)\n"
    if old in text:
        path.write_text(text.replace(old, new))
        return
    print(f"[diligent-wayland] expected Linux compile definitions not found in {path}", file=sys.stderr)
    sys.exit(1)


def main() -> int:
    repo = pathlib.Path(".")
    linux_native = repo / "DiligentCore/Platforms/Linux/interface/LinuxNativeWindow.h"
    swapchain = repo / "DiligentCore/Graphics/GraphicsEngineVulkan/src/SwapChainVkImpl.cpp"
    vulkan_cmake = repo / "DiligentCore/Graphics/GraphicsEngineVulkan/CMakeLists.txt"

    if not linux_native.exists() or not swapchain.exists() or not vulkan_cmake.exists():
        print("[diligent-wayland] expected DiligentCore layout not found", file=sys.stderr)
        return 1

    insert_wayland_member(linux_native)
    patch_swapchain(swapchain)
    patch_vulkan_cmake(vulkan_cmake)
    print("[diligent-wayland] applied Wayland Vulkan patches")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
