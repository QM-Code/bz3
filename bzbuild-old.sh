#!/bin/bash

set -euo pipefail

usage() {
  echo "Usage: $0 [-c] [build-<window>-<ui>-<physics>-<audio>-<renderer>]"
  echo "  -c   run cmake -S configure step before building"
  exit 1
}

run_configure=false

while getopts ":c" opt; do
  case "$opt" in
    c) run_configure=true ;;
    *) usage ;;
  esac
done
shift $((OPTIND - 1))

prompt_choice() {
  local prompt="$1"
  local default="$2"
  shift 2
  local options=("$@")
  local input
  while true; do
    printf "%s [%s]: " "$prompt" "$default" > /dev/tty
    if ! IFS= read -r input < /dev/tty; then
      echo "Error: unable to read from tty." > /dev/tty
      exit 1
    fi
    if [ -z "$input" ]; then
      input="$default"
    fi
    for opt in "${options[@]}"; do
      if [ "$input" = "$opt" ]; then
        echo "$input"
        return
      fi
    done
    echo "Invalid choice: $input" > /dev/tty
  done
}

window=""
ui=""
physics=""
audio=""
render=""
build_dir=""

if [ $# -eq 0 ]; then
  if [ ! -t 0 ]; then
    echo "Error: interactive prompts require a tty. Provide a build directory." >&2
    usage
  fi
  window=$(prompt_choice "Platform (sdl/glfw)" "sdl" sdl glfw)
  ui=$(prompt_choice "UI (rmlui/imgui)" "rmlui" rmlui imgui)
  physics=$(prompt_choice "Physics (jolt/bullet)" "jolt" jolt bullet)
  audio=$(prompt_choice "Audio (miniaudio/sdlaudio)" "sdlaudio" miniaudio sdlaudio)
  render=$(prompt_choice "Renderer (threepp)" "threepp" threepp)
  build_dir="build-${window}-${ui}-${physics}-${audio}-${render}"
else
  if [ $# -ne 1 ]; then
    usage
  fi
  build_dir="$1"
  if [[ "$build_dir" != build-* ]]; then
    echo "Error: build directory must start with 'build-'"
    usage
  fi

  name="${build_dir#build-}"
  IFS='-' read -r -a parts <<< "$name"

  for part in "${parts[@]}"; do
    case "$part" in
      glfw|sdl)
        if [ -z "$window" ]; then
          window="$part"
        elif [ -z "$audio" ]; then
          audio="$part"
        fi
        ;;
      imgui|rmlui)
        ui="$part"
        ;;
      jolt|bullet)
        physics="$part"
        ;;
      miniaudio)
        audio="miniaudio"
        ;;
      sdlaudio)
        audio="sdl"
        ;;
      threepp)
        render="threepp"
        ;;
      *)
        ;;
    esac
  done

  if [ -z "$window" ] || [ -z "$ui" ] || [ -z "$physics" ] || [ -z "$audio" ] || [ -z "$render" ]; then
    echo "Error: build dir must include window, ui, physics, audio, and renderer tokens."
    usage
  fi
fi

cmake_args=(
  "-DBZ3_WINDOW_BACKEND=${window}"
  "-DBZ3_UI_BACKEND=${ui}"
  "-DBZ3_PHYSICS_BACKEND=${physics}"
  "-DBZ3_AUDIO_BACKEND=${audio}"
  "-DBZ3_RENDER_BACKEND=${render}"
)

if [ ! -d "$build_dir" ]; then
  mkdir -p "$build_dir"
  run_configure=true
fi

if [ "$run_configure" = true ]; then
  cmake -S . -B "$build_dir" "${cmake_args[@]}"
fi

cmake --build "$build_dir" -j
