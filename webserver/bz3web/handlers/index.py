from bz3web import auth, config, db, views, webhttp
from bz3web.server_status import is_active


def handle(request):
    if request.method != "GET":
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    settings = config.get_config()
    list_name = settings.get("list_name", "Server List")

    conn = db.connect(db.default_db_path())
    rows = db.list_servers(conn, approved=True)
    conn.close()

    show_inactive = request.query.get("show_inactive", [""])[0] == "1"
    timeout = int(settings.get("heartbeat_timeout_seconds", 120))
    active_count = 0
    inactive_count = 0
    servers = []
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
        entry = {"id": row["id"], "host": row["host"], "port": str(row["port"])}
        if row["name"]:
            entry["name"] = row["name"]
        if row["description"]:
            entry["description"] = row["description"]
        if row["max_players"] is not None:
            entry["max_players"] = row["max_players"]
        if row["num_players"] is not None:
            entry["num_players"] = row["num_players"]
        entry["approved"] = bool(row["approved"])
        entry["active"] = active
        entry["owner"] = row["owner_username"]
        entry["screenshot_thumb"] = row["screenshot_thumb"]
        entry["screenshot_full"] = row["screenshot_full"]
        servers.append(entry)

    servers.sort(key=lambda item: item.get("num_players") if item.get("num_players") is not None else -1, reverse=True)

    user = auth.get_user_from_request(request)
    is_admin = bool(user and user["is_admin"])
    for entry in servers:
        if not is_admin:
            continue
        server_id = entry.get("id")
        entry["actions_html"] = f"""<form method="get" action="/admin/edit">
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">Edit</button>
</form>
<form method="post" action="/admin/delete">
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">Delete</button>
</form>"""
    toggle_url = "/?show_inactive=1" if not show_inactive else "/"
    toggle_label = "Show offline servers" if not show_inactive else "Show online servers"
    header_html = views.header(
        list_name,
        request.path,
        user is not None,
        user_name=auth.display_username(user),
    )
    summary_text = f"<strong>{active_count} online</strong> / {inactive_count} offline"
    cards_html = views.render_server_cards(
        servers,
        header_title="Servers",
        summary_text=summary_text,
        toggle_url=toggle_url,
        toggle_label=toggle_label,
    )
    body = f"""{header_html}
{cards_html}
"""
    return views.render_page("Server List", body)
