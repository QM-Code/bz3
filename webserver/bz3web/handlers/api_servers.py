from bz3web import config, db, webhttp
from bz3web.server_status import is_active


def handle(request):
    if request.method != "GET":
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    settings = config.get_config()
    community_name = settings.get("community_name", "Server List")

    owner = request.query.get("owner", [""])[0].strip()
    show_inactive = request.query.get("show_inactive", [""])[0] == "1"

    conn = db.connect(db.default_db_path())
    try:
        if owner:
            user = db.get_user_by_username(conn, owner)
            if not user or user["deleted"]:
                return webhttp.json_response(
                    {
                        "ok": False,
                        "error": "user_not_found",
                        "message": f"No user named {owner}.",
                    },
                    status="404 Not Found",
                )
            rows = db.list_user_servers(conn, user["id"])
        else:
            rows = db.list_servers(conn)
    finally:
        conn.close()

    servers = []
    timeout = int(settings.get("heartbeat_timeout_seconds", 120))
    active_count = 0
    inactive_count = 0
    for row in rows:
        active = is_active(row, timeout)
        if active:
            active_count += 1
        else:
            inactive_count += 1
        if show_inactive:
            if active:
                continue
        else:
            if not active:
                continue
        entry = {
            "id": row["id"],
            "name": row["name"],
            "owner": row["owner_username"],
            "host": row["host"],
            "port": str(row["port"]),
            "active": active,
        }
        if row["description"]:
            entry["description"] = row["description"]
        if row["max_players"] is not None:
            entry["max_players"] = row["max_players"]
        if row["num_players"] is not None:
            entry["num_players"] = row["num_players"]
        if row["screenshot_id"] is not None:
            entry["screenshot_id"] = row["screenshot_id"]
        servers.append(entry)

    servers.sort(key=lambda item: item.get("num_players", -1), reverse=True)

    payload = {
        "community_name": community_name,
        "active_count": active_count,
        "inactive_count": inactive_count,
        "servers": servers,
    }
    return webhttp.json_response(payload)
