import urllib.parse

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


def _admin_levels(conn, settings):
    admin_username = settings.get("admin_user", "Admin")
    root_user = db.get_user_by_username(conn, admin_username)
    if not root_user:
        return {}, None
    levels = {root_user["id"]: 0}
    primary_admins = db.list_user_admins(conn, root_user["id"])
    for admin in primary_admins:
        levels[admin["admin_user_id"]] = 1
    for admin in primary_admins:
        if not admin["trust_admins"]:
            continue
        for sub_admin in db.list_user_admins(conn, admin["admin_user_id"]):
            levels.setdefault(sub_admin["admin_user_id"], 2)
    return levels, root_user["id"]


def _can_manage_user(current_user, target_user, levels, root_id):
    if root_id and current_user["id"] == root_id:
        return True
    current_level = levels.get(current_user["id"])
    target_level = levels.get(target_user["id"])
    if current_level is None:
        return False
    if target_level is None:
        return True
    return target_level >= current_level


def _render_users_list(
    users,
    current_user,
    message=None,
    form_data=None,
    show_admin_fields=False,
    root_admin_name="Admin",
    admin_levels=None,
    root_id=None,
):
    list_name = config.get_config().get("community_name", "Server List")
    form_data = form_data or {}

    if show_admin_fields:
        rows = []
        for user in users:
            admin_flag = "Yes" if user["is_admin"] else "No"
            locked = "Yes" if user["is_locked"] else "No"
            deleted = "Yes" if user["deleted"] else "No"
            is_root_target = _normalize_key(user["username"]) == _normalize_key(root_admin_name)
            can_lock = not is_root_target and (not user["is_admin"] or _is_root_admin(current_user, config.get_config()))
            can_edit = _can_manage_user(current_user, user, admin_levels or {}, root_id)
            lock_checked = "checked" if user["is_locked"] else ""
            lock_html = ""
            if can_lock:
                lock_html = f"""<form method="post" action="/users/lock" class="js-toggle-form">
      <input type="hidden" name="id" value="{user["id"]}">
      <input type="checkbox" name="locked" value="1" {lock_checked}>
    </form>"""
            edit_html = ""
            if can_edit:
                edit_html = f"""<a class="admin-link secondary" href="/users/{urllib.parse.quote(user["username"], safe='')}/edit">Edit</a>"""
            rows.append(
                f"""<tr>
  <td><a class="plain-link bold-link" href="/users/{urllib.parse.quote(user["username"], safe='')}">{webhttp.html_escape(user["username"])}</a></td>
  <td class="center-cell"><span class="status-{admin_flag.lower()}">{admin_flag}</span></td>
  <td class="center-cell">
    {lock_html or f'<span class="status-{locked.lower()}">{locked}</span>'}
  </td>
  <td class="center-cell"><span class="status-{deleted.lower()}">{deleted}</span></td>
  <td class="center-cell">
    {edit_html}
  </td>
</tr>"""
            )
        rows_html = "".join(rows) or "<tr><td colspan=\"5\">No users yet.</td></tr>"
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
        body = f"""<table class="users-table">
  <thead>
    <tr>
      <th class="center-cell">Username</th>
      <th class="center-cell">Admin</th>
      <th class="center-cell">Lock</th>
      <th class="center-cell">Deleted</th>
      <th class="center-cell">Actions</th>
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
  </div>
{admin_toggle_html}  <div class="actions">
    <button type="submit">Create User</button>
  </div>
</form>
"""
    else:
        rows = "".join(
            f"""<tr>
  <td><a class="plain-link bold-link" href="/users/{urllib.parse.quote(user["username"], safe='')}">{webhttp.html_escape(user["username"])}</a></td>
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

    profile_url = f"/users/{urllib.parse.quote(current_user['username'], safe='')}"
    header_html = views.header_with_title(
        list_name,
        "/users",
        logged_in=True,
        title="Users",
        error=message,
        user_name=auth.display_username(current_user),
        is_admin=auth.is_admin(current_user),
        profile_url=profile_url,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Users", body)


def _render_user_edit(
    user,
    message=None,
    form_data=None,
    current_user=None,
    root_admin_name="Admin",
    admin_levels=None,
):
    form_data = form_data or {}
    username_value = form_data.get("username", user["username"])
    email_value = form_data.get("email", user["email"])
    action_url = f"/users/{urllib.parse.quote(user['username'], safe='')}/edit"
    cancel_url = f"/users/{urllib.parse.quote(user['username'], safe='')}"
    body = f"""<form method="post" action="{action_url}">
  <input type="hidden" name="id" value="{user["id"]}">
  <div>
    <label for="username">Username</label>
    <input id="username" name="username" required value="{webhttp.html_escape(username_value)}">
  </div>
  <div>
    <label for="email">Email</label>
    <input id="email" name="email" type="email" required value="{webhttp.html_escape(email_value)}">
  </div>
  <div>
    <label for="password">Reset password (optional)</label>
    <input id="password" name="password" type="password" placeholder="Leave blank to keep current password">
  </div>
  <div class="actions">
    <button type="submit">Save changes</button>
    <a class="admin-link align-right" href="{cancel_url}">Cancel</a>
  </div>
</form>
"""
    if auth.is_admin(current_user):
        body += f"""<form method="post" action="/users/delete">
  <input type="hidden" name="id" value="{user["id"]}">
  <div class="actions center">
    <button type="submit" class="danger">Delete user</button>
  </div>
</form>
"""
    profile_url = f"/users/{urllib.parse.quote(current_user['username'], safe='')}"
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/users/edit",
        logged_in=True,
        title="Edit user",
        error=message,
        user_name=auth.display_username(current_user),
        is_admin=auth.is_admin(current_user),
        profile_url=profile_url,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Edit User", body)


def _render_user_settings(user, message=None, form_data=None, current_user=None):
    form_data = form_data or {}
    email_value = form_data.get("email", user["email"])
    body = f"""<form method="post" action="/users/{urllib.parse.quote(user["username"], safe='')}/edit">
  <div class="row">
    <div>
      <label for="email">Email</label>
      <input id="email" name="email" type="email" required value="{webhttp.html_escape(email_value)}">
    </div>
  </div>
  <div>
    <label for="password">New password (optional)</label>
    <input id="password" name="password" type="password" placeholder="Leave blank to keep current password">
  </div>
  <div class="actions">
    <button type="submit">Save changes</button>
    <a class="admin-link align-right" href="/users/{urllib.parse.quote(user["username"], safe='')}">Cancel</a>
  </div>
</form>
"""
    profile_url = f"/users/{urllib.parse.quote(current_user['username'], safe='')}"
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        f"/users/{urllib.parse.quote(user['username'], safe='')}/edit",
        logged_in=True,
        title="Personal settings",
        error=message,
        user_name=auth.display_username(current_user),
        is_admin=auth.is_admin(current_user),
        profile_url=profile_url,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Personal settings", body)


def _handle_admin_edit(
    conn,
    current_user,
    target_user,
    form,
    admin_username,
    admin_levels,
    root_id,
    is_root_admin,
    redirect_url="/users",
):
    is_root_target = _normalize_key(target_user["username"]) == _normalize_key(admin_username)
    if not is_root_admin and is_root_target:
        return webhttp.redirect(redirect_url)
    if not _can_manage_user(current_user, target_user, admin_levels, root_id):
        return webhttp.redirect(redirect_url)
    username = _first(form, "username")
    email = _first(form, "email").lower()
    password = _first(form, "password")
    is_admin_value = _first(form, "is_admin") == "on"
    form_data = {"username": username, "email": email, "is_admin": is_admin_value}
    if not username or not email:
        return _render_user_edit(
            target_user,
            "Username and email are required.",
            form_data=form_data,
            current_user=current_user,
            root_admin_name=admin_username,
            admin_levels=admin_levels,
        )
    if " " in username:
        return _render_user_edit(
            target_user,
            "Username cannot contain spaces.",
            form_data=form_data,
            current_user=current_user,
            root_admin_name=admin_username,
            admin_levels=admin_levels,
        )
    if _normalize_key(username) == _normalize_key(admin_username):
        return _render_user_edit(
            target_user,
            "Username is reserved.",
            form_data=form_data,
            current_user=current_user,
            root_admin_name=admin_username,
            admin_levels=admin_levels,
        )
    if is_root_target and _normalize_key(username) != _normalize_key(admin_username):
        return _render_user_edit(
            target_user,
            "Root admin username cannot be changed.",
            form_data=form_data,
            current_user=current_user,
            root_admin_name=admin_username,
            admin_levels=admin_levels,
        )
    existing = db.get_user_by_username(conn, username)
    if existing and existing["id"] != target_user["id"]:
        return _render_user_edit(
            target_user,
            "Username already taken.",
            form_data=form_data,
            current_user=current_user,
            root_admin_name=admin_username,
            admin_levels=admin_levels,
        )
    if email != target_user["email"]:
        existing_email = db.get_user_by_email(conn, email)
        if existing_email and existing_email["id"] != target_user["id"]:
            return _render_user_edit(
                target_user,
                "Email already in use.",
                form_data=form_data,
                current_user=current_user,
                root_admin_name=admin_username,
                admin_levels=admin_levels,
            )
    db.update_user_email(conn, target_user["id"], email)
    if username != target_user["username"]:
        db.update_user_username(conn, target_user["id"], username)
    if is_root_admin:
        if is_root_target:
            root_user = db.get_user_by_username(conn, admin_username)
            if root_user:
                db.recompute_admin_flags(conn, root_user["id"])
        else:
            root_user = db.get_user_by_username(conn, admin_username)
            if root_user:
                if is_admin_value:
                    db.add_user_admin(conn, root_user["id"], target_user["id"])
                else:
                    db.remove_user_admin(conn, root_user["id"], target_user["id"])
                db.recompute_admin_flags(conn, root_user["id"])
    if password:
        digest, salt = auth.new_password(password)
        db.set_user_password(conn, target_user["id"], digest, salt)
    return webhttp.redirect(redirect_url)


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
        admin_levels, root_id = _admin_levels(conn, settings)
        if path.startswith("/users/") and path.endswith("/edit"):
            username = request.query.get("name", [""])[0].strip()
            if not username:
                return webhttp.html_response("<h1>Missing user</h1>", status="400 Bad Request")
            target_user = db.get_user_by_username(conn, username)
            if request.method == "POST" and not target_user:
                form = request.form()
                user_id = _first(form, "id")
                if user_id.isdigit():
                    target_user = db.get_user_by_id(conn, int(user_id))
            if not target_user or target_user["deleted"]:
                return webhttp.html_response("<h1>User not found</h1>", status="404 Not Found")
            if current_user["id"] == target_user["id"]:
                if request.method == "GET":
                    return _render_user_settings(target_user, current_user=current_user)
                if request.method == "POST":
                    form = request.form()
                    email = _first(form, "email").lower()
                    password = _first(form, "password")
                    form_data = {"email": email}
                    if not email:
                        return _render_user_settings(
                            target_user,
                            "Email is required.",
                            form_data=form_data,
                            current_user=current_user,
                        )
                    if email != target_user["email"]:
                        existing_email = db.get_user_by_email(conn, email)
                        if existing_email and existing_email["id"] != target_user["id"]:
                            return _render_user_settings(
                                target_user,
                                "Email already in use.",
                                form_data=form_data,
                                current_user=current_user,
                            )
                    db.update_user_email(conn, target_user["id"], email)
                    if password:
                        digest, salt = auth.new_password(password)
                        db.set_user_password(conn, target_user["id"], digest, salt)
                    return webhttp.redirect(f"/users/{urllib.parse.quote(target_user['username'], safe='')}")
                return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")
            if not is_admin:
                return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
            if not _can_manage_user(current_user, target_user, admin_levels, root_id):
                return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
            if request.method == "GET":
                return _render_user_edit(
                    target_user,
                    current_user=current_user,
                    root_admin_name=admin_username,
                    admin_levels=admin_levels,
                )
            if request.method == "POST":
                return _handle_admin_edit(
                    conn,
                    current_user,
                    target_user,
                    request.form(),
                    admin_username,
                    admin_levels,
                    root_id,
                    is_root_admin,
                    redirect_url=f"/users/{urllib.parse.quote(target_user['username'], safe='')}",
                )
            return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")
        if path in ("/users", "/users/"):
            users = db.list_users(conn)
            return _render_users_list(
                users,
                current_user,
                show_admin_fields=is_admin,
                root_admin_name=admin_username,
                admin_levels=admin_levels,
                root_id=root_id,
            )

        if path in ("/users/create", "/users/create/") and request.method == "POST":
            if not is_admin:
                return webhttp.redirect("/users")
            form = request.form()
            username = _first(form, "username")
            email = _first(form, "email").lower()
            password = _first(form, "password")
            is_admin_value = _first(form, "is_admin") == "on"
            form_data = {
                "username": username,
                "email": email,
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
            db.add_user(conn, username, email, digest, salt, is_admin=False, is_admin_manual=False)
            root_user = db.get_user_by_username(conn, admin_username)
            if root_user:
                if is_root_admin and is_admin_value:
                    new_user = db.get_user_by_username(conn, username)
                    if new_user:
                        db.add_user_admin(conn, root_user["id"], new_user["id"])
                db.recompute_admin_flags(conn, root_user["id"])
            return webhttp.redirect("/users")

        if path in ("/users/lock", "/users/lock/") and request.method == "POST":
            if not is_admin:
                return webhttp.redirect("/users")
            form = request.form()
            user_id = _first(form, "id")
            if not user_id.isdigit():
                return webhttp.redirect("/users")
            user = db.get_user_by_id(conn, int(user_id))
            if not user:
                return webhttp.redirect("/users")
            if _normalize_key(user["username"]) == _normalize_key(admin_username):
                return webhttp.redirect("/users")
            if user["is_admin"] and not is_root_admin:
                return webhttp.redirect("/users")
            locked = _first(form, "locked") == "1"
            db.set_user_locked(conn, int(user_id), locked)
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
                        if not _can_manage_user(current_user, user, admin_levels, root_id):
                            return webhttp.redirect("/users")
                        return _render_user_edit(
                            user,
                            current_user=current_user,
                            root_admin_name=admin_username,
                            admin_levels=admin_levels,
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
                return _handle_admin_edit(
                    conn,
                    current_user,
                    user,
                    form,
                    admin_username,
                    admin_levels,
                    root_id,
                    is_root_admin,
                )

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
                    if not _can_manage_user(current_user, user, admin_levels, root_id):
                        return webhttp.redirect("/users")
                    db.delete_user(conn, int(user_id))
            return webhttp.redirect("/users")
    finally:
        conn.close()

    return webhttp.html_response("<h1>Not Found</h1>", status="404 Not Found")
