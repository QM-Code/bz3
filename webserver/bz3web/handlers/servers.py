from bz3web import config, db, webhttp
from bz3web.server_status import is_active


def handle(request):
    if request.method != "GET":
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    settings = config.get_config()
    list_name = settings.get("list_name", "Server List")

    conn = db.connect(db.default_db_path())
    rows = db.list_servers(conn, approved=True)
    conn.close()

    servers = []
    timeout = int(settings.get("heartbeat_timeout_seconds", 120))
    for row in rows:
        if not is_active(row, timeout):
            continue
        entry = {"host": row["host"], "port": str(row["port"])}
        if row["name"]:
            entry["name"] = row["name"]
        if row["description"]:
            entry["description"] = row["description"]
        if row["game_mode"]:
            entry["game_mode"] = row["game_mode"]
        if row["max_players"] is not None:
            entry["max_players"] = row["max_players"]
        if row["num_players"] is not None:
            entry["num_players"] = row["num_players"]
        servers.append(entry)

    payload = {"name": list_name, "servers": servers}
    return webhttp.json_response(payload)
