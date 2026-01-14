import argparse
import json
import os
import time

from bz3web import auth, config, db


def _load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _normalize(value):
    return str(value).strip() if value is not None else ""


def _parse_int(value):
    if value is None or value == "":
        return None
    try:
        return int(str(value))
    except ValueError:
        return None


def import_data(path):
    payload = _load_json(path)
    users = payload.get("users", [])
    servers = payload.get("servers", [])

    if not isinstance(users, list) or not isinstance(servers, list):
        raise SystemExit("Expected 'users' and 'servers' arrays in the JSON payload.")

    db.init_db(db.default_db_path())
    conn = db.connect(db.default_db_path())
    inserted_users = 0
    skipped_users = 0
    inserted_servers = 0
    skipped_servers = 0
    try:
        admin_username = config.get_config().get("admin_user", "Admin")
        for entry in users:
            if not isinstance(entry, dict):
                skipped_users += 1
                continue
            username = _normalize(entry.get("username"))
            email = _normalize(entry.get("email")).lower()
            password = _normalize(entry.get("password"))
            is_admin = bool(entry.get("is_admin"))
            if not username or not email or not password:
                skipped_users += 1
                continue
            if " " in username:
                skipped_users += 1
                continue
            if username.lower() == admin_username.lower():
                skipped_users += 1
                continue
            if db.get_user_by_username(conn, username) or db.get_user_by_email(conn, email):
                skipped_users += 1
                continue
            digest, salt = auth.new_password(password)
            db.add_user(conn, username, email, digest, salt, is_admin=is_admin)
            inserted_users += 1

        user_rows = conn.execute("SELECT id, username FROM users").fetchall()
        user_ids = {row["username"].lower(): row["id"] for row in user_rows}
        user_names = {row["username"].lower(): row["username"] for row in user_rows}

        for entry in users:
            if not isinstance(entry, dict):
                continue
            owner_id = user_ids.get(_normalize(entry.get("username")).lower())
            if not owner_id:
                continue
            admins = entry.get("admin_list") or []
            if not isinstance(admins, list):
                continue
            for name in admins:
                admin_id = user_ids.get(_normalize(name).lower())
                if not admin_id or admin_id == owner_id:
                    continue
                db.add_user_admin(conn, owner_id, admin_id)

        for entry in servers:
            if not isinstance(entry, dict):
                skipped_servers += 1
                continue
            host = _normalize(entry.get("host"))
            port = _parse_int(entry.get("port"))
            if not host or port is None:
                skipped_servers += 1
                continue
            owner = _normalize(entry.get("owner"))
            owner_key = owner.lower() if owner else ""
            owner_id = user_ids.get(owner_key) if owner else None
            approved = entry.get("approved", True)
            approved_value = 1 if approved else 0
            heartbeat_value = None
            if approved_value:
                raw_heartbeat = entry.get("last_heartbeat")
                heartbeat_value = _parse_int(raw_heartbeat)
            record = {
                "name": entry.get("name") or None,
                "description": entry.get("description") or None,
                "host": host,
                "port": port,
                "plugins": None,
                "max_players": _parse_int(entry.get("max_players")),
                "num_players": _parse_int(entry.get("num_players")),
                "game_mode": None,
                "approved": approved_value,
                "user_id": owner_id,
                "owner_username": user_names.get(owner_key) if owner_id else None,
                "screenshot_original": None,
                "screenshot_full": None,
                "screenshot_thumb": None,
                "last_heartbeat": heartbeat_value,
            }
            db.add_server(conn, record)
            inserted_servers += 1
    finally:
        conn.close()

    return inserted_users, skipped_users, inserted_servers, skipped_servers


def main():
    parser = argparse.ArgumentParser(description="Import users and servers into the bz3web database.")
    parser.add_argument("path", help="Path to JSON file (e.g. test-data.json)")
    args = parser.parse_args()
    path = os.path.abspath(args.path)
    users_ok, users_skip, servers_ok, servers_skip = import_data(path)
    print(f"Imported {users_ok} users; skipped {users_skip}.")
    print(f"Imported {servers_ok} servers; skipped {servers_skip}.")


if __name__ == "__main__":
    main()
