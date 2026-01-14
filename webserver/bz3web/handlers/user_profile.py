from urllib.parse import quote

from bz3web import auth, config, db, views, webhttp
from bz3web.server_status import is_active


def handle(request):
    if request.method != "GET":
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    username = request.query.get("name", [""])[0].strip()
    if not username:
        return webhttp.html_response("<h1>Missing user</h1>", status="400 Bad Request")

    conn = db.connect(db.default_db_path())
    try:
        user = db.get_user_by_username(conn, username)
        if not user:
            return webhttp.html_response("<h1>User not found</h1>", status="404 Not Found")
        servers = db.list_user_servers(conn, user["id"], user["username"])
    finally:
        conn.close()

    timeout = int(config.get_config().get("heartbeat_timeout_seconds", 120))
    show_inactive = request.query.get("show_inactive", [""])[0] == "1"
    active_count = 0
    inactive_count = 0
    entries = []
    for server in servers:
        active = is_active(server, timeout)
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
            "id": server["id"],
            "host": server["host"],
            "port": str(server["port"]),
            "name": server["name"],
            "description": server["description"],
            "max_players": server["max_players"],
            "num_players": server["num_players"],
            "active": active,
            "owner": server["owner_username"] or user["username"],
            "screenshot_id": server["screenshot_id"],
        }
        entries.append(entry)
    encoded_user = quote(user["username"])
    toggle_url = f"/user?name={encoded_user}&show_inactive=1" if not show_inactive else f"/user?name={encoded_user}"
    toggle_label = "Show offline servers" if not show_inactive else "Show online servers"
    summary_text = f"<strong>{active_count} online</strong> / {inactive_count} offline"
    logged_in_user = auth.get_user_from_request(request)
    is_admin = auth.is_admin(logged_in_user)
    if is_admin:
        for entry in entries:
            server_id = entry.get("id")
            entry["actions_html"] = f"""<form method="get" action="/server/edit">
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">Edit</button>
</form>
<form method="post" action="/server/delete" data-confirm="Delete this server permanently?">
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">Delete</button>
</form>"""
    cards_html = views.render_server_cards(
        entries,
        header_title=f"Worlds by {user['username']}",
        summary_text=summary_text,
        toggle_url=toggle_url,
        toggle_label=toggle_label,
    )
    header_html = views.header(
        config.get_config().get("community_name", "Server List"),
        request.path,
        logged_in=logged_in_user is not None,
        user_name=auth.display_username(logged_in_user),
    )
    body = f"""{header_html}
{cards_html}
"""
    return views.render_page("User worlds", body)
