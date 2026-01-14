from bz3web import auth, config, db, views, webhttp


def _first(form, key):
    value = form.get(key, [""])
    return value[0].strip()


def _normalize_key(value):
    return value.strip().lower()


def _is_admin(user):
    return auth.is_admin(user)


def _is_root_admin(user, settings):
    admin_username = settings.get("admin_user", "Admin")
    return _normalize_key(user["username"]) == _normalize_key(admin_username)


def _render_users_list(users, current_user, message=None, form_data=None, show_admin_fields=False, root_admin_name="Admin"):
    list_name = config.get_config().get("community_name", "Server List")
    form_data = form_data or {}

    if show_admin_fields:
        rows = []
        for user in users:
            auto = "Yes" if user["auto_approve"] else "No"
            admin_flag = "Yes" if user["is_admin"] else "No"
            rows.append(
                f"""<tr>
  <td>{webhttp.html_escape(user["username"])}</td>
  <td>{webhttp.html_escape(user["email"])}</td>
  <td>{auto}</td>
  <td>{admin_flag}</td>
  <td>
    <form method="get" action="/users/edit">
      <input type="hidden" name="id" value="{user["id"]}">
      <button type="submit" class="secondary">Edit</button>
    </form>
  </td>
</tr>"""
            )
        rows_html = "".join(rows) or "<tr><td colspan=\"5\">No users yet.</td></tr>"
        new_auto_checked = "checked" if form_data.get("auto_approve") else ""
        new_admin_checked = "checked" if form_data.get("is_admin") else ""
        show_admin_toggle = _is_root_admin(current_user, config.get_config())
        admin_toggle_html = ""
        if show_admin_toggle:
            admin_toggle_html = f"""  <div class="row">
    <div>
      <label for="new_is_admin">Admin</label>
      <input id="new_is_admin" name="is_admin" type="checkbox" {new_admin_checked}>
    </div>
  </div>
"""
        body = f"""<table>
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
    {rows_html}
  </tbody>
</table>
<h2>Add User</h2>
<form method="post" action="/users/create">
  <div class="row">
    <div>
      <label for="new_username">Username</label>
      <input id="new_username" name="username" required value="{webhttp.html_escape(form_data.get("username", ""))}">
    </div>
    <div>
      <label for="new_email">Email</label>
      <input id="new_email" name="email" type="email" required value="{webhttp.html_escape(form_data.get("email", ""))}">
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
{admin_toggle_html}  <div class="actions">
    <button type="submit">Create User</button>
  </div>
</form>
"""
    else:
        rows = "".join(
            f"""<tr>
  <td><a class="admin-link" href="/user?name={webhttp.html_escape(user["username"])}">{webhttp.html_escape(user["username"])}</a></td>
</tr>"""
            for user in users
        )
        rows_html = rows or "<tr><td>No users yet.</td></tr>"
        body = f"""<table>
  <thead>
    <tr>
      <th>Username</th>
    </tr>
  </thead>
  <tbody>
    {rows_html}
  </tbody>
</table>
"""

    header_html = views.header_with_title(
        list_name,
        "/users",
        logged_in=True,
        title="Users",
        error=message,
        user_name=auth.display_username(current_user),
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Users", body)


def _render_user_edit(user, message=None, form_data=None, current_user=None, root_admin_name="Admin"):
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
    show_admin_toggle = _is_root_admin(current_user, config.get_config())
    admin_field = ""
    if show_admin_toggle:
        disabled = "disabled" if is_root_target else ""
        admin_field = f"""  <div class="row">
    <div>
      <label for="is_admin">Admin</label>
      <input id="is_admin" name="is_admin" type="checkbox" {admin_checked} {disabled}>
    </div>
  </div>
"""
    body = f"""<form method="post" action="/users/edit">
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
{admin_field}  <div>
    <label for="password">Reset password (optional)</label>
    <input id="password" name="password" type="password" placeholder="Leave blank to keep current password">
  </div>
  <div class="actions">
    <button type="submit">Save changes</button>
    <a class="admin-link align-right" href="/users">Cancel</a>
  </div>
</form>
<form method="post" action="/users/delete">
  <input type="hidden" name="id" value="{user["id"]}">
  <button type="submit" class="secondary">Delete user</button>
</form>
"""
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/users/edit",
        logged_in=True,
        title="Edit user",
        error=message,
        user_name=auth.display_username(current_user),
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Edit User", body)


def handle(request):
    settings = config.get_config()
    current_user = auth.get_user_from_request(request)
    if not current_user:
        return webhttp.redirect("/login")
    is_admin = _is_admin(current_user)
    admin_username = settings.get("admin_user", "Admin")
    is_root_admin = _is_root_admin(current_user, settings)

    path = request.path.rstrip("/") or "/users"
    conn = db.connect(db.default_db_path())
    try:
        if path in ("/users", "/users/"):
            users = db.list_users(conn)
            return _render_users_list(
                users,
                current_user,
                show_admin_fields=is_admin,
                root_admin_name=admin_username,
            )

        if path in ("/users/create", "/users/create/") and request.method == "POST":
            if not is_admin:
                return webhttp.redirect("/users")
            form = request.form()
            username = _first(form, "username")
            email = _first(form, "email").lower()
            password = _first(form, "password")
            auto_approve = _first(form, "auto_approve") == "on"
            is_admin_value = _first(form, "is_admin") == "on"
            form_data = {
                "username": username,
                "email": email,
                "auto_approve": auto_approve,
                "is_admin": is_admin_value,
            }
            if not username or not email or not password:
                users = db.list_users(conn)
                return _render_users_list(
                    users,
                    current_user,
                    message="Username, email, and password are required.",
                    form_data=form_data,
                    show_admin_fields=True,
                    root_admin_name=admin_username,
                )
            if " " in username:
                users = db.list_users(conn)
                return _render_users_list(
                    users,
                    current_user,
                    message="Username cannot contain spaces.",
                    form_data=form_data,
                    show_admin_fields=True,
                    root_admin_name=admin_username,
                )
            if _normalize_key(username) == _normalize_key(admin_username):
                users = db.list_users(conn)
                return _render_users_list(
                    users,
                    current_user,
                    message="Username is reserved.",
                    form_data=form_data,
                    show_admin_fields=True,
                    root_admin_name=admin_username,
                )
            if db.get_user_by_username(conn, username):
                users = db.list_users(conn)
                return _render_users_list(
                    users,
                    current_user,
                    message="Username already taken.",
                    form_data=form_data,
                    show_admin_fields=True,
                    root_admin_name=admin_username,
                )
            if db.get_user_by_email(conn, email):
                users = db.list_users(conn)
                return _render_users_list(
                    users,
                    current_user,
                    message="Email already registered.",
                    form_data=form_data,
                    show_admin_fields=True,
                    root_admin_name=admin_username,
                )
            digest, salt = auth.new_password(password)
            db.add_user(conn, username, email, digest, salt, is_admin=is_admin_value if is_root_admin else False)
            if auto_approve:
                user = db.get_user_by_email(conn, email)
                if user:
                    db.set_user_auto_approve(conn, user["id"], True)
            return webhttp.redirect("/users")

        if path in ("/users/edit", "/users/edit/"):
            if not is_admin:
                return webhttp.redirect("/users")
            if request.method == "GET":
                user_id = request.query.get("id", [""])[0]
                if user_id.isdigit():
                    user = db.get_user_by_id(conn, int(user_id))
                    if user:
                        if not is_root_admin and _normalize_key(user["username"]) == _normalize_key(admin_username):
                            return webhttp.redirect("/users")
                        return _render_user_edit(
                            user,
                            current_user=current_user,
                            root_admin_name=admin_username,
                        )
                return webhttp.redirect("/users")
            if request.method == "POST":
                form = request.form()
                user_id = _first(form, "id")
                if not user_id.isdigit():
                    return webhttp.redirect("/users")
                user = db.get_user_by_id(conn, int(user_id))
                if not user:
                    return webhttp.redirect("/users")
                is_root_target = _normalize_key(user["username"]) == _normalize_key(admin_username)
                if not is_root_admin and is_root_target:
                    return webhttp.redirect("/users")
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
                        current_user=current_user,
                        root_admin_name=admin_username,
                    )
                if " " in username:
                    return _render_user_edit(
                        user,
                        "Username cannot contain spaces.",
                        form_data=form_data,
                        current_user=current_user,
                        root_admin_name=admin_username,
                    )
                if _normalize_key(username) == _normalize_key(admin_username):
                    return _render_user_edit(
                        user,
                        "Username is reserved.",
                        form_data=form_data,
                        current_user=current_user,
                        root_admin_name=admin_username,
                    )
                if is_root_target and _normalize_key(username) != _normalize_key(admin_username):
                    return _render_user_edit(
                        user,
                        "Root admin username cannot be changed.",
                        form_data=form_data,
                        current_user=current_user,
                        root_admin_name=admin_username,
                    )
                existing = db.get_user_by_username(conn, username)
                if existing and existing["id"] != user["id"]:
                    return _render_user_edit(
                        user,
                        "Username already taken.",
                        form_data=form_data,
                        current_user=current_user,
                        root_admin_name=admin_username,
                    )
                if email != user["email"]:
                    existing_email = db.get_user_by_email(conn, email)
                    if existing_email and existing_email["id"] != user["id"]:
                        return _render_user_edit(
                            user,
                            "Email already in use.",
                            form_data=form_data,
                            current_user=current_user,
                            root_admin_name=admin_username,
                        )
                db.update_user_email(conn, int(user_id), email)
                if username != user["username"]:
                    db.update_user_username(conn, int(user_id), username)
                    db.update_owner_username(conn, user["username"], username)
                db.set_user_auto_approve(conn, int(user_id), auto_approve)
                if is_root_admin:
                    if is_root_target:
                        db.set_user_admin(conn, int(user_id), True)
                    else:
                        db.set_user_admin(conn, int(user_id), is_admin_value)
                if password:
                    digest, salt = auth.new_password(password)
                    db.set_user_password(conn, int(user_id), digest, salt)
                return webhttp.redirect("/users")

        if path in ("/users/delete", "/users/delete/") and request.method == "POST":
            if not is_admin:
                return webhttp.redirect("/users")
            form = request.form()
            user_id = _first(form, "id")
            if user_id.isdigit():
                user = db.get_user_by_id(conn, int(user_id))
                if user:
                    if _normalize_key(user["username"]) == _normalize_key(admin_username):
                        return webhttp.redirect("/users")
                    if not is_root_admin and user["is_admin"]:
                        return webhttp.redirect("/users")
                    db.delete_user(conn, int(user_id))
            return webhttp.redirect("/users")
    finally:
        conn.close()

    return webhttp.html_response("<h1>Not Found</h1>", status="404 Not Found")
