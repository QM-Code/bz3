# Webserver

Use this prompt when working on the community webserver in `webserver/`.

## Start here
1) Read and summarize `webserver/README.md` and `webserver/TODO.md`.
2) Scan `webserver/bz3web/` (handlers, views, db, auth, router, tools, static) and note drift from the docs.
3) Only after that, proceed with new tasks.

## Key context
- Python WSGI app under `webserver/bz3web/`; `waitress` if available, fallback to `wsgiref`.
- Config in `webserver/config.json` (community_name, host/port, data_dir/uploads_dir, heartbeat, debug_auth, servers_auto_refresh, etc.).
- Database in `data/` (default `bz3web.db`), uploads in `uploads/` (configurable).
- `/` redirects to `/servers`. `/servers` is HTML list. `/api/servers` is JSON list. `/api/admins` takes host+port and returns owner’s admin list (1-level trust). `/api/auth` accepts POST `passhash` only; GET is allowed only when `debug_auth` is true and accepts `password` or `passhash`.
- Host+port is enforced unique at DB level via unique index.
- Deleted users are hidden from public profile and excluded from server lists and admin lists. Locked users cannot log in.
- CSRF protection is implemented for logged-in POST actions (server edit/delete, submit, user admin toggles, user edit/create/delete/lock, admin list changes). TODO for CSRF on unauthenticated forms.
- Server list rendering is “widgetized” in `views.render_server_section` and used on `/servers` and `/users/<username>`.

## Guardrails
- Do not store plaintext passwords.
- Keep unauthenticated forms in mind (CSRF TODO).
- Treat `webserver/config.json` as the authoritative source of defaults; do not hardcode any configurable defaults in code. If a value is missing, prefer surfacing a clear error instead of falling back.

## Sanity checks
- If `python` is missing, try `python3`.
- Quick check: `python3 -m py_compile` on edited files.
