# BZ3 Community Web Server

This folder contains a small Python webserver that hosts a server list for BZ3 clients and provides account management, submissions, and admin tooling.

## Requirements

- Python 3.8+ (tested with 3.12). On macOS/Linux, use `python3`. On Windows, use `py -3` or `python`.
- Pillow (`python3-pil`) for screenshot resizing
- Waitress (`pip install waitress`) for multi-request serving (falls back to wsgiref if missing)

## Directory layout

- `bz3web/`: Python package for the webserver.
- `bin/`: helper scripts.
- `config.json`: runtime configuration (host, data paths, heartbeat settings, upload limits, debug flags, server auto-refresh, HTTP server threads).
- `communities/`: per-community storage roots.
- `communities/<name>/config.json`: per-community overrides (`community_name`, `community_description`, `port`, `session_secret`, `admin_user`).
  - Sample server configurations live under `INSTALL_ROOT/communities/example[1-5]`.

The database and uploads live under a community directory (e.g. `communities/example1/data` and `communities/example1/uploads`) using `data_dir` and `uploads_dir` from `config.json`. The default database filename is `bz3web.db`.

Community directories must be user-writable. The command-line tools require a community directory argument and will use the selected community. If you run the Python modules directly, call `config.set_community_dir(...)` before loading the app.

## Admin user behavior

- `admin_user` identifies the **root admin name** used for admin propagation (trusted admins become community admins).
- Changing `admin_user` does **not** automatically grant admin privileges; the user must already be an admin in the database.
- The new root’s admin graph is applied when admin flags are recomputed (e.g., via profile/admin actions).

## Getting started

1) Run the initialization script to create the community config and admin credentials. The community directory must be empty (it will create `config.json`, plus `data/` + `uploads/`). Provide the community directory (for system-wide installs, use a per-user path like `~/bz3/communities/<name>`):

```
python3 ./bin/initialize.py /path/to/communities/example1
```

2) Start the webserver:

```
python3 ./bin/start.py ./communities/example1
```

Windows (PowerShell):

```
py -3 .\bin\start.py .\communities\example1
```

The server binds to `host` in `config.json` and `port` from the community config (default host is `0.0.0.0`). The site header uses `community_name`. Set `pages.servers.auto_refresh` (seconds) to control live refresh for the server list pages; `0` disables. Set `pages.servers.auto_refresh_animate` to `true` to animate list reordering.

3) (Optional) Restore users and servers from a JSON file (see `communities/example1/data/test-data.json` for an example). `db-restore.py` requires a fresh database, while `db-merge.py` updates an existing one:

```
python3 ./bin/db-restore.py ./communities/example1 -f /path/to/data/test-data.json
```

Merge example:

```
python3 ./bin/db-merge.py ./communities/example1 -f /path/to/data/test-data.json
```

4) (Optional) Export the database to a JSON file:

```
python3 ./bin/db-snapshot.py ./communities/example1 /path/to/export.json
```

If no output path is provided, `bin/db-snapshot.py` writes to `communities/<name>/data/snapshot-<timestamp>.json`. Use `-z` to generate a zip file instead of the raw JSON. That export can be re-imported by `bin/db-restore.py` to restore the site (uploads must be restored separately).

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

- `bin/initialize.py`: prompts for community defaults and admin credentials, writes the community `config.json`, and seeds the database (requires a community directory argument).
- `bin/start.py`: runs the WSGI server from any directory (requires a community directory argument).
- `bin/db-restore.py`: restores users and servers from a JSON file into a fresh database; requires `-f <json-file>` and a community directory argument (fails if the database exists).
- `bin/db-merge.py`: merges users and servers from a JSON file into the existing database; requires `-f <json-file>` and a community directory argument.
- `bin/db-snapshot.py`: exports the database to a JSON file for backup/restore (`-z` to zip the output).
- `bin/db-makedata.py`: generates random users and servers for an existing database (`-s <server-num>` and `-u <user-num>`).
- `bin/db-test-heartbeat.py`: sends heartbeat pings for roughly half of the registered servers (dev helper).
- `bin/clean-images.py`: deletes unreferenced screenshot files in the uploads directory (use `--dry-run` to preview).

## Agent prompts

Repo-level guidance lives in `AGENTS.md`. Task-specific prompts live under `docs/agent-prompts/` (see the Webserver prompt).

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
- `/api/auth` validates username/password for game server auth (POST only unless `debug.auth` is enabled).
- `/api/user_registered` reports whether a username exists on the community server (GET or POST).

## API auth parameters

`/api/auth` accepts the same parameters via GET or POST (GET is only enabled when `debug.auth` is true).

Parameters:
- `username`: account username (use either `username` or `email`)
- `email`: account email (use either `username` or `email`)
- `passhash`: PBKDF2-SHA256 hash (required when `debug.auth` is false)
- `password`: plaintext password (only accepted when `debug.auth` is true)
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
