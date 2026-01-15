from urllib.parse import quote

from bz3web import auth, config, db, views, webhttp


def handle(request):
    if request.method != "GET":
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    settings = config.get_config()
    list_name = settings.get("community_name", "Server List")

    conn = db.connect(db.default_db_path())
    rows = db.list_servers(conn)
    conn.close()

    show_inactive = request.query.get("show_inactive", [""])[0] == "1"
    timeout = int(settings.get("heartbeat_timeout_seconds", 120))
    user = auth.get_user_from_request(request)
    is_admin = auth.is_admin(user)
    profile_url = None
    if user:
        profile_url = f"/users/{quote(user['username'], safe='')}"
    header_html = views.header(
        list_name,
        request.path,
        user is not None,
        user_name=auth.display_username(user),
        is_admin=is_admin,
        profile_url=profile_url,
    )
    def _entry_builder(row, active):
        entry = {"id": row["id"], "host": row["host"], "port": str(row["port"])}
        if row["name"]:
            entry["name"] = row["name"]
        if row["description"]:
            entry["description"] = row["description"]
        if row["max_players"] is not None:
            entry["max_players"] = row["max_players"]
        if row["num_players"] is not None:
            entry["num_players"] = row["num_players"]
        entry["owner"] = row["owner_username"]
        entry["screenshot_id"] = row["screenshot_id"]
        if is_admin:
            server_id = entry.get("id")
            entry["actions_html"] = f"""<form method="get" action="/server/edit">
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">Edit</button>
</form>
<form method="post" action="/server/delete" data-confirm="Delete this server permanently?">
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">Delete</button>
</form>"""
        return entry

    refresh_interval = int(settings.get("servers_auto_refresh", 10) or 0)
    refresh_animate = bool(settings.get("servers_auto_refresh_animate", False))
    refresh_url = None
    if refresh_interval > 0:
        refresh_url = "/api/servers"
        if show_inactive:
            refresh_url = "/api/servers?show_inactive=1"
    cards_html = views.render_server_section(
        rows,
        timeout,
        show_inactive,
        _entry_builder,
        header_title="Servers",
        toggle_on_url="/servers?show_inactive=1",
        toggle_off_url="/servers",
        refresh_url=refresh_url,
        refresh_interval=refresh_interval,
        allow_actions=is_admin,
        refresh_animate=refresh_animate,
    )
    body = f"""{header_html}
{cards_html}
"""
    return views.render_page("Server List", body)
