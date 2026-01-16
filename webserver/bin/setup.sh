#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
config_path="$(cd "${script_dir}/.." && pwd)/config.json"

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required but was not found in PATH."
  exit 1
fi

if [[ ! -f "${config_path}" ]]; then
  echo "Missing config.json at ${config_path}"
  exit 1
fi

read -r -s -p "Admin password: " admin_password
echo
if [[ -z "${admin_password}" ]]; then
  echo "Password is required."
  exit 1
fi

CONFIG_PATH="${config_path}" ADMIN_PASSWORD="${admin_password}" python3 - <<'PY'
import json
import os
import secrets
import hashlib
import sys

config_path = os.environ.get("CONFIG_PATH")
admin_password = os.environ.get("ADMIN_PASSWORD")

with open(config_path, "r", encoding="utf-8") as handle:
    config = json.load(handle)

data_dir = config.get("data_dir", "data")
uploads_dir = config.get("uploads_dir", "uploads")
base_dir = os.path.dirname(config_path)
if not os.path.isabs(data_dir):
    data_dir = os.path.normpath(os.path.join(base_dir, data_dir))
if not os.path.isabs(uploads_dir):
    uploads_dir = os.path.normpath(os.path.join(base_dir, uploads_dir))
os.makedirs(data_dir, exist_ok=True)
os.makedirs(uploads_dir, exist_ok=True)

salt = secrets.token_hex(16)
password_hash = hashlib.pbkdf2_hmac(
    "sha256", admin_password.encode("utf-8"), salt.encode("utf-8"), 100_000
).hex()
session_secret = secrets.token_hex(32)

config["admin_password_salt"] = salt
config["admin_password_hash"] = password_hash
config["session_secret"] = session_secret
config.setdefault("host", "0.0.0.0")
admin_user = config.get("admin_user") or "Admin"
config["admin_user"] = admin_user

with open(config_path, "w", encoding="utf-8") as handle:
    json.dump(config, handle, indent=2, sort_keys=False)
    handle.write("\n")

os.environ["BZ3WEB_CONFIG"] = config_path
sys.path.insert(0, os.path.dirname(config_path))
from bz3web import db  # noqa: E402

db.init_db(db.default_db_path())
conn = db.connect(db.default_db_path())
admin_row = db.get_user_by_username(conn, admin_user)
if admin_row:
    db.set_user_password(conn, admin_row["id"], password_hash, salt)
    db.set_user_admin(conn, admin_row["id"], True)
else:
    admin_email = f"{admin_user.lower()}@local"
    db.add_user(conn, admin_user, admin_email, password_hash, salt, is_admin=True)
    admin_row = db.get_user_by_username(conn, admin_user)
conn.close()
PY

echo "Updated ${config_path} with admin credentials and session_secret."
