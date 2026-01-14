from bz3web import auth, config, db, uploads, views, webhttp


def _first(form, key):
    value = form.get(key, [""])
    return value[0].strip()


def _form_values(form, keys):
    return {key: _first(form, key) for key in keys}


def _normalize_key(value):
    return value.strip().lower()


def _owns_server(user, server):
    if server["owner_username"]:
        return _normalize_key(server["owner_username"]) == _normalize_key(user["username"])
    return server["user_id"] == user["id"]


def _render_form(server, user, message=None, form_data=None, usernames=None, is_admin=False):
    form_data = form_data or {}
    usernames = usernames or []

    def val(key):
        if key in form_data:
            return webhttp.html_escape(str(form_data.get(key) or ""))
        value = server[key]
        return webhttp.html_escape("" if value is None else str(value))

    owner_value = webhttp.html_escape(form_data.get("owner_username") or (server["owner_username"] or ""))
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
    cancel_href = "/users" if is_admin else "/account"
    body = f"""<form method=\"post\" action=\"/server/edit\" enctype=\"multipart/form-data\">
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
    <input id=\"name\" name=\"name\" value=\"{val('name')}\">
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
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/server/edit",
        logged_in=True,
        title="Edit server",
        error=message,
        user_name=auth.display_username(user),
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
            server_id = _first(form, "id")
            if not server_id.isdigit():
                return webhttp.redirect("/account")
            server = db.get_server(conn, int(server_id))
            if not server:
                return webhttp.redirect("/account")
            if not is_admin and not _owns_server(user, server):
                return webhttp.redirect("/account")
            db.delete_server(conn, int(server_id))
            return webhttp.redirect("/users" if is_admin else "/account")

        if request.method == "GET":
            server_id = request.query.get("id", [""])[0]
            if not server_id.isdigit():
                return webhttp.redirect("/users" if is_admin else "/account")
            server = db.get_server(conn, int(server_id))
            if not server:
                return webhttp.redirect("/users" if is_admin else "/account")
            if not is_admin and not _owns_server(user, server):
                return webhttp.redirect("/account")
            usernames = [row["username"] for row in db.list_users(conn)] if is_admin else []
            return _render_form(server, user, usernames=usernames, is_admin=is_admin)

        content_length = int(request.environ.get("CONTENT_LENGTH") or 0)
        max_bytes = int(settings.get("upload_max_bytes", 3 * 1024 * 1024))
        form, files = request.multipart()
        server_id = _first(form, "id")
        if not server_id.isdigit():
            return webhttp.redirect("/users" if is_admin else "/account")
        server = db.get_server(conn, int(server_id))
        if not server:
            return webhttp.redirect("/users" if is_admin else "/account")
        if not is_admin and not _owns_server(user, server):
            return webhttp.redirect("/account")
        form_data = _form_values(
            form,
            ["host", "port", "name", "description", "owner_username"],
        )
        usernames = [row["username"] for row in db.list_users(conn)] if is_admin else []
        if content_length > max_bytes + 1024 * 1024:
            return _render_form(
                server,
                user,
                message="Upload too large.",
                form_data=form_data,
                usernames=usernames,
                is_admin=is_admin,
            )
        host = _first(form, "host")
        port_text = _first(form, "port")
        if not host or not port_text:
            return _render_form(
                server,
                user,
                message="Host and port are required.",
                form_data=form_data,
                usernames=usernames,
                is_admin=is_admin,
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
            )

        owner_username = server["owner_username"] or user["username"]
        if is_admin:
            owner_input = _first(form, "owner_username")
            if owner_input:
                owner_user = db.get_user_by_username(conn, owner_input)
                if not owner_user:
                    return _render_form(
                        server,
                        user,
                        message="Owner username not found.",
                        form_data=form_data,
                        usernames=usernames,
                        is_admin=is_admin,
                    )
                owner_username = owner_user["username"]
            else:
                owner_username = None

        record = {
            "name": _first(form, "name") or None,
            "description": _first(form, "description") or None,
            "host": host,
            "port": port,
            "plugins": server["plugins"],
            "max_players": server["max_players"],
            "num_players": server["num_players"],
            "game_mode": server["game_mode"],
            "owner_username": owner_username,
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
        return webhttp.redirect("/users" if is_admin else "/account")
    finally:
        conn.close()
