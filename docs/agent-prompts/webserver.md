# Webserver

Use this prompt when working on the community webserver in `webserver/`.

## Start here
1) Read and summarize `webserver/README.md` and `webserver/TODO.md`.
2) Scan `webserver/bz3web/` (handlers, views, db, auth, router, tools, static) and note drift from the docs.
3) Only after that, proceed with new tasks.

## Key context
- Python WSGI app under `webserver/bz3web/`; `waitress` if available, fallback to `wsgiref`.
- Config defaults live in `webserver/config.json` and are authoritative; avoid hardcoded defaults in code.
- Language strings live under `webserver/strings/<lang>.json` (distribution) and `<community>/strings/<lang>.json` (overrides). Missing keys fall back to `strings/en.json`.
- Database lives under `<community>/data/bz3web.db`, uploads under `<community>/uploads/`.
- `/` redirects to `/servers`. `/servers` is HTML list. `/api/servers` is JSON list (overview only). `/api/servers/<name|id>` returns a full server record. `/api/users/<name>` returns a user + server list. `/api/admins` takes host+port and returns owner’s admin list (1-level trust). `/api/auth` accepts POST `passhash` only; GET is allowed only when `debug.auth` is true and accepts `password` or `passhash`.
- Host+port is enforced unique at DB level via unique index + CHECK constraints for player counts.
- Deleted users are hidden from public profile and excluded from server lists and admin lists. Locked users cannot log in.
- CSRF protection is implemented for both authenticated and unauthenticated forms.
- Server list rendering is “widgetized” in `views.render_server_section` and used on `/servers` and `/users/<username>`.

## Guardrails
- Do not store plaintext passwords.
- Treat `webserver/config.json` as the authoritative source of defaults; do not hardcode any configurable defaults in code. If a value is missing, prefer surfacing a clear error instead of falling back.
- Strings should be configurable via `strings/<lang>.json`; avoid hardcoding UI text.

## Sanity checks
- If `python` is missing, try `python3`.
- Quick check: `python3 -m py_compile` on edited files.
- Use `tests/validate_strings.py` after touching translation files.
