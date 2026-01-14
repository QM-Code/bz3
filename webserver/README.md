# BZ3 Server List Web

This folder contains a small Python webserver that hosts a server list for BZ3 clients and provides account management, submissions, and admin tooling.

## Requirements

- Python 3.8+ (tested with 3.12)
- Pillow (`python3-pil`) for screenshot resizing

## Directory layout

- `bz3web/`: Python package for the webserver.
- `bin/`: helper scripts.
- `config.json`: runtime configuration (host, port, data paths, heartbeat settings, upload limits, community name, debug auth).
- `data/`: database storage (configurable).
- `uploads/`: screenshot storage (configurable).

The database and uploads live outside the code directory by default using `data_dir` and `uploads_dir` in `config.json`. The default database filename is `bz3web.db`.

## Getting started

1) Run the setup script to create admin credentials and a session secret:

```
./bin/setup.sh
```

2) Start the webserver:

```
./bin/start.sh
```

The server binds to `host` and `port` in `config.json` (default host is `0.0.0.0`). The site header uses `community_name`.

3) (Optional) Import users and servers from a JSON file:

```
./bin/import-data.sh /path/to/data/test-data.json
```

4) (Optional) Export the database to a JSON file:

```
./bin/export-data.sh /path/to/export.json
```

If no output path is provided, `bin/export-data.sh` writes to `data/snapshot-<timestamp>.json`. Use `-z/--zip` to generate a zip file instead of the raw JSON. That export can be re-imported by `bin/import-data.sh` to restore the site (uploads must be restored separately).

## JSON format

The import/export format is modeled after `data/test-data.json` and includes all site data:

- Users:
  - `username`, `email`
  - either `password` (plaintext for import only) or `password_hash` + `password_salt`
  - `auto_approve`, `is_admin`, `admin_list_public`
  - `admin_list` (array of usernames or objects with `{ "username", "trust_admins" }`)
- Servers:
  - `name`, `description`, `host`, `port`, `owner`
  - `max_players`, `num_players`, `last_heartbeat` are intentionally omitted (heartbeat fills these)
  - `screenshot_id`
  - optional `plugins`, `game_mode` fields are accepted if present

## Scripts

- `bin/setup.sh`: prompts for an admin password, generates the hash/salt, and writes them plus a new `session_secret` to `config.json`.
- `bin/start.sh`: runs the WSGI server from any directory.
- `bin/import-data.sh`: imports users and servers from a JSON file into the database (`-m/--merge` to merge, `-o/--overwrite` to replace).
- `bin/export-data.sh`: exports the database to a JSON file for backup/restore (`-z/--zip` to zip the output).
- `bin/clean-images.sh`: deletes unreferenced screenshot files in the uploads directory (use `--dry-run` to preview).
- `bin/test-heartbeat.sh`: sends heartbeat pings for sample servers (dev helper).

## URLs

- `/` shows the server list in HTML.
- `/servers` returns JSON for clients.
- `/login`, `/register`, `/forgot`, `/reset` handle account auth.
- `/account` shows the signed-in user's servers and admin list.
- `/user?name=<username>` shows a public page of that user's servers.
- `/submit` submits a new server (login required).
- `/server/edit?id=<id>` edits an existing server (owner or admin).
- `/server/delete?id=<id>` deletes a server (owner or admin).
- `/users` lists users; admins can create/edit users here.
- `/admins` returns the admin list for a user. If the user's list is not public, send `user` and `password` (hash) via POST. When `debug_auth` is true, GET can also use `passhash` or `password` (plaintext) for testing.
- `/heartbeat` updates server state from game servers (see `config.json` for debug/host rules).
