# BZ3 Community Web Server

Python WSGI webserver for the BZ3 community list (accounts, submissions, admin tooling).

## Requirements

- Python 3.8+ (tested with 3.12). macOS/Linux: `python3`. Windows: `py -3` or `python`.
- Pillow (`python3-pil`) for screenshot resizing.
- Waitress (`pip install waitress`) for multi-request serving (falls back to wsgiref).

## Configuration model

There are **two** config layers:

- **Distribution config**: `webserver/config.json`  
  This is the authoritative source of defaults and must include required keys. End users should not edit it.

- **Community config**: `<community>/config.json`  
  Per-community overrides written by `bin/initialize.py`.

If a required key is missing from `webserver/config.json`, the server **errors on startup** with a clear message.

### Required keys in `webserver/config.json`

- `host`
- `port`
- `data_dir`
- `uploads_dir`
- `heartbeat_timeout_seconds`
- `pages.servers.overview_max_chars`
- `pages.servers.auto_refresh`
- `pages.servers.auto_refresh_animate`
- `uploads.screenshots.limits.*` and `uploads.screenshots.{thumbnail,full}.*`
- `debug.*`
- `httpserver.threads`
- `rate_limits.*`
- `placeholders.*` (optional; missing placeholders are simply omitted)

### Community config fields

`bin/initialize.py` creates a minimal community config with:

- `community_name`
- `port`
- `session_secret`
- `admin_user`

You can also add:

- `community_description`

### Example: full `webserver/config.json`

```json
{
  "community_name": "COMMUNITY NAME",
  "community_description": "Enter a community description here.",
  "admin_user": "Admin",
  "host": "0.0.0.0",
  "port": 8080,
  "data_dir": "data",
  "uploads_dir": "uploads",
  "uploads": {
    "screenshots": {
      "limits": {
        "min_bytes": 0,
        "max_bytes": 8388608,
        "min_width": 640,
        "min_height": 360,
        "max_width": 3840,
        "max_height": 2160
      },
      "thumbnail": {
        "width": 480,
        "height": 270
      },
      "full": {
        "width": 1600,
        "height": 900
      }
    }
  },
  "debug": {
    "heartbeat": false,
    "auth": false
  },
  "pages": {
    "servers": {
      "overview_max_chars": 160,
      "auto_refresh": 10,
      "auto_refresh_animate": true
    }
  },
  "rate_limits": {
    "login": {
      "window_seconds": 60,
      "max_requests": 10
    },
    "register": {
      "window_seconds": 60,
      "max_requests": 5
    },
    "forgot": {
      "window_seconds": 60,
      "max_requests": 5
    },
    "reset": {
      "window_seconds": 60,
      "max_requests": 5
    },
    "api_auth": {
      "window_seconds": 60,
      "max_requests": 30
    },
    "api_user_registered": {
      "window_seconds": 60,
      "max_requests": 60
    }
  },
  "placeholders": {
    "add_server": {
      "host": "example.com or 10.0.0.5",
      "port": "5154",
      "name": "Required unique name",
      "overview": "Short summary for listings",
      "description": "Optional description"
    },
    "users": {
      "reset_password": "Leave blank to keep current password",
      "new_password": "Leave blank to keep current password"
    }
  },
  "heartbeat_timeout_seconds": 120,
  "httpserver": {
    "threads": 8
  }
}
```

### Example: minimal community `config.json` (created by `bin/initialize.py`)

```json
{
  "community_name": "My Community",
  "port": 8080,
  "session_secret": "CHANGE_ME",
  "admin_user": "Admin"
}
```

## Directory layout

- `bz3web/`: Python package.
- `bin/`: CLI tools.
- `config.json`: distribution defaults (authoritative).
- `communities/`: per-community storage roots.
  - `communities/<name>/config.json`: per-community overrides.
  - Sample community data lives under `INSTALL_ROOT/communities/example[1-5]`.

Community directories are **user-writable** and contain:
- `config.json`
- `data/` (SQLite DB)
- `uploads/` (screenshots)

## Getting started

1) Initialize a community directory (must be empty). This writes `config.json`, creates `data/` + `uploads/`, and seeds the admin user:

```
python3 ./bin/initialize.py /path/to/communities/example1
```

2) Start the server:

```
python3 ./bin/start.py ./communities/example1
```

Windows (PowerShell):

```
py -3 .\bin\start.py .\communities\example1
```

You can override the port at startup:

```
python3 ./bin/start.py ./communities/example1 -p 8081
```

## Data import/export

Restore a fresh database from JSON:

```
python3 ./bin/db-restore.py ./communities/example1 -f /path/to/data.json
```

Merge JSON into an existing database:

```
python3 ./bin/db-merge.py ./communities/example1 -f /path/to/data.json
```

Snapshot the database:

```
python3 ./bin/db-snapshot.py ./communities/example1 /path/to/export.json
```

If no output path is provided, `db-snapshot.py` writes to:
`communities/<name>/data/snapshot-<timestamp>.json`. Use `-z` to write a zip.

## JSON format

Import/export JSON matches `communities/*/data/test-data.json` and includes:

- Users:
  - `username`, `email`
  - `password` (plaintext, import only) **or** `password_hash` + `password_salt`
  - `is_admin`
  - `is_locked`, `locked_at`, `deleted`, `deleted_at`
  - `admin_list` (array of usernames or `{ "username", "trust_admins" }`)
- Servers:
  - `name`, `overview`, `description`, `host`, `port`, `owner`
  - `screenshot_id`
  - `max_players`, `num_players`, `last_heartbeat` omitted (heartbeat populates them)

## Scripts

- `bin/initialize.py`: prompts for community defaults + admin credentials, writes community config, seeds DB.
- `bin/start.py`: starts the WSGI server (`-p <port>` optional).
- `bin/db-restore.py`: restore DB from JSON (`-f <json-file>` required, fails if DB exists).
- `bin/db-merge.py`: merge JSON into existing DB (`-f <json-file>` required).
- `bin/db-snapshot.py`: export DB to JSON (`-z` to zip).
- `bin/db-makedata.py`: generate random users/servers (`-s <server-num>`, `-u <user-num>`).
- `bin/db-test-heartbeat.py`: repeatedly heartbeat about half of servers (dev helper).
- `bin/clean-images.py`: delete unreferenced screenshots (`--dry-run` supported).

## Admin user behavior

- `admin_user` is the **root admin name** for admin propagation.
- Changing `admin_user` does **not** grant admin rights; the DB flag controls that.
- Admin propagation updates when admin flags are recomputed.

## URLs

- `/` → `/servers`
- `/servers` HTML list
- `/servers/<server>` server profile
- `/api/servers` JSON list (overview only)
- `/api/server/<name>` JSON for a single server (overview + full description)
- `/submit` submit a server (login required)
- `/server/edit?id=<id>` edit server (owner/admin)
- `/server/delete?id=<id>` delete server (owner/admin)
- `/users` list users (admin only)
- `/users/<username>` user profile
- `/users/<username>/edit` personal settings
- `/register`, `/login`, `/logout`, `/forgot`, `/reset` auth
- `/api/admins` admin list for a host+port
- `/api/heartbeat` game server heartbeat
- `/api/auth` game server auth (POST only unless `debug.auth` is true)
- `/api/user_registered` check username

## Notes

- Markdown is rendered on `/servers/<server>` (description) and `/info` (community description) with sanitization.
- Placeholders are configurable under `placeholders.*` in `webserver/config.json`.
- `host` may be `0.0.0.0` for binding, but clients should use a reachable address.
