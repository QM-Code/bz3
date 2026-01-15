# BZ3

BZ3 is a C++20 client/server 3D game inspired by BZFlag.

## Quick Start

**Linux/macOS**
- `./setup.sh`
- `cmake --build build`

**Windows**
- Run `setup.bat`
- `cmake --build build --config Release`

This project uses vcpkg to provide most native dependencies and the setup scripts automatically configure CMake with the correct toolchain.

## Runtime Data

The programs load assets/config from a data root resolved via the `BZ3_DATA_DIR` environment variable.

- Linux/macOS:

  - `export BZ3_DATA_DIR="$PWD/data"`
- Windows (PowerShell):

  - `$env:BZ3_DATA_DIR = "$pwd\data"`

## Install (Prerequisites)

Dependencies are built via vcpkg, but you still need a working C/C++ toolchain and OS graphics headers.

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y \
  git cmake build-essential ninja-build pkg-config \
  python3 python3-dev \
  xorg-dev libgl1-mesa-dev
```

Then run `./setup.sh`.

### macOS

- Install Xcode Command Line Tools:

  - `xcode-select --install`
- Install build tools:

  - `brew install git cmake ninja pkg-config python`

Then run `./setup.sh`.

### Windows

- Install **Visual Studio 2022** (or Build Tools) with **Desktop development with C++**.
- Install **Git**.
- Ensure **CMake** is available (VS includes it, or install separately).

Then run `setup.bat`.

## Run

After building:

- Linux/macOS: `./build/bz3` and `./build/bz3-server`
- Windows: `build\Release\bz3.exe` and `build\Release\bz3-server.exe`

## Overview (Libraries in Use)

Rendering and windowing
- **threepp** (renderer/scene graph)
- **OpenGL** + **GLFW** (window + context)
- **Assimp** (model loading)

UI
- **Dear ImGui** (HUD + server browser)

Networking
- **ENet** (reliable UDP transport)
- **Protobuf** (message schema/serialization)
- Custom LAN discovery protocol (see `src/network/discovery_protocol.hpp`)

Simulation
- **Bullet** (physics)
- **glm** (math)

Other
- **miniaudio** (audio)
- **spdlog** (logging)
- **cxxopts** (CLI parsing)
- **nlohmann-json** (config)
- **miniz** (world zip/unzip)
- **libcurl** (HTTP fetches, e.g. remote server list)
- **pybind11** + **Python** (server-side plugins)

## Notes

- Most dependencies are provided by vcpkg (see `vcpkg.json`).
- Some libraries are fetched via CMake FetchContent (notably `threepp`, `enet`, `pybind11`).
- Python plugin bytecode is redirected to a writable cache: set `BZ3_PY_CACHE_DIR` to choose the location (defaults to `/tmp/bz3-pycache`). If the directory cannot be created, bytecode writing is disabled. Current behavior is acceptable for now; we may revisit a dedicated cache path later.

## TODO
* Switch to Jolt physics controller instead of bullet

## License

MIT