#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root_dir="$(cd "${script_dir}/.." && pwd)"
export PYTHONPATH="${root_dir}"

readarray -t servers < <(python3 - <<'PY'
import random
import sqlite3

from bz3web import db

conn = db.connect(db.default_db_path())
servers = conn.execute("SELECT host, port, max_players FROM servers ORDER BY id").fetchall()
conn.close()

for index, entry in enumerate(servers):
    if index % 2 == 0:
        host = str(entry["host"]).strip() if entry["host"] is not None else ""
        port = str(entry["port"]).strip() if entry["port"] is not None else ""
        max_players = entry["max_players"]
        if max_players is None:
            max_players = random.randint(10, 32)
        if host and port:
            print(f"{host} {port} {max_players}")
PY
)

if [[ ${#servers[@]} -eq 0 ]]; then
  echo "No servers found in the database"
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
