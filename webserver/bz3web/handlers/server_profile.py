import time
import urllib.parse

from bz3web import auth, config, db, markdown_utils, views, webhttp
from bz3web.handlers import users as users_handler
from bz3web.server_status import is_active


def _format_heartbeat(timestamp):
    if not timestamp:
        return config.ui_text("messages.server_profile.heartbeat_never")
    try:
        return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(int(timestamp)))
    except Exception:
        return config.ui_text("messages.server_profile.heartbeat_unknown")


def _label(key):
    return config.ui_text(f"labels.{key}", "config.json ui_text.labels")


def _action(key):
    return config.ui_text(f"actions.{key}", "config.json ui_text.actions")


def _title(key):
    return config.ui_text(f"titles.{key}", "config.json ui_text.titles")


def handle(request):
    if request.method != "GET":
        return views.error_page("405 Method Not Allowed", "method_not_allowed")

    name = request.query.get("name", [""])[0].strip()
    if not name:
        return views.error_page("400 Bad Request", "missing_server")

    with db.connect_ctx() as conn:
        server = db.get_server_by_name(conn, name)
        if not server:
            return views.error_page("404 Not Found", "server_not_found")

        settings = config.get_config()
        timeout = int(config.require_setting(settings, "heartbeat.timeout_seconds"))
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
            "overview": server["overview"] or "",
            "max_players": server["max_players"],
            "num_players": server["num_players"],
            "owner": server["owner_username"],
            "screenshot_id": server["screenshot_id"],
            "active": active,
        }
        if is_owner or can_manage:
            server_id = server["id"]
            csrf_html = views.csrf_input(auth.csrf_token(request))
            edit_label = webhttp.html_escape(_action("edit"))
            delete_label = webhttp.html_escape(_action("delete"))
            confirm_delete = webhttp.html_escape(config.ui_text("confirmations.delete_server"))
            entry["actions_html"] = f"""<form method="get" action="/server/edit">
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">{edit_label}</button>
</form>
<form method="post" action="/server/delete" data-confirm="{confirm_delete}">
  {csrf_html}
  <input type="hidden" name="id" value="{server_id}">
  <button type="submit" class="secondary small">{delete_label}</button>
</form>"""

        profile_url = None
        if user:
            profile_url = f"/users/{urllib.parse.quote(user['username'], safe='')}"
        header_html = views.header(
            config.require_setting(settings, "server.community_name"),
            request.path,
            logged_in=user is not None,
            user_name=auth.display_username(user),
            is_admin=is_admin,
            profile_url=profile_url,
        )
        header_title_html = f'<span class="server-owner">{webhttp.html_escape(server["name"])}:</span> {webhttp.html_escape(_title("server_profile"))}'
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

        description_html = markdown_utils.render_markdown(server["description"])
        if not description_html:
            empty_desc = config.require_setting(settings, "ui_text.empty_states.description")
            description_html = f'<p class="muted">{webhttp.html_escape(empty_desc)}</p>'
        status_label = webhttp.html_escape(_label("status"))
        owner_label = webhttp.html_escape(_label("owner"))
        host_label = webhttp.html_escape(_label("host"))
        players_label = webhttp.html_escape(_label("players"))
        heartbeat_label = webhttp.html_escape(_label("last_heartbeat"))
        online_text = config.ui_text("status.online")
        offline_text = config.ui_text("status.offline")
        info_html = f"""<div class="info-panel">
  <div><strong>{status_label}:</strong> {webhttp.html_escape(online_text if active else offline_text)}</div>
  <div><strong>{owner_label}:</strong> {owner_html}</div>
  <div><strong>{host_label}:</strong> {webhttp.html_escape(server["host"])}:{server["port"]}</div>
  <div><strong>{players_label}:</strong> {players_html}</div>
  <div><strong>{heartbeat_label}:</strong> {_format_heartbeat(server["last_heartbeat"])}</div>
</div>"""

        description_section = f"""<div class="info-panel">
  <div><strong>{webhttp.html_escape(_label("description"))}</strong></div>
  {description_html}
</div>"""

        body = f"""{cards_html}
{info_html}
{description_section}
"""
        return views.render_page_with_header(_title("server_profile"), header_html, body)
