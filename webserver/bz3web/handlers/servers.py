from urllib.parse import quote

from bz3web import auth, config, db, views, webhttp


def _action(key):
    return config.ui_text(f"actions.{key}", "config.json ui_text.actions")


def _title(key):
    return config.ui_text(f"titles.{key}", "config.json ui_text.titles")


def _section(key):
    return config.ui_text(f"sections.{key}", "config.json ui_text.sections")


def handle(request):
    if request.method != "GET":
        return views.error_page("405 Method Not Allowed", "method_not_allowed")

    settings = config.get_config()
    list_name = config.require_setting(settings, "community_name")

    with db.connect_ctx() as conn:
        rows = db.list_servers(conn)

    show_inactive = request.query.get("show_inactive", [""])[0] == "1"
    overview_max = int(config.require_setting(settings, "pages.servers.overview_max_chars"))
    timeout = int(config.require_setting(settings, "heartbeat_timeout_seconds"))
    user = auth.get_user_from_request(request)
    is_admin = auth.is_admin(user)
    csrf_token = auth.csrf_token(request)
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
        overview = row["overview"] or ""
        if overview and len(overview) > overview_max:
            overview = overview[:overview_max]
        if overview:
            entry["overview"] = overview
        if row["max_players"] is not None:
            entry["max_players"] = row["max_players"]
        if row["num_players"] is not None:
            entry["num_players"] = row["num_players"]
        entry["owner"] = row["owner_username"]
        entry["screenshot_id"] = row["screenshot_id"]
        if is_admin:
            server_id = entry.get("id")
            csrf_html = views.csrf_input(csrf_token)
            confirm_delete = webhttp.html_escape(config.ui_text("confirmations.delete_server"))
            entry["actions_html"] = f"""<form method="get" action="/server/edit">
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">{webhttp.html_escape(_action("edit"))}</button>
</form>
<form method="post" action="/server/delete" data-confirm="{confirm_delete}">
  {csrf_html}
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">{webhttp.html_escape(_action("delete"))}</button>
</form>"""
        return entry

    server_page = config.require_setting(settings, "pages.servers")
    refresh_interval = int(config.require_setting(server_page, "auto_refresh", "config.json pages.servers") or 0)
    refresh_animate = bool(config.require_setting(server_page, "auto_refresh_animate", "config.json pages.servers"))
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
        header_title=_section("servers"),
        toggle_on_url="/servers?show_inactive=1",
        toggle_off_url="/servers",
        refresh_url=refresh_url,
        refresh_interval=refresh_interval,
        allow_actions=is_admin,
        refresh_animate=refresh_animate,
    )
    body = f"""{cards_html}
"""
    return views.render_page_with_header(_title("server_list"), header_html, body)
