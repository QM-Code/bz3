#!/usr/bin/env python3

import os
import sys
import subprocess


def usage() -> None:
    prog = os.path.basename(sys.argv[0])
    print(f"Usage: {prog} [-c] [build-<window>-<ui>-<physics>-<audio>-<renderer>-<network>]", file=sys.stderr)
    print("  -c   run cmake -S configure step before building", file=sys.stderr)
    raise SystemExit(1)


def prompt_choice(prompt: str, default: str, options: list[str]) -> str:
    if not sys.stdin.isatty():
        print("Error: interactive prompts require a tty. Provide a build directory.", file=sys.stderr)
        raise SystemExit(1)

    while True:
        try:
            choice = input(f"{prompt} [{default}]: ").strip()
        except EOFError:
            print("Error: unable to read interactive input.", file=sys.stderr)
            raise SystemExit(1)
        except KeyboardInterrupt:
            print("\nCanceled")
            raise SystemExit(130)
        if not choice:
            choice = default
        if choice in options:
            return choice
        print(f"Invalid choice: {choice}")


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


args = sys.argv[1:]
run_configure = False

# Minimal option parsing for -c
if args and args[0] == "-c":
    run_configure = True
    args = args[1:]

if len(args) > 1:
    usage()

window = ""
ui = ""
physics = ""
audio = ""
render = ""
network = ""
build_dir = ""

if not args:
    window = prompt_choice("Platform (sdl/glfw)", "sdl", ["sdl", "glfw"])
    ui = prompt_choice("UI (rmlui/imgui)", "rmlui", ["rmlui", "imgui"])
    physics = prompt_choice("Physics (jolt/bullet)", "jolt", ["jolt", "bullet"])
    audio = prompt_choice("Audio (miniaudio/sdlaudio)", "sdlaudio", ["miniaudio", "sdlaudio"])
    render = prompt_choice("Renderer (threepp)", "threepp", ["threepp"])
    network = prompt_choice("Network (enet)", "enet", ["enet"])
    build_dir = f"build-{window}-{ui}-{physics}-{audio}-{render}-{network}"
else:
    build_dir = args[0]
    if not build_dir.startswith("build-"):
        print("Error: build directory must start with 'build-'", file=sys.stderr)
        usage()

    name = build_dir[len("build-"):]
    parts = name.split("-") if name else []

    for part in parts:
        if part in ("glfw", "sdl"):
            if not window:
                window = part
        elif part in ("imgui", "rmlui"):
            ui = part
        elif part in ("jolt", "bullet"):
            physics = part
        elif part in ("miniaudio", "sdlaudio"):
            audio = part
        elif part == "threepp":
            render = part
        elif part == "enet":
            network = part

    if not (window and ui and physics and audio and render and network):
        print("Error: build dir must include window, ui, physics, audio, renderer, and network tokens.", file=sys.stderr)
        usage()

cmake_args = [
    f"-DBZ3_WINDOW_BACKEND={window}",
    f"-DBZ3_UI_BACKEND={ui}",
    f"-DBZ3_PHYSICS_BACKEND={physics}",
    f"-DBZ3_AUDIO_BACKEND={audio}",
    f"-DBZ3_RENDER_BACKEND={render}",
    f"-DBZ3_NETWORK_BACKEND={network}",
]

if not os.path.isdir(build_dir):
    os.makedirs(build_dir, exist_ok=True)
    run_configure = True

if run_configure:
    run(["cmake", "-S", ".", "-B", build_dir, *cmake_args])

run(["cmake", "--build", build_dir, "-j"])
