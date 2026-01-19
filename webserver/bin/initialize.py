#!/usr/bin/env python3
import getpass
import hashlib
import json
import os
import secrets
import sys


def _ensure_community_dir(path):
    if not path:
        raise SystemExit("usage: initialize.py <community-directory>")
    if not os.path.isdir(path):
        answer = input(f"Create {path} [Y/n] ").strip().lower()
        if answer not in ("", "y", "yes"):
            raise SystemExit("Cancelled")
        os.makedirs(path, exist_ok=True)
        return True
    if os.listdir(path):
        raise SystemExit("Directory must be empty.")
    return False


def _cleanup_paths(paths):
    for path in reversed(paths):
        if os.path.isdir(path):
            try:
                os.rmdir(path)
            except OSError:
                pass
        else:
            try:
                os.remove(path)
            except OSError:
                pass


def _ensure_dir(path):
    if os.path.isdir(path):
        return False
    os.makedirs(path, exist_ok=True)
    return True


def _prompt_text(label, default_value):
    response = input(f"{label} [{default_value}]: ").strip()
    return response or default_value


def _prompt_port(default_value):
    while True:
        response = input(f"Server port [{default_value}]: ").strip()
        if not response:
            return default_value
        try:
            port = int(response)
        except ValueError:
            print("Port must be an integer.")
            continue
        if 1 <= port <= 65535:
            return port
        print("Port must be between 1 and 65535.")


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: initialize.py <community-directory>")
    directory = sys.argv[1]

    created_paths = []
    try:
        created_dir = _ensure_community_dir(directory)
        if created_dir:
            created_paths.append(directory)

        root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        config_path = os.path.join(root_dir, "config.json")
        if not os.path.isfile(config_path):
            raise SystemExit(f"Missing config.json at {config_path}")

        community_name = _prompt_text("Community name", "My Community")
        server_port = _prompt_port(8080)
        admin_user = _prompt_text("Admin username", "Admin")

        try:
            admin_password = getpass.getpass("Admin password: ")
        except KeyboardInterrupt:
            raise
        if not admin_password:
            raise SystemExit("Password is required.")

        with open(config_path, "r", encoding="utf-8") as handle:
            config = json.load(handle)

        data_dir = config.get("data_dir", "data")
        uploads_dir = config.get("uploads_dir", "uploads")
        if not os.path.isabs(data_dir):
            data_dir = os.path.normpath(os.path.join(directory, data_dir))
        if not os.path.isabs(uploads_dir):
            uploads_dir = os.path.normpath(os.path.join(directory, uploads_dir))
        if _ensure_dir(data_dir):
            created_paths.append(data_dir)
        if _ensure_dir(uploads_dir):
            created_paths.append(uploads_dir)

        salt = secrets.token_hex(16)
        password_hash = hashlib.pbkdf2_hmac(
            "sha256", admin_password.encode("utf-8"), salt.encode("utf-8"), 100_000
        ).hex()
        session_secret = secrets.token_hex(32)

        community_config = {
            "community_name": community_name,
            "community_description": "",
            "port": server_port,
            "session_secret": session_secret,
            "admin_user": admin_user,
        }
        community_config_path = os.path.join(directory, "config.json")
        with open(community_config_path, "w", encoding="utf-8") as handle:
            json.dump(community_config, handle, indent=2)
            handle.write("\n")
        created_paths.append(community_config_path)

        if root_dir not in sys.path:
            sys.path.insert(0, root_dir)

        from bz3web import cli
        from bz3web import db

        cli.bootstrap(directory, "usage: initialize.py <community-directory>")
        db.init_db(db.default_db_path())
        conn = db.connect(db.default_db_path())
        try:
            admin_row = db.get_user_by_username(conn, admin_user)
            if admin_row:
                db.set_user_password(conn, admin_row["id"], password_hash, salt)
                db.set_user_admin(conn, admin_row["id"], True)
            else:
                admin_email = f"{admin_user.lower()}@local"
                db.add_user(conn, admin_user, admin_email, password_hash, salt, is_admin=True)
        finally:
            conn.close()

        print(f"Initialized {directory} and updated {community_config_path}.")
    except KeyboardInterrupt:
        _cleanup_paths(created_paths)
        print()
        print("Canceled")


if __name__ == "__main__":
    main()
