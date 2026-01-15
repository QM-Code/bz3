import time
import urllib.parse

from bz3web import auth, config, db, views, webhttp
from bz3web.handlers import users as users_handler
from bz3web.server_status import is_active


def _format_heartbeat(timestamp):
    if not timestamp:
        return "Never"
    try:
        return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(int(timestamp)))
    except Exception:
        return "Unknown"


def handle(request):
    if request.method != "GET":
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    name = request.query.get("name", [""])[0].strip()
    if not name:
        return webhttp.html_response("<h1>Missing server name</h1>", status="400 Bad Request")

    conn = db.connect(db.default_db_path())
    try:
        server = db.get_server_by_name(conn, name)
        if not server:
            return webhttp.html_response("<h1>Server not found</h1>", status="404 Not Found")

        settings = config.get_config()
        timeout = int(settings.get("heartbeat_timeout_seconds", 120))
        active = is_active(server, timeout)
        user = auth.get_user_from_request(request)
        is_admin = auth.is_admin(user)
        is_owner = bool(user and server["owner_user_id"] == user["id"])
        can_manage = False
        if user and not is_owner and is_admin:
            owner_user = db.get_user_by_id(conn, server["owner_user_id"])
            levels, root_id = users_handler._admin_levels(conn, settings)
            if owner_user:
                can_manage = users_handler._can_manage_user(user, owner_user, levels, root_id)

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
            "active": active,
        }
        if is_owner or can_manage:
            server_id = server["id"]
            csrf_html = views.csrf_input(auth.csrf_token(request))
            entry["actions_html"] = f"""<form method="get" action="/server/edit">
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">Edit</button>
</form>
<form method="post" action="/server/delete" data-confirm="Delete this server permanently?">
  {csrf_html}
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">Delete</button>
</form>"""

        profile_url = None
        if user:
            profile_url = f"/users/{urllib.parse.quote(user['username'], safe='')}"
        header_html = views.header(
            settings.get("community_name", "Server List"),
            request.path,
            logged_in=user is not None,
            user_name=auth.display_username(user),
            is_admin=is_admin,
            profile_url=profile_url,
        )
        header_title_html = f'<span class="server-owner">{webhttp.html_escape(server["name"])}:</span> Server'
        cards_html = views.render_server_cards(
            [entry],
            header_title_html=header_title_html,
        )

        owner_url = f"/users/{urllib.parse.quote(server['owner_username'], safe='')}"
        owner_html = f'<a href="{owner_url}">{webhttp.html_escape(server["owner_username"])}</a>'
        players_html = "—"
        if server["num_players"] is not None and server["max_players"] is not None:
            players_html = f"{server['num_players']} / {server['max_players']}"
        elif server["num_players"] is not None:
            players_html = f"{server['num_players']}"
        elif server["max_players"] is not None:
            players_html = f"— / {server['max_players']}"

        info_html = f"""<div class="info-panel">
  <div><strong>Status:</strong> {"Online" if active else "Offline"}</div>
  <div><strong>Owner:</strong> {owner_html}</div>
  <div><strong>Host:</strong> {webhttp.html_escape(server["host"])}:{server["port"]}</div>
  <div><strong>Players:</strong> {players_html}</div>
  <div><strong>Last heartbeat:</strong> {_format_heartbeat(server["last_heartbeat"])}</div>
</div>"""

        body = f"""{header_html}
{cards_html}
{info_html}
"""
        return views.render_page("Server profile", body)
    finally:
        conn.close()
