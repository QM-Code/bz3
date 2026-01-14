# BZ3 Server List Web

This folder contains a small Python webserver that hosts a server list for BZ3 clients and provides submit/admin pages.

## Requirements

- Python 3.8+ (tested with 3.12)

## Getting started

1) Run the setup script to create admin credentials and a session secret:

```
./setup.sh
```

2) Start the webserver:

```
./bin/start.sh
```

The server will bind to the host/port in `config.json` (default host is `0.0.0.0`).

User accounts require a unique username and email. Users must be signed in to submit servers.

By default, the database and uploads live outside the code directory using `data_dir` and `uploads_dir` in `config.json`.

3) (Optional) Import users and servers from a JSON file:

```
./bin/import-data.sh /path/to/test-data.json
```

Example `test-data.json`:

```json
{
  "users": [
    {
      "username": "pilot",
      "email": "pilot@example.com",
      "password": "change_me",
      "admin_list": ["wingmate", "navigator"]
    }
  ],
  "servers": [
    {
      "name": "Drift Basin",
      "description": "Open basin with long arcs.",
      "host": "192.168.1.100",
      "port": "5154",
      "max_players": "24",
      "num_players": "8",
      "owner": "pilot"
    }
  ]
}
```

## Scripts

- `setup.sh`: prompts for an admin password, generates the hash/salt, and writes them plus a new `session_secret` to `config.json`.
- `bin/start.sh`: runs the WSGI server from any directory.
- `bin/import-data.sh`: imports users and servers from a JSON file into the database.

## URLs

- `/` shows the approved servers in a formatted HTML view.
- `/servers` returns the JSON server list for clients.
- `/submit` lets server owners submit entries.
- `/admin/login` lets the admin approve/edit entries.
