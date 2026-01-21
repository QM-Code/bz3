from bz3web import config, db, webhttp
from bz3web.server_status import is_active


def handle(request):
    if request.method != "GET":
        return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")

    name = request.query.get("name", [""])[0].strip()
    server_id = request.query.get("id", [""])[0].strip()
    if not name and not server_id:
        return webhttp.json_error("missing_name", status="400 Bad Request")
    if server_id and not server_id.isdigit():
        return webhttp.json_error("invalid_id", status="400 Bad Request")

    with db.connect_ctx() as conn:
        if server_id:
            server = db.get_server(conn, int(server_id))
        else:
            server = db.get_server_by_name(conn, name)
        if not server:
            return webhttp.json_error("not_found", status="404 Not Found")

    settings = config.get_config()
    timeout = int(config.require_setting(settings, "heartbeat.timeout_seconds"))
    active = is_active(server, timeout)

    payload = {
        "ok": True,
        "server": {
            "id": server["id"],
            "name": server["name"],
            "overview": server["overview"],
            "description": server["description"],
            "host": server["host"],
            "port": server["port"],
            "owner": server["owner_username"],
            "max_players": server["max_players"],
            "num_players": server["num_players"],
            "last_heartbeat": server["last_heartbeat"],
            "screenshot_id": server["screenshot_id"],
            "active": active,
        },
    }
    return webhttp.json_response(payload)
