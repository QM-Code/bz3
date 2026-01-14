import time

from bz3web import auth, config, db, uploads, views, webhttp


def _first(form, key):
    value = form.get(key, [""])
    return value[0].strip()


def _form_values(form, keys):
    return {key: _first(form, key) for key in keys}


def _parse_int(value):
    if value == "":
        return None
    try:
        return int(value)
    except ValueError:
        return None


def _is_admin_user(user):
    return bool(user and user["is_admin"])


def _normalize_key(value):
    return value.strip().lower()


def _render_login(message=None):
    body = """<form method="post" action="/admin/login">
  <div class="row">
    <div>
      <label for="username">Username</label>
      <input id="username" name="username" required>
    </div>
    <div>
      <label for="password">Password</label>
      <input id="password" name="password" type="password" required>
    </div>
  </div>
  <div class="actions">
    <button type="submit">Sign In</button>
    <a class="admin-link align-right" href="/">Cancel</a>
  </div>
</form>
"""
    header_html = views.header_with_title(
        config.get_config().get("list_name", "Server List"),
        "/login",
        logged_in=False,
        title="Admin Login",
        error=message,
        show_login=False,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Admin Login", body)


def _render_edit_form(server, usernames, message=None, form_data=None, header_username="Admin"):
    form_data = form_data or {}

    def val(key):
        if key in form_data:
            return webhttp.html_escape(str(form_data.get(key) or ""))
        value = server[key]
        return webhttp.html_escape("" if value is None else str(value))

    if "owner_username" in form_data:
        owner_value = webhttp.html_escape(form_data.get("owner_username") or "")
    else:
        owner_value = webhttp.html_escape(server["owner_username"] or "")
    options = "".join(f"<option value=\"{webhttp.html_escape(name)}\"></option>" for name in usernames)
    body = f"""<form method="post" action="/admin/edit" enctype="multipart/form-data">
  <input type="hidden" name="id" value="{server['id']}">
  <div class="row">
    <div>
      <label for="host">Host</label>
      <input id="host" name="host" required value="{val('host')}">
    </div>
    <div>
      <label for="port">Port</label>
      <input id="port" name="port" required value="{val('port')}">
    </div>
  </div>
  <div>
    <label for="name">Server Name</label>
    <input id="name" name="name" value="{val('name')}">
  </div>
  <div>
    <label for="description">Description</label>
    <textarea id="description" name="description">{val('description')}</textarea>
  </div>
  <div>
    <label for="owner_username">Owner username (optional)</label>
    <input id="owner_username" name="owner_username" value="{owner_value}" list="usernames">
    <datalist id="usernames">
      {options}
    </datalist>
  </div>
  <div>
    <label for="screenshot">Replace screenshot (PNG/JPG)</label>
    <input id="screenshot" name="screenshot" type="file" accept="image/*">
  </div>
  <div class="actions">
    <button type="submit">Save Changes</button>
    <a href="/admin" class="muted">Cancel</a>
  </div>
</form>
"""
    header_html = views.header_with_title(
        config.get_config().get("list_name", "Server List"),
        "/admin/edit",
        logged_in=True,
        title="Edit server",
        error=message,
        logout_url="/admin/logout",
        user_name=header_username,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Edit Server", body)


def _render_user_edit(
    user,
    message=None,
    form_data=None,
    can_manage_admins=False,
    root_admin_name="Admin",
    header_username="Admin",
):
    form_data = form_data or {}
    username_value = form_data.get("username", user["username"])
    email_value = form_data.get("email", user["email"])
    if "auto_approve" in form_data:
        checked = "checked" if form_data.get("auto_approve") else ""
    else:
        checked = "checked" if user["auto_approve"] else ""
    if "is_admin" in form_data:
        admin_checked = "checked" if form_data.get("is_admin") else ""
    else:
        admin_checked = "checked" if user["is_admin"] else ""
    is_root_target = _normalize_key(user["username"]) == _normalize_key(root_admin_name)
    admin_field = ""
    if can_manage_admins:
        disabled = "disabled" if is_root_target else ""
        admin_field = f"""  <div class="row">
    <div>
      <label for="is_admin">Admin</label>
      <input id="is_admin" name="is_admin" type="checkbox" {admin_checked} {disabled}>
    </div>
  </div>
"""
    else:
        status = "Yes" if user["is_admin"] else "No"
        admin_field = f"""  <div class="row">
    <div>
      <label>Admin</label>
      <div class="muted">{status}</div>
    </div>
  </div>
"""
    body = f"""<form method="post" action="/admin/user/edit">
  <input type="hidden" name="id" value="{user["id"]}">
  <div class="row">
    <div>
      <label for="username">Username</label>
      <input id="username" name="username" required value="{webhttp.html_escape(username_value)}">
    </div>
    <div>
      <label for="email">Email</label>
      <input id="email" name="email" type="email" required value="{webhttp.html_escape(email_value)}">
    </div>
  </div>
  <div class="row">
    <div>
      <label for="auto_approve">Auto-Approve</label>
      <input id="auto_approve" name="auto_approve" type="checkbox" {checked}>
    </div>
  </div>
{admin_field}
  <div>
    <label for="password">Reset password (optional)</label>
    <input id="password" name="password" type="password" placeholder="Leave blank to keep current password">
  </div>
  <div class="actions">
    <button type="submit">Save changes</button>
    <a class="admin-link align-right" href="/admin">Cancel</a>
  </div>
</form>
<form method="post" action="/admin/user/delete">
  <input type="hidden" name="id" value="{user["id"]}">
  <button type="submit" class="secondary">Delete user</button>
</form>
"""
    header_html = views.header_with_title(
        config.get_config().get("list_name", "Server List"),
        "/admin/user/edit",
        logged_in=True,
        title="Edit user",
        error=message,
        logout_url="/admin/logout",
        user_name=header_username,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Edit User", body)


def _render_dashboard(
    pending,
    approved,
    users,
    header_username,
    message=None,
    server_form=None,
    user_form=None,
    can_manage_admins=False,
    root_admin_name="Admin",
):
    usernames = {user["username"] for user in users}
    server_form = server_form or {}
    user_form = user_form or {}
    new_auto_checked = "checked" if user_form.get("auto_approve") else ""

    def row_html(row, approved_state):
        name = webhttp.html_escape(row["name"] or "")
        host = webhttp.html_escape(row["host"] or "")
        port = webhttp.html_escape(str(row["port"]))
        description = webhttp.html_escape(row["description"] or "")
        owner = row["owner_username"] or ""
        if owner and owner not in usernames:
            owner = ""
        owner_display = webhttp.html_escape(owner or root_admin_name)

        action_label = "Unapprove" if approved_state else "Approve"
        action_value = "unapprove" if approved_state else "approve"
        return f"""
<tr>
  <td>{name or host}</td>
  <td>{host}:{port}</td>
  <td>{owner_display}</td>
  <td>{description}</td>
  <td>
    <form method="post" action="/admin/approve">
      <input type="hidden" name="id" value="{row['id']}">
      <input type="hidden" name="action" value="{action_value}">
      <button type="submit">{action_label}</button>
    </form>
    <form method="get" action="/admin/edit">
      <input type="hidden" name="id" value="{row['id']}">
      <button type="submit" class="secondary">Edit</button>
    </form>
    <form method="post" action="/admin/delete">
      <input type="hidden" name="id" value="{row['id']}">
      <button type="submit" class="secondary">Delete</button>
    </form>
  </td>
</tr>
"""

    pending_rows = "".join(row_html(row, False) for row in pending) or "<tr><td colspan=\"5\">No pending submissions.</td></tr>"
    approved_rows = "".join(row_html(row, True) for row in approved) or "<tr><td colspan=\"5\">No approved servers.</td></tr>"

    user_rows = []
    for user in users:
        status = "Yes" if user["auto_approve"] else "No"
        admin_status = "Yes" if user["is_admin"] else "No"
        user_rows.append(
            f"""<tr>
  <td>{webhttp.html_escape(user["username"])}</td>
  <td>{webhttp.html_escape(user["email"])}</td>
  <td>{status}</td>
  <td>{admin_status}</td>
  <td>
    <form method="get" action="/admin/user/edit">
      <input type="hidden" name="id" value="{user["id"]}">
      <button type="submit" class="secondary">Edit</button>
    </form>
  </td>
</tr>"""
        )
    user_rows_html = "".join(user_rows) or "<tr><td colspan=\"5\">No users yet.</td></tr>"

    body = f"""<h2>Server List Admin</h2>
<h2>Pending Submissions</h2>
<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Host</th>
      <th>Owner</th>
      <th>Description</th>
      <th>Actions</th>
    </tr>
  </thead>
  <tbody>
    {pending_rows}
  </tbody>
</table>
<h2>Approved Servers</h2>
<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Host</th>
      <th>Owner</th>
      <th>Description</th>
      <th>Actions</th>
    </tr>
  </thead>
  <tbody>
    {approved_rows}
  </tbody>
</table>
<h2>Add Server Manually</h2>
<form method="post" action="/admin/create">
  <div class="row">
    <div>
      <label for="host">Host</label>
      <input id="host" name="host" required value="{webhttp.html_escape(server_form.get("host", ""))}">
    </div>
    <div>
      <label for="port">Port</label>
      <input id="port" name="port" required value="{webhttp.html_escape(server_form.get("port", ""))}">
    </div>
  </div>
  <div>
    <label for="name">Server Name</label>
    <input id="name" name="name" value="{webhttp.html_escape(server_form.get("name", ""))}">
  </div>
  <div>
    <label for="description">Description</label>
    <textarea id="description" name="description">{webhttp.html_escape(server_form.get("description", ""))}</textarea>
  </div>
  <div>
    <label for="owner_username">Owner username (optional)</label>
    <input id="owner_username" name="owner_username" list="usernames" value="{webhttp.html_escape(server_form.get("owner_username", ""))}">
    <datalist id="usernames">
      {"".join(f"<option value=\"{webhttp.html_escape(user['username'])}\"></option>" for user in users)}
    </datalist>
  </div>
  <div class="actions">
    <button type="submit">Add Approved Server</button>
  </div>
</form>
<h2>Users</h2>
<table>
  <thead>
    <tr>
      <th>Username</th>
      <th>Email</th>
      <th>Auto-Approve</th>
      <th>Admin</th>
      <th>Actions</th>
    </tr>
  </thead>
  <tbody>
    {user_rows_html}
  </tbody>
</table>
<h2>Add User</h2>
<form method="post" action="/admin/user/create">
  <div class="row">
    <div>
      <label for="new_username">Username</label>
      <input id="new_username" name="username" required value="{webhttp.html_escape(user_form.get("username", ""))}">
    </div>
    <div>
      <label for="new_email">Email</label>
      <input id="new_email" name="email" type="email" required value="{webhttp.html_escape(user_form.get("email", ""))}">
    </div>
  </div>
  <div class="row">
    <div>
      <label for="new_password">Password</label>
      <input id="new_password" name="password" type="password" required>
    </div>
    <div>
      <label for="new_auto_approve">Auto-Approve</label>
      <input id="new_auto_approve" name="auto_approve" type="checkbox" {new_auto_checked}>
    </div>
  </div>
  {f'''<div class="row">
    <div>
      <label for="new_is_admin">Admin</label>
      <input id="new_is_admin" name="is_admin" type="checkbox" {'checked' if user_form.get("is_admin") else ''}>
    </div>
  </div>''' if can_manage_admins else ''}
  <div class="actions">
    <button type="submit">Create User</button>
  </div>
</form>
"""
    header_html = views.header(
        config.get_config().get("list_name", "Server List"),
        "/admin",
        logged_in=True,
        error=message,
        logout_url="/admin/logout",
        user_name=header_username,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Admin", body)


def _make_record(form):
    host = _first(form, "host")
    port_text = _first(form, "port")
    if not host or not port_text:
        return None, "Host and port are required."
    try:
        port = int(port_text)
    except ValueError:
        return None, "Port must be a number."

    record = {
        "name": _first(form, "name") or None,
        "description": _first(form, "description") or None,
        "host": host,
        "port": port,
    }
    return record, None


def _resolve_owner_username(conn, owner_input):
    owner = (owner_input or "").strip()
    if not owner:
        return None, None
    user = db.get_user_by_username(conn, owner)
    if not user:
        return None, "Owner username not found."
    return user["username"], None


def handle(request):
    settings = config.get_config()
    path = request.path.rstrip("/") or "/admin"

    if path in ("/admin/logout", "/admin/logout/"):
        return webhttp.redirect("/logout")

    current_user = auth.get_user_from_request(request)
    if not _is_admin_user(current_user):
        return webhttp.redirect("/login")
    admin_username = settings.get("admin_user", "Admin")
    is_root_admin = _normalize_key(current_user["username"]) == _normalize_key(admin_username)

    conn = db.connect(db.default_db_path())
    try:
        if path in ("/admin", "/admin/"):
            pending = db.list_servers(conn, approved=False)
            approved = db.list_servers(conn, approved=True)
            users = db.list_users(conn)
            return _render_dashboard(
                pending,
                approved,
                users,
                auth.display_username(current_user),
                can_manage_admins=is_root_admin,
                root_admin_name=admin_username,
            )

        if path in ("/admin/approve", "/admin/approve/") and request.method == "POST":
            form = request.form()
            server_id = _first(form, "id")
            action = _first(form, "action")
            if server_id.isdigit():
                db.set_approved(conn, int(server_id), action == "approve")
            return webhttp.redirect("/admin")

        if path in ("/admin/delete", "/admin/delete/") and request.method == "POST":
            form = request.form()
            server_id = _first(form, "id")
            if server_id.isdigit():
                db.delete_server(conn, int(server_id))
            return webhttp.redirect("/admin")

        if path in ("/admin/create", "/admin/create/") and request.method == "POST":
            form = request.form()
            server_form = _form_values(
                form,
                ["host", "port", "name", "description", "owner_username"],
            )
            record, error = _make_record(form)
            if error:
                pending = db.list_servers(conn, approved=False)
                approved = db.list_servers(conn, approved=True)
                users = db.list_users(conn)
                return _render_dashboard(
                    pending,
                    approved,
                    users,
                    auth.display_username(current_user),
                    message=error,
                    server_form=server_form,
                    can_manage_admins=is_root_admin,
                    root_admin_name=admin_username,
                )
            owner_username, error = _resolve_owner_username(conn, _first(form, "owner_username"))
            if error:
                pending = db.list_servers(conn, approved=False)
                approved = db.list_servers(conn, approved=True)
                users = db.list_users(conn)
                return _render_dashboard(
                    pending,
                    approved,
                    users,
                    auth.display_username(current_user),
                    message=error,
                    server_form=server_form,
                    can_manage_admins=is_root_admin,
                    root_admin_name=admin_username,
                )
            record["approved"] = 1
            record["last_heartbeat"] = int(time.time())
            record["owner_username"] = owner_username
            db.add_server(conn, record)
            return webhttp.redirect("/admin")

        if path in ("/admin/edit", "/admin/edit/"):
            if request.method == "GET":
                server_id = request.query.get("id", [""])[0]
                if server_id.isdigit():
                    server = db.get_server(conn, int(server_id))
                    if server:
                        usernames = [user["username"] for user in db.list_users(conn)]
                        return _render_edit_form(
                            server,
                            usernames,
                            header_username=auth.display_username(current_user),
                        )
                return webhttp.redirect("/admin")
            if request.method == "POST":
                content_length = int(request.environ.get("CONTENT_LENGTH") or 0)
                max_bytes = int(settings.get("upload_max_bytes", 3 * 1024 * 1024))
                form, files = request.multipart()
                server_id = _first(form, "id")
                if not server_id.isdigit():
                    return webhttp.redirect("/admin")
                form_data = _form_values(
                    form,
                    [
                        "host",
                        "port",
                        "name",
                        "description",
                        "owner_username",
                    ],
                )
                record, error = _make_record(form)
                if error:
                    server = db.get_server(conn, int(server_id))
                    if server:
                        usernames = [user["username"] for user in db.list_users(conn)]
                        return _render_edit_form(
                            server,
                            usernames,
                            message=error,
                            form_data=form_data,
                            header_username=auth.display_username(current_user),
                        )
                    return webhttp.redirect("/admin")
                owner_username, error = _resolve_owner_username(conn, _first(form, "owner_username"))
                if error:
                    server = db.get_server(conn, int(server_id))
                    usernames = [user["username"] for user in db.list_users(conn)]
                    return _render_edit_form(
                        server,
                        usernames,
                        message=error,
                        form_data=form_data,
                        header_username=auth.display_username(current_user),
                    )
                server = db.get_server(conn, int(server_id))
                if server:
                    record["screenshot_original"] = server["screenshot_original"]
                    record["screenshot_full"] = server["screenshot_full"]
                    record["screenshot_thumb"] = server["screenshot_thumb"]
                    record["owner_username"] = owner_username
                    record["num_players"] = server["num_players"]
                    record["game_mode"] = server["game_mode"]
                    record["plugins"] = server["plugins"]
                    record["max_players"] = server["max_players"]
                if content_length > max_bytes + 1024 * 1024:
                    usernames = [user["username"] for user in db.list_users(conn)]
                    return _render_edit_form(
                        server,
                        usernames,
                        "Upload too large.",
                        form_data=form_data,
                        header_username=auth.display_username(current_user),
                    )
                file_item = files.get("screenshot")
                if file_item is not None and file_item.filename:
                    content_length = int(request.environ.get("CONTENT_LENGTH") or 0)
                    max_bytes = int(settings.get("upload_max_bytes", 3 * 1024 * 1024))
                    if content_length > max_bytes + 1024 * 1024:
                        usernames = [user["username"] for user in db.list_users(conn)]
                        return _render_edit_form(
                            server,
                            usernames,
                            "Upload too large.",
                            form_data=form_data,
                            header_username=auth.display_username(current_user),
                        )
                    upload_info, error = uploads.handle_upload(file_item)
                    if error:
                        usernames = [user["username"] for user in db.list_users(conn)]
                        return _render_edit_form(
                            server,
                            usernames,
                            error,
                            form_data=form_data,
                            header_username=auth.display_username(current_user),
                        )
                    record["screenshot_original"] = upload_info["original"]
                    record["screenshot_full"] = upload_info["full"]
                    record["screenshot_thumb"] = upload_info["thumb"]
                db.update_server(conn, int(server_id), record)
                return webhttp.redirect("/admin")

        if path in ("/admin/user/edit", "/admin/user/edit/"):
            if request.method == "GET":
                user_id = request.query.get("id", [""])[0]
                if user_id.isdigit():
                    user = db.get_user_by_id(conn, int(user_id))
                    if user:
                        if not is_root_admin and _normalize_key(user["username"]) == _normalize_key(admin_username):
                            return webhttp.redirect("/admin")
                        return _render_user_edit(
                            user,
                            can_manage_admins=is_root_admin,
                            root_admin_name=admin_username,
                            header_username=auth.display_username(current_user),
                        )
                return webhttp.redirect("/admin")
            if request.method == "POST":
                form = request.form()
                user_id = _first(form, "id")
                if not user_id.isdigit():
                    return webhttp.redirect("/admin")
                user = db.get_user_by_id(conn, int(user_id))
                if not user:
                    return webhttp.redirect("/admin")
                is_root_target = _normalize_key(user["username"]) == _normalize_key(admin_username)
                if not is_root_admin and is_root_target:
                    return webhttp.redirect("/admin")
                username = _first(form, "username")
                email = _first(form, "email").lower()
                auto_approve = _first(form, "auto_approve") == "on"
                password = _first(form, "password")
                is_admin_value = _first(form, "is_admin") == "on"
                form_data = {"username": username, "email": email, "auto_approve": auto_approve, "is_admin": is_admin_value}
                if not username or not email:
                    return _render_user_edit(
                        user,
                        "Username and email are required.",
                        form_data=form_data,
                        can_manage_admins=is_root_admin,
                        root_admin_name=admin_username,
                        header_username=auth.display_username(current_user),
                    )
                if " " in username:
                    return _render_user_edit(
                        user,
                        "Username cannot contain spaces.",
                        form_data=form_data,
                        can_manage_admins=is_root_admin,
                        root_admin_name=admin_username,
                        header_username=auth.display_username(current_user),
                    )
                if _normalize_key(username) == _normalize_key(admin_username):
                    return _render_user_edit(
                        user,
                        "Username is reserved.",
                        form_data=form_data,
                        can_manage_admins=is_root_admin,
                        root_admin_name=admin_username,
                        header_username=auth.display_username(current_user),
                    )
                if is_root_target and _normalize_key(username) != _normalize_key(admin_username):
                    return _render_user_edit(
                        user,
                        "Root admin username cannot be changed.",
                        form_data=form_data,
                        can_manage_admins=is_root_admin,
                        root_admin_name=admin_username,
                        header_username=auth.display_username(current_user),
                    )
                existing = db.get_user_by_username(conn, username)
                if existing and existing["id"] != user["id"]:
                    return _render_user_edit(
                        user,
                        "Username already taken.",
                        form_data=form_data,
                        can_manage_admins=is_root_admin,
                        root_admin_name=admin_username,
                        header_username=auth.display_username(current_user),
                    )
                if email != user["email"]:
                    existing_email = db.get_user_by_email(conn, email)
                    if existing_email and existing_email["id"] != user["id"]:
                        return _render_user_edit(
                            user,
                            "Email already in use.",
                            form_data=form_data,
                            can_manage_admins=is_root_admin,
                            root_admin_name=admin_username,
                            header_username=auth.display_username(current_user),
                        )
                db.update_user_email(conn, int(user_id), email)
                if username != user["username"]:
                    db.update_user_username(conn, int(user_id), username)
                    db.update_owner_username(conn, user["username"], username)
                db.set_user_auto_approve(conn, int(user_id), auto_approve)
                if is_root_admin:
                    if _normalize_key(user["username"]) == _normalize_key(admin_username):
                        db.set_user_admin(conn, int(user_id), True)
                    else:
                        db.set_user_admin(conn, int(user_id), is_admin_value)
                if password:
                    digest, salt = auth.new_password(password)
                    db.set_user_password(conn, int(user_id), digest, salt)
                return webhttp.redirect("/admin")

        if path in ("/admin/user/delete", "/admin/user/delete/") and request.method == "POST":
            form = request.form()
            user_id = _first(form, "id")
            if user_id.isdigit():
                db.delete_user(conn, int(user_id))
            return webhttp.redirect("/admin")

        if path in ("/admin/user/create", "/admin/user/create/") and request.method == "POST":
            form = request.form()
            username = _first(form, "username")
            email = _first(form, "email").lower()
            password = _first(form, "password")
            auto_approve = _first(form, "auto_approve") == "on"
            is_admin_value = _first(form, "is_admin") == "on"
            user_form = {
                "username": username,
                "email": email,
                "auto_approve": auto_approve,
                "is_admin": is_admin_value,
            }
            if not username or not email or not password:
                pending = db.list_servers(conn, approved=False)
                approved = db.list_servers(conn, approved=True)
                users = db.list_users(conn)
                return _render_dashboard(
                    pending,
                    approved,
                    users,
                    auth.display_username(current_user),
                    message="Username, email, and password are required.",
                    user_form=user_form,
                    can_manage_admins=is_root_admin,
                    root_admin_name=admin_username,
                )
            if " " in username:
                pending = db.list_servers(conn, approved=False)
                approved = db.list_servers(conn, approved=True)
                users = db.list_users(conn)
                return _render_dashboard(
                    pending,
                    approved,
                    users,
                    auth.display_username(current_user),
                    message="Username cannot contain spaces.",
                    user_form=user_form,
                    can_manage_admins=is_root_admin,
                    root_admin_name=admin_username,
                )
            if _normalize_key(username) == _normalize_key(admin_username):
                pending = db.list_servers(conn, approved=False)
                approved = db.list_servers(conn, approved=True)
                users = db.list_users(conn)
                return _render_dashboard(
                    pending,
                    approved,
                    users,
                    auth.display_username(current_user),
                    message="Username is reserved.",
                    user_form=user_form,
                    can_manage_admins=is_root_admin,
                    root_admin_name=admin_username,
                )
            if db.get_user_by_username(conn, username):
                pending = db.list_servers(conn, approved=False)
                approved = db.list_servers(conn, approved=True)
                users = db.list_users(conn)
                return _render_dashboard(
                    pending,
                    approved,
                    users,
                    auth.display_username(current_user),
                    message="Username already taken.",
                    user_form=user_form,
                    can_manage_admins=is_root_admin,
                    root_admin_name=admin_username,
                )
            if db.get_user_by_email(conn, email):
                pending = db.list_servers(conn, approved=False)
                approved = db.list_servers(conn, approved=True)
                users = db.list_users(conn)
                return _render_dashboard(
                    pending,
                    approved,
                    users,
                    auth.display_username(current_user),
                    message="Email already registered.",
                    user_form=user_form,
                    can_manage_admins=is_root_admin,
                    root_admin_name=admin_username,
                )
            digest, salt = auth.new_password(password)
            db.add_user(conn, username, email, digest, salt, is_admin=is_admin_value if is_root_admin else False)
            if auto_approve:
                user = db.get_user_by_email(conn, email)
                if user:
                    db.set_user_auto_approve(conn, user["id"], True)
            return webhttp.redirect("/admin")

        return webhttp.html_response("<h1>Not Found</h1>", status="404 Not Found")
    finally:
        conn.close()
