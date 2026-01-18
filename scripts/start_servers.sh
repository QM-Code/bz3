#!/usr/bin/env bash
set -euo pipefail

SERVER_BIN=${SERVER_BIN:-""}
START_PORT=${START_PORT:-8000}
COUNT=${COUNT:-10}
WORLD_DIR=${WORLD_DIR:-""}
DATA_DIR=${DATA_DIR:-""}

usage() {
  cat <<USAGE
Usage: $0 [-b /path/to/bz3-server] [-p start_port] [-n count] [-w world_dir] [-d data_dir]

Defaults:
  -b (auto-detect bz3-server)
  -p 8080
  -n 10
  -w (use -D default world)
  -d (no data dir override)

You can also set SERVER_BIN, START_PORT, COUNT, WORLD_DIR, DATA_DIR env vars.
USAGE
}

while getopts ":b:p:n:w:d:h" opt; do
  case "$opt" in
    b) SERVER_BIN="$OPTARG" ;;
    p) START_PORT="$OPTARG" ;;
    n) COUNT="$OPTARG" ;;
    w) WORLD_DIR="$OPTARG" ;;
    d) DATA_DIR="$OPTARG" ;;
    h) usage; exit 0 ;;
    *) usage; exit 1 ;;
  esac
done

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)

find_server_bin() {
  local candidate
  for candidate in \
    "$ROOT_DIR/bz3-server" \
    "$ROOT_DIR/build/bz3-server" \
    "$ROOT_DIR/build/Debug/bz3-server" \
    "$ROOT_DIR/build/Release/bz3-server"; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done
  find "$ROOT_DIR" -maxdepth 3 -type f -name "bz3-server" -perm -u+x 2>/dev/null | head -n 1
}

if [[ -z "$SERVER_BIN" ]]; then
  SERVER_BIN="$(find_server_bin)"
fi

if [[ -z "$SERVER_BIN" ]]; then
  echo "Server binary not found. Set -b /path/to/bz3-server or SERVER_BIN." >&2
  exit 1
fi

if ! command -v "$SERVER_BIN" >/dev/null 2>&1; then
  if [[ ! -x "$SERVER_BIN" ]]; then
    echo "Server binary not found or not executable: $SERVER_BIN" >&2
    exit 1
  fi
fi

echo "Using server binary: $SERVER_BIN"

pids=()

cleanup() {
  if [[ ${#pids[@]} -gt 0 ]]; then
    kill "${pids[@]}" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

for ((i=0; i<COUNT; i++)); do
  port=$((START_PORT + i))
  args=("-p" "$port")
  if [[ -n "$DATA_DIR" ]]; then
    args+=("-d" "$DATA_DIR")
  fi
  if [[ -n "$WORLD_DIR" ]]; then
    args+=("-w" "$WORLD_DIR")
  else
    args+=("-D")
  fi

  "$SERVER_BIN" "${args[@]}" &
  pids+=("$!")
  echo "Started server on port $port (pid ${pids[-1]})"
  sleep 0.1
 done

wait
