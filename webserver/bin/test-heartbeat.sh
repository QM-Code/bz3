#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root_dir="$(cd "${script_dir}/.." && pwd)"
data_file="${root_dir}/test-data.json"

if [[ ! -f "${data_file}" ]]; then
  echo "Missing test-data.json at ${data_file}"
  exit 1
fi

readarray -t servers < <(DATA_FILE="${data_file}" python3 - <<'PY'
import json
import os
import random

data_file = os.environ["DATA_FILE"]
with open(data_file, "r", encoding="utf-8") as handle:
    payload = json.load(handle)
servers = payload.get("servers", [])
for index, entry in enumerate(servers):
    if index % 2 == 0:
        host = str(entry.get("host", "")).strip()
        port = str(entry.get("port", "")).strip()
        max_players = entry.get("max_players")
        if max_players is None:
            max_players = random.randint(10, 32)
        if host and port:
            print(f"{host} {port} {max_players}")
PY
)

if [[ ${#servers[@]} -eq 0 ]]; then
  echo "No servers found in test-data.json"
  exit 1
fi

while true; do
  for entry in "${servers[@]}"; do
    host="$(awk '{print $1}' <<< "${entry}")"
    port="$(awk '{print $2}' <<< "${entry}")"
    max_players="$(awk '{print $3}' <<< "${entry}")"
    players="$((RANDOM % max_players))"
    echo "heartbeat ${host}:${port} players=${players} max=${max_players}"
    response="$(curl -s -w "\n%{http_code}" "http://127.0.0.1:8080/heartbeat?host=${host}&port=${port}&players=${players}&max=${max_players}")"
    body="$(printf "%s" "${response}" | head -n 1)"
    status="$(printf "%s" "${response}" | tail -n 1)"
    if [[ "${status}" != "200" ]] || grep -q '"ok": false' <<< "${body}"; then
      echo "error ${host}:${port} status=${status} response=${body}"
    fi
  done
  sleep 10
done
