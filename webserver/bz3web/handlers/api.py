import hmac
import time
import urllib.parse

from bz3web import auth, config, db, webhttp


def _handle_auth(request):
    if request.method != "POST":
        return webhttp.json_response({"ok": False, "error": "method_not_allowed"}, status="405 Method Not Allowed")
    form = request.form()
    username = form.get("username", [""])[0].strip()
    email = form.get("email", [""])[0].strip().lower()
    password = form.get("password", [""])[0]
    if not password or (not email and not username):
        return webhttp.json_response({"ok": False, "error": "missing_credentials"}, status="400 Bad Request")
    conn = db.connect(db.default_db_path())
    try:
        if username:
            user = db.get_user_by_username(conn, username)
        else:
            user = db.get_user_by_email(conn, email)
        if not user:
            return webhttp.json_response({"ok": False, "error": "invalid_credentials"}, status="401 Unauthorized")
        if user["is_locked"] or user["deleted"]:
            return webhttp.json_response({"ok": False, "error": "invalid_credentials"}, status="401 Unauthorized")
        if not auth.verify_password(password, user["password_salt"], user["password_hash"]):
            return webhttp.json_response({"ok": False, "error": "invalid_credentials"}, status="401 Unauthorized")
        return webhttp.json_response(
            {"ok": True, "user_id": user["id"], "email": user["email"], "username": user["username"]}
        )
    finally:
        conn.close()


def _handle_heartbeat(request):
    if request.method not in ("GET", "POST"):
        return webhttp.json_response({"ok": False, "error": "method_not_allowed"}, status="405 Method Not Allowed")
    settings = config.get_config()
    debug_heartbeat = bool(settings.get("debug_heartbeat", False))
    num_players = None
    max_players = None
    new_port = None
    if request.method == "GET":
        host = request.query.get("host", [""])[0].strip()
        port_text = request.query.get("port", [""])[0].strip()
        port_missing = not port_text
        players_text = request.query.get("players", [""])[0].strip()
        max_text = request.query.get("max", [""])[0].strip()
        newport_text = request.query.get("newport", [""])[0].strip()
        try:
            port = int(port_text)
            if port < 1 or port > 65535:
                raise ValueError
        except ValueError:
            port = None
    else:
        form = request.form()
        host = form.get("host", [""])[0].strip()
        port_text = form.get("port", [""])[0].strip()
        port_missing = not port_text
        players_text = form.get("players", [""])[0].strip()
        max_text = form.get("max", [""])[0].strip()
        newport_text = form.get("newport", [""])[0].strip()
        try:
            port = int(port_text)
            if port < 1 or port > 65535:
                raise ValueError
        except ValueError:
            port = None

    remote_addr = request.environ.get("REMOTE_ADDR", "")
    if not debug_heartbeat:
        host = remote_addr
    if port is None:
        error = "missing_port" if port_missing else "invalid_port"
        message = (
            "port is required and must be an integer in the range 1-65535"
            if port_missing
            else "port must be an integer in the range 1-65535"
        )
        return webhttp.json_response(
            {
                "ok": False,
                "error": error,
                "message": message,
            },
            status="400 Bad Request",
        )
    if not host:
        message = "host is required for heartbeat requests"
        if debug_heartbeat:
            message = "Heartbeat is running in debug mode. A host parameter must be specified in the query string."
        return webhttp.json_response(
            {
                "ok": False,
                "error": "missing_server",
                "message": message,
            },
            status="400 Bad Request",
        )
    if not debug_heartbeat and host != remote_addr:
        return webhttp.json_response({"ok": False, "error": "host_mismatch"}, status="403 Forbidden")
    if players_text:
        try:
            num_players = int(players_text)
            if num_players < 0:
                raise ValueError
        except ValueError:
            num_players = 0
            return webhttp.json_response(
                {"ok": False, "error": "invalid_players", "message": "players must be a non-negative integer"},
                status="400 Bad Request",
            )
    if max_text:
        try:
            max_players = int(max_text)
            if max_players < 0:
                raise ValueError
        except ValueError:
            return webhttp.json_response(
                {"ok": False, "error": "invalid_max", "message": "max must be a non-negative integer"},
                status="400 Bad Request",
            )
    if max_players is not None and num_players is not None and num_players > max_players:
        return webhttp.json_response(
            {
                "ok": False,
                "error": "invalid_players",
                "message": "players must be less than or equal to max",
            },
            status="400 Bad Request",
        )

    if newport_text:
        try:
            new_port = int(newport_text)
            if new_port < 1 or new_port > 65535:
                raise ValueError
        except ValueError:
            return webhttp.json_response(
                {
                    "ok": False,
                    "error": "invalid_newport",
                    "message": "newport must be an integer in the range 1-65535",
                },
                status="400 Bad Request",
            )
    conn = db.connect(db.default_db_path())
    try:
        server = db.get_server_by_host_port(conn, host, port)
        if not server:
            ports = db.list_ports_by_host(conn, host)
            if ports:
                return webhttp.json_response(
                    {
                        "ok": False,
                        "error": "port_mismatch",
                        "message": (
                            f"Heartbeat received for {host}:{port}, but there is no game registered on port "
                            f"{port} for host {host}."
                        ),
                    },
                    status="404 Not Found",
                )
            host_label = "specified host" if debug_heartbeat else "source host"
            return webhttp.json_response(
                {
                    "ok": False,
                    "error": "host_not_found",
                    "message": f"The {host_label} ({host}) does not exist in the database of registered servers.",
                },
                status="404 Not Found",
            )
        if new_port is not None and new_port != port:
            existing = db.get_server_by_host_port(conn, host, new_port)
            if existing:
                return webhttp.json_response(
                    {
                        "ok": False,
                        "error": "port_in_use",
                        "message": f"newport ({new_port}) is already in use for host {host}",
                    },
                    status="409 Conflict",
                )
            db.update_server_port(conn, server["id"], new_port)
        db.update_heartbeat(conn, server["id"], int(time.time()), num_players=num_players, max_players=max_players)
        return webhttp.json_response({"ok": True})
    finally:
        conn.close()


