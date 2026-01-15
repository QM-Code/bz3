from urllib.parse import quote

from bz3web import auth, config, db, views, webhttp
from bz3web.handlers import account, users as users_handler


def _can_manage_profile(current_user, target_user, conn, settings):
    if not current_user:
        return False
    if current_user["id"] == target_user["id"]:
        return True
    if not auth.is_admin(current_user):
        return False
    levels, root_id = users_handler._admin_levels(conn, settings)
    return users_handler._can_manage_user(current_user, target_user, levels, root_id)


def _profile_url(username):
    return f"/users/{quote(username, safe='')}"


def _render_profile(
    target_user,
    current_user,
    servers,
    admins,
    show_inactive,
    can_manage,
    message=None,
    admin_notice="",
    csrf_token="",
):
    settings = config.get_config()
    timeout = int(settings.get("heartbeat_timeout_seconds", 120))
    safe_username = webhttp.html_escape(target_user["username"])
    header_title_html = f'<span class="server-owner">{safe_username}:</span> Servers'

    def _entry_builder(server, active):
        entry = {
            "id": server["id"],
            "host": server["host"],
            "port": str(server["port"]),
            "name": server["name"],
            "description": server["description"],
            "max_players": server["max_players"],
            "num_players": server["num_players"],
            "owner": server["owner_username"],
            "screenshot_id": server["screenshot_id"],
        }
        if can_manage:
            server_id = entry.get("id")
            csrf_html = views.csrf_input(csrf_token)
            entry["actions_html"] = f"""<form method="get" action="/server/edit">
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">Edit</button>
</form>
<form method="post" action="/server/delete" data-confirm="Delete this server permanently?">
  {csrf_html}
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">Delete</button>
</form>"""
        return entry

    encoded_user = quote(target_user["username"])
    refresh_interval = int(settings.get("servers_auto_refresh", 10) or 0)
    refresh_animate = bool(settings.get("servers_auto_refresh_animate", False))
    refresh_url = None
    if refresh_interval > 0:
        refresh_url = f"/api/servers?owner={encoded_user}"
        if show_inactive:
            refresh_url = f"{refresh_url}&show_inactive=1"
    cards_html = views.render_server_section(
        servers,
        timeout,
        show_inactive,
        _entry_builder,
        header_title_html=header_title_html,
        toggle_on_url=f"/users/{encoded_user}?show_inactive=1",
        toggle_off_url=f"/users/{encoded_user}",
        refresh_url=refresh_url,
        refresh_interval=refresh_interval,
        allow_actions=can_manage,
        refresh_animate=refresh_animate,
    )

    admins_header_html = f'<span class="server-owner">{safe_username}:</span> Admins'
    admins_section = views.render_admins_section(
        admins,
        show_controls=can_manage,
        show_add_form=can_manage,
        form_prefix=f"/users/{encoded_user}",
        notice_html=admin_notice,
        header_title_html=admins_header_html,
        csrf_token=csrf_token,
    )

    submit_html = ""
    if can_manage:
        submit_html = """<div class="actions section-actions">
  <a class="admin-link" href="/submit">Add server</a>
  <a class="admin-link secondary" href="/users/{username}/edit">Personal settings</a>
</div>"""
        submit_html = submit_html.format(username=encoded_user)

    profile_url = None
    if current_user:
        profile_url = _profile_url(current_user["username"])
    header_html = views.header(
        settings.get("community_name", "Server List"),
        f"/users/{encoded_user}",
        logged_in=current_user is not None,
        user_name=auth.display_username(current_user),
        is_admin=auth.is_admin(current_user),
        profile_url=profile_url,
        error=message,
    )
    body = f"""{header_html}
{cards_html}
{submit_html}
<hr class="section-divider">
{admins_section}
"""
    return views.render_page("User profile", body)


def handle(request):
    if request.method not in ("GET", "POST"):
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    username = request.query.get("name", [""])[0].strip()
    if not username:
        return webhttp.html_response("<h1>Missing user</h1>", status="400 Bad Request")

    settings = config.get_config()
    conn = db.connect(db.default_db_path())
    try:
        target_user = db.get_user_by_username(conn, username)
        if not target_user:
            return webhttp.html_response("<h1>User not found</h1>", status="404 Not Found")

        current_user = auth.get_user_from_request(request)
        is_admin = auth.is_admin(current_user)
        if target_user["deleted"] and not is_admin:
            return webhttp.html_response("<h1>User not found</h1>", status="404 Not Found")
        if current_user:
            account._sync_root_admin_privileges(conn, settings)
        can_manage = _can_manage_profile(current_user, target_user, conn, settings)
        show_inactive = request.query.get("show_inactive", [""])[0] == "1"

        path = request.path.rstrip("/")
        if request.method == "POST":
            if not can_manage:
                return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
            form = request.form()
            if not auth.verify_csrf(request, form):
                return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
            remainder = path[len("/users/") :]
            parts = remainder.split("/", 2)
            action = parts[2] if len(parts) == 3 and parts[1] == "admins" else ""
            message = None
            if not action:
                return webhttp.html_response("<h1>Not Found</h1>", status="404 Not Found")
            if action == "add":
                username_input = form.get("username", [""])[0].strip()
                if not username_input:
                    message = "Username is required."
                elif username_input.lower() == target_user["username"].lower():
                    message = "You cannot add yourself."
                else:
                    admin_user = db.get_user_by_username(conn, username_input)
                    if not admin_user:
                        message = "User not found."
                    else:
                        db.add_user_admin(conn, target_user["id"], admin_user["id"])
                        account._recompute_root_admins(conn, target_user, settings)
                        return webhttp.redirect(_profile_url(target_user["username"]))
            elif action == "trust":
                username_input = form.get("username", [""])[0].strip()
                trust = form.get("trust_admins", [""])[0] == "1"
                admin_user = db.get_user_by_username(conn, username_input)
                if not admin_user:
                    message = "User not found."
                else:
                    db.set_user_admin_trust(conn, target_user["id"], admin_user["id"], trust)
                    account._recompute_root_admins(conn, target_user, settings)
                    return webhttp.redirect(_profile_url(target_user["username"]))
            elif action == "remove":
                username_input = form.get("username", [""])[0].strip()
                if username_input:
                    admin_user = db.get_user_by_username(conn, username_input)
                    if admin_user:
                        db.remove_user_admin(conn, target_user["id"], admin_user["id"])
                        account._recompute_root_admins(conn, target_user, settings)
                        return webhttp.redirect(_profile_url(target_user["username"]))
            servers = []
            if not target_user["deleted"]:
                servers = db.list_user_servers(conn, target_user["id"])
            admins = db.list_user_admins(conn, target_user["id"])
            notice_html = ""
            if current_user and current_user["id"] == target_user["id"]:
                notice_html = account._trusted_primary_notice(conn, current_user, settings)
            return _render_profile(
                target_user,
                current_user,
                servers,
                admins,
                show_inactive,
                can_manage,
                message=message,
                admin_notice=notice_html,
                csrf_token=auth.csrf_token(request),
            )

        servers = []
        if not target_user["deleted"]:
            servers = db.list_user_servers(conn, target_user["id"])
        admins = db.list_user_admins(conn, target_user["id"])
        notice_html = ""
        if current_user and current_user["id"] == target_user["id"]:
            notice_html = account._trusted_primary_notice(conn, current_user, settings)
        return _render_profile(
            target_user,
            current_user,
            servers,
            admins,
            show_inactive,
            can_manage,
            admin_notice=notice_html,
            csrf_token=auth.csrf_token(request),
        )
    finally:
        conn.close()
