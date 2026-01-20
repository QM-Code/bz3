from urllib.parse import quote

from bz3web import auth, config, db, uploads, views, webhttp
from bz3web.handlers import users as users_handler


def _first(form, key):
    value = form.get(key, [""])
    return value[0].strip()


def _form_values(form, keys):
    return {key: _first(form, key) for key in keys}


def _normalize_key(value):
    return value.strip().lower()


def _profile_url(username):
    return f"/users/{quote(username, safe='')}"


def _can_manage_owner(user, owner_user, conn, settings):
    if not user:
        return False
    if owner_user is None:
        return False
    if owner_user and user["id"] == owner_user["id"]:
        return True
    if not auth.is_admin(user):
        return False
    levels, root_id = users_handler._admin_levels(conn, settings)
    return users_handler._can_manage_user(user, owner_user, levels, root_id)


def _render_form(server, user, message=None, form_data=None, usernames=None, is_admin=False, csrf_token=""):
    form_data = form_data or {}
    usernames = usernames or []

    def val(key):
        if key in form_data:
            return webhttp.html_escape(str(form_data.get(key) or ""))
        value = server[key]
        return webhttp.html_escape("" if value is None else str(value))

    owner_value = webhttp.html_escape(form_data.get("owner_username") or server["owner_username"])
    owner_field = ""
    if is_admin:
        options = "".join(f"<option value=\"{webhttp.html_escape(name)}\"></option>" for name in usernames)
        owner_field = f"""
  <div>
    <label for=\"owner_username\">Owner username (optional)</label>
    <input id=\"owner_username\" name=\"owner_username\" value=\"{owner_value}\" list=\"usernames\">
    <datalist id=\"usernames\">
      {options}
    </datalist>
  </div>
"""
    cancel_href = _profile_url(server["owner_username"])
    csrf_html = views.csrf_input(csrf_token)
    body = f"""<form method=\"post\" action=\"/server/edit\" enctype=\"multipart/form-data\">
  {csrf_html}
  <input type=\"hidden\" name=\"id\" value=\"{server['id']}\">
  <div class=\"row\">
    <div>
      <label for=\"host\">Host</label>
      <input id=\"host\" name=\"host\" required value=\"{val('host')}\">
    </div>
    <div>
      <label for=\"port\">Port</label>
      <input id=\"port\" name=\"port\" required value=\"{val('port')}\">
    </div>
  </div>
  <div>
    <label for=\"name\">Server Name</label>
    <input id=\"name\" name=\"name\" required value=\"{val('name')}\">
  </div>
  <div>
    <label for=\"overview\">Overview</label>
    <textarea id=\"overview\" name=\"overview\">{val('overview')}</textarea>
  </div>
  <div>
    <label for=\"description\">Description</label>
    <textarea id=\"description\" name=\"description\">{val('description')}</textarea>
  </div>
{owner_field}  <div>
    <label for=\"screenshot\">Replace screenshot (PNG/JPG)</label>
    <input id=\"screenshot\" name=\"screenshot\" type=\"file\" accept=\"image/*\">
  </div>
  <div class=\"actions\">
    <button type=\"submit\">Save changes</button>
    <a class=\"admin-link align-right\" href=\"{cancel_href}\">Cancel</a>
  </div>
</form>
"""
    profile_url = _profile_url(user["username"])
    header_html = views.header_with_title(
        config.require_setting(config.get_config(), "community_name"),
        "/server/edit",
        logged_in=True,
        title="Edit server",
        error=message,
        user_name=auth.display_username(user),
        is_admin=auth.is_admin(user),
        profile_url=profile_url,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Edit server", body)


def handle(request):
    if request.method not in ("GET", "POST"):
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    user = auth.get_user_from_request(request)
    if not user:
        return webhttp.redirect("/login")

    settings = config.get_config()
    is_admin = auth.is_admin(user)
    conn = db.connect(db.default_db_path())
    try:
        if request.path.rstrip("/") == "/server/delete" and request.method == "POST":
            form = request.form()
            if not auth.verify_csrf(request, form):
                return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
            server_id = _first(form, "id")
            if not server_id.isdigit():
                return webhttp.redirect(_profile_url(user["username"]))
            server = db.get_server(conn, int(server_id))
            if not server:
                return webhttp.redirect(_profile_url(user["username"]))
            owner_user = db.get_user_by_id(conn, server["owner_user_id"])
            if not _can_manage_owner(user, owner_user, conn, settings):
                return webhttp.redirect(_profile_url(user["username"]))
            db.delete_server(conn, int(server_id))
            return webhttp.redirect(_profile_url(server["owner_username"]))

        if request.method == "GET":
            server_id = request.query.get("id", [""])[0]
            if not server_id.isdigit():
                return webhttp.redirect(_profile_url(user["username"]))
            server = db.get_server(conn, int(server_id))
            if not server:
                return webhttp.redirect(_profile_url(user["username"]))
            owner_user = db.get_user_by_id(conn, server["owner_user_id"])
            if not _can_manage_owner(user, owner_user, conn, settings):
                return webhttp.redirect(_profile_url(user["username"]))
            usernames = [row["username"] for row in db.list_users(conn) if not row["deleted"]] if is_admin else []
            return _render_form(
                server,
                user,
                usernames=usernames,
                is_admin=is_admin,
                csrf_token=auth.csrf_token(request),
            )

        content_length = int(request.environ.get("CONTENT_LENGTH") or 0)
        max_bytes = uploads._screenshot_settings(settings)["max_bytes"]
        form, files = request.multipart()
        if not auth.verify_csrf(request, form):
            return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
        server_id = _first(form, "id")
        if not server_id.isdigit():
            return webhttp.redirect(_profile_url(user["username"]))
        server = db.get_server(conn, int(server_id))
        if not server:
            return webhttp.redirect(_profile_url(user["username"]))
        owner_user = db.get_user_by_id(conn, server["owner_user_id"])
        if not _can_manage_owner(user, owner_user, conn, settings):
            return webhttp.redirect(_profile_url(user["username"]))
        form_data = _form_values(
            form,
            ["host", "port", "name", "overview", "description", "owner_username"],
        )
        usernames = [row["username"] for row in db.list_users(conn) if not row["deleted"]] if is_admin else []
        if content_length > max_bytes + 1024 * 1024:
            return _render_form(
                server,
                user,
                message="Upload too large.",
                form_data=form_data,
                usernames=usernames,
                is_admin=is_admin,
                csrf_token=auth.csrf_token(request),
            )
        host = _first(form, "host")
        port_text = _first(form, "port")
        name = _first(form, "name")
        if not host or not port_text or not name:
            return _render_form(
                server,
                user,
                message="Host, port, and server name are required.",
                form_data=form_data,
                usernames=usernames,
                is_admin=is_admin,
                csrf_token=auth.csrf_token(request),
            )

        overview = _first(form, "overview")
        overview_max = int(config.require_setting(settings, "pages.servers.overview_max_chars"))
        if overview and len(overview) > overview_max:
            return _render_form(
                server,
                user,
                message=f"Overview must be at most {overview_max} characters.",
                form_data=form_data,
                usernames=usernames,
                is_admin=is_admin,
                csrf_token=auth.csrf_token(request),
            )
        try:
            port = int(port_text)
        except ValueError:
            return _render_form(
                server,
                user,
                message="Port must be a number.",
                form_data=form_data,
                usernames=usernames,
                is_admin=is_admin,
                csrf_token=auth.csrf_token(request),
            )

        owner_user_id = server["owner_user_id"]
        if is_admin:
            owner_input = _first(form, "owner_username")
            if owner_input:
                owner_user = db.get_user_by_username(conn, owner_input)
                if not owner_user or owner_user["deleted"]:
                    return _render_form(
                        server,
                        user,
                        message="Owner username not found.",
                        form_data=form_data,
                        usernames=usernames,
                        is_admin=is_admin,
                        csrf_token=auth.csrf_token(request),
                    )
                owner_user_id = owner_user["id"]

        existing = db.get_server_by_name(conn, name)
        if existing and existing["id"] != server["id"]:
            return _render_form(
                server,
                user,
                message="Server name is already taken.",
                form_data=form_data,
                usernames=usernames,
                is_admin=is_admin,
                csrf_token=auth.csrf_token(request),
            )
        record = {
            "name": name,
            "overview": overview or None,
            "description": _first(form, "description") or None,
            "host": host,
            "port": port,
            "max_players": server["max_players"],
            "num_players": server["num_players"],
            "owner_user_id": owner_user_id,
            "screenshot_id": server["screenshot_id"],
        }

        file_item = files.get("screenshot")
        if file_item is not None and file_item.filename:
            upload_info, error = uploads.handle_upload(file_item)
            if error:
                return _render_form(
                    server,
                    user,
                    message=error,
                    form_data=form_data,
                    usernames=usernames,
                    is_admin=is_admin,
                )
            record["screenshot_id"] = upload_info["id"]

        db.update_server(conn, int(server_id), record)
        return webhttp.redirect(_profile_url(server["owner_username"]))
    finally:
        conn.close()
