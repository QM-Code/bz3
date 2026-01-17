# BZ3 Community Web Server

This folder contains a small Python webserver that hosts a server list for BZ3 clients and provides account management, submissions, and admin tooling.

## Requirements

- Python 3.8+ (tested with 3.12)
- Pillow (`python3-pil`) for screenshot resizing
- Waitress (`pip install waitress`) for multi-request serving (falls back to wsgiref if missing)

## Directory layout

- `bz3web/`: Python package for the webserver.
- `bin/`: helper scripts.
- `config.json`: runtime configuration (host, port, data paths, heartbeat settings, upload limits, community name, debug flags, server auto-refresh, refresh animation, waitress threads).
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

The server binds to `host` and `port` in `config.json` (default host is `0.0.0.0`). The site header uses `community_name`. Set `servers_auto_refresh` (seconds) to control live refresh for the server list pages; `0` disables. Set `servers_auto_refresh_animate` to `true` to animate list reordering.

3) (Optional) Import users and servers from a JSON file (see `data/test-data.json` for an example):

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
  - `is_admin`
  - `is_locked`, `locked_at`, `deleted`, `deleted_at`
  - `admin_list` (array of usernames or objects with `{ "username", "trust_admins" }`)
- Servers:
  - `name`, `description`, `host`, `port`, `owner`
  - `max_players`, `num_players`, `last_heartbeat` are intentionally omitted (heartbeat fills these)
  - `screenshot_id`

## Scripts

- `bin/setup.sh`: prompts for an admin password, generates the hash/salt, and writes them plus a new `session_secret` to `config.json`.
- `bin/start.sh`: runs the WSGI server from any directory.
- `bin/import-data.sh`: imports users and servers from a JSON file into the database (`-m/--merge` to merge, `-o/--overwrite` to replace).
- `bin/export-data.sh`: exports the database to a JSON file for backup/restore (`-z/--zip` to zip the output).
- `bin/clean-images.sh`: deletes unreferenced screenshot files in the uploads directory (use `--dry-run` to preview).
- `bin/test-heartbeat.sh`: sends heartbeat pings for sample servers (dev helper).

## URLs

- `/` redirects to `/servers`.
- `/servers` shows the server list in HTML.
- `/servers/<server>` shows a server profile page.
- `/api/servers` returns JSON for clients.
- `/login`, `/register`, `/forgot`, `/reset` handle account auth.
- `/users/<username>` shows a public page of that user's servers and doubles as the signed-in user's account page when viewing your own profile.
- `/users/<username>/edit` lets a user manage personal settings (email/password); admins can edit users here too.
- `/submit` submits a new server (login required).
- `/server/edit?id=<id>` edits an existing server (owner or admin).
- `/server/delete?id=<id>` deletes a server (owner or admin).
- `/users` lists users; admins can create/edit users here.
- `/api/admins` returns the admin list for the server identified by `host` + `port` (GET or POST).
- `/api/heartbeat` updates server state from game servers (see `config.json` for debug/host rules).
- `/api/auth` validates username/password for game server auth (POST only unless `debug_auth` is enabled).
- `/api/user_registered` reports whether a username exists on the community server (GET or POST).

## API auth parameters

`/api/auth` accepts the same parameters via GET or POST (GET is only enabled when `debug_auth` is true).

Parameters:
- `username`: account username (use either `username` or `email`)
- `email`: account email (use either `username` or `email`)
- `passhash`: PBKDF2-SHA256 hash (required when `debug_auth` is false)
- `password`: plaintext password (only accepted when `debug_auth` is true)
- `world`: optional world/server name; includes `local_admin` in the response when supplied

Example response:
```json
{
  "ok": true,
  "community_admin": false,
  "local_admin": true
}
```
- `GET /api/user_registered`: query param `username` (response includes `registered`, `community_name`, and `salt` when registered).