def _handle_admins(request):
    if request.method not in ("GET", "POST"):
        return webhttp.json_response({"ok": False, "error": "method_not_allowed"}, status="405 Method Not Allowed")
    settings = config.get_config()
    debug_auth = bool(settings.get("debug_auth", False))
    if request.method == "GET":
        if not debug_auth:
            return webhttp.json_response(
                {
                    "ok": False,
                    "error": "method_not_allowed",
                    "message": "GET is disabled unless debug_auth is enabled.",
                },
                status="405 Method Not Allowed",
            )
        username = request.query.get("user", [""])[0].strip()
        password_hash = request.query.get("passhash", [""])[0].strip()
        password_text = request.query.get("password", [""])[0]
    else:
        form = request.form()
        username = form.get("user", [""])[0].strip()
        password_hash = form.get("password", [""])[0].strip() or form.get("passhash", [""])[0].strip()
        password_text = ""
    if not username:
        return webhttp.json_response(
            {"ok": False, "error": "missing_user", "message": "user is required"},
            status="400 Bad Request",
        )
    conn = db.connect(db.default_db_path())
    try:
        user = db.get_user_by_username(conn, username)
        if not user or user["deleted"]:
            return webhttp.json_response(
                {"ok": False, "error": "user_not_found", "message": f"User {username} was not found"},
                status="404 Not Found",
            )
        direct_admins = db.list_user_admins(conn, user["id"])
        admin_names = {admin["username"] for admin in direct_admins}
        for admin in direct_admins:
            if not admin["trust_admins"]:
                continue
            for trusted in db.list_user_admins(conn, admin["admin_user_id"]):
                admin_names.add(trusted["username"])
        return webhttp.json_response({"ok": True, "user": username, "admins": sorted(admin_names)})
    finally:
        conn.close()


def handle(request):
    path = request.path.rstrip("/")
    if path == "/api/auth":
        return _handle_auth(request)
    if path == "/api/heartbeat":
        return _handle_heartbeat(request)
    if path == "/api/admins":
        return _handle_admins(request)
    return webhttp.json_response({"ok": False, "error": "not_found"}, status="404 Not Found")
