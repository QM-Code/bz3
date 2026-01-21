from bz3web import config, db, webhttp
from bz3web.server_status import is_active


def handle(request):
    if request.method != "GET":
        return webhttp.json_error("method_not_allowed", status="405 Method Not Allowed")

    username = request.query.get("name", [""])[0].strip()
    if not username:
        return webhttp.json_error("missing_name", status="400 Bad Request")

    settings = config.get_config()
    timeout = int(config.require_setting(settings, "heartbeat.timeout_seconds"))
    overview_max = int(config.require_setting(settings, "pages.servers.overview_max_chars"))

    with db.connect_ctx() as conn:
        user = db.get_user_by_username(conn, username)
        if not user or user["deleted"]:
            return webhttp.json_error("not_found", status="404 Not Found")
        rows = db.list_user_servers(conn, user["id"])

    active_count = 0
    inactive_count = 0
    servers = []
    for row in rows:
        active = is_active(row, timeout)
        if active:
            active_count += 1
        else:
            inactive_count += 1
        overview = row["overview"] or ""
        if overview and len(overview) > overview_max:
            overview = overview[:overview_max]
        entry = {
            "id": row["id"],
            "name": row["name"],
            "owner": row["owner_username"],
            "host": row["host"],
            "port": str(row["port"]),
            "overview": overview,
            "active": active,
        }
        if row["max_players"] is not None:
            entry["max_players"] = row["max_players"]
        if row["num_players"] is not None:
            entry["num_players"] = row["num_players"]
        if row["screenshot_id"] is not None:
            entry["screenshot_id"] = row["screenshot_id"]
        servers.append(entry)

    servers.sort(key=lambda item: item.get("num_players", -1), reverse=True)

    return webhttp.json_response(
        {
            "ok": True,
            "user": {"id": user["id"], "username": user["username"]},
            "active_count": active_count,
            "inactive_count": inactive_count,
            "servers": servers,
        }
    )
