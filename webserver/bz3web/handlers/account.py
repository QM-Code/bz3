import secrets
import time

from bz3web import admin_auth, auth, config, db, lightbox, views, webhttp
from bz3web.server_status import is_active


def _first(form, key):
    value = form.get(key, [""])
    return value[0].strip()


def _form_values(form, keys):
    return {key: _first(form, key) for key in keys}


def _render_register(message=None, form_data=None):
    form_data = form_data or {}
    username_value = webhttp.html_escape(form_data.get("username", ""))
    email_value = webhttp.html_escape(form_data.get("email", ""))
    body = f"""<form method="post" action="/register">
  <div class="row">
    <div>
      <label for="username">Username</label>
      <input id="username" name="username" required value="{username_value}">
    </div>
    <div>
      <label for="email">Email</label>
      <input id="email" name="email" type="email" required value="{email_value}">
    </div>
  </div>
  <div class="row">
    <div>
      <label for="password">Password</label>
      <input id="password" name="password" type="password" required>
    </div>
  </div>
  <div class="actions">
    <button type="submit">Create account</button>
    <a class="admin-link align-right" href="/login">Already have an account?</a>
  </div>
</form>
"""
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/register",
        logged_in=False,
        title="Create account",
        error=message,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Create account", body)


def _render_login(message=None, list_name=None, logged_in=False, form_data=None):
    form_data = form_data or {}
    identifier_value = webhttp.html_escape(form_data.get("identifier", ""))
    body = f"""<form method="post" action="/login">
  <div class="row">
    <div>
      <label for="email">Email or Username</label>
      <input id="email" name="email" required value="{identifier_value}" autofocus>
    </div>
    <div>
      <label for="password">Password</label>
      <input id="password" name="password" type="password" required>
    </div>
  </div>
  <div class="actions">
    <button type="submit">Sign In</button>
    <a class="admin-link align-right" href="/forgot">Forgot password?</a>
  </div>
</form>
<p class="muted">Need an account? <a class="admin-link" href="/register">Register here</a></p>
"""
    header_html = ""
    if list_name is not None:
        header_html = views.header_with_title(
            list_name,
            "/login",
            logged_in,
            title="Login",
            show_login=False,
            error=message,
        )
    body = f"""{header_html}
{body}"""
    return views.render_page("User Login", body)


def _render_forgot(message=None, reset_link=None, level="info", form_data=None):
    link_html = ""
    if reset_link:
        link_html = f'<p class="muted">Reset link: <a href="{reset_link}">{reset_link}</a></p>'
    form_data = form_data or {}
    email_value = webhttp.html_escape(form_data.get("email", ""))
    body = f"""<form method="post" action="/forgot">
  <div>
    <label for="email">Email</label>
    <input id="email" name="email" type="email" required value="{email_value}">
  </div>
  <div class="actions">
    <button type="submit">Generate reset link</button>
    <a class="admin-link align-right" href="/login">Back to login</a>
  </div>
</form>
{link_html}
<p class="muted">Reset links are displayed here until email delivery is configured.</p>
"""
    notice_kwargs = {"info": None, "warning": None, "error": None}
    if level in notice_kwargs:
        notice_kwargs[level] = message
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/forgot",
        logged_in=False,
        title="Reset password",
        **notice_kwargs,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Reset password", body)


def _render_reset(token, message=None):
    body = f"""<form method="post" action="/reset">
  <input type="hidden" name="token" value="{webhttp.html_escape(token)}">
  <div class="row">
    <div>
      <label for="password">New password</label>
      <input id="password" name="password" type="password" required>
    </div>
  </div>
  <div class="actions">
    <button type="submit">Update password</button>
    <a class="admin-link align-right" href="/login">Back to login</a>
  </div>
</form>
"""
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/reset",
        logged_in=False,
        title="Set a new password",
        error=message,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Reset password", body)


def _render_account(user, servers, admins, admin_list_public=False, show_inactive=False, message=None, form_data=None):
    entries = []
    settings = config.get_config()
    timeout = int(settings.get("heartbeat_timeout_seconds", 120))
    active_count = 0
    inactive_count = 0
    for server in servers:
        active = is_active(server, timeout)
        if active:
            active_count += 1
        else:
            inactive_count += 1
        if show_inactive:
            if active:
                continue
        else:
            if not active:
                continue
        actions_html = f"""<form method="get" action="/server/edit">
  <input type="hidden" name="id" value="{server["id"]}">
  <button type="submit" class="secondary small">Edit</button>
</form>
<form method="post" action="/server/delete" data-confirm="Delete this server permanently?">
  <input type="hidden" name="id" value="{server["id"]}">
  <button type="submit" class="secondary small">Delete</button>
</form>"""
        entries.append(
            {
                "host": server["host"],
                "port": str(server["port"]),
                "name": server["name"],
                "description": server["description"],
                "max_players": server["max_players"],
                "num_players": server["num_players"],
                "active": active,
                "owner": server["owner_username"] or user["username"],
                "screenshot_id": server["screenshot_id"],
                "actions_html": actions_html,
            }
        )
    toggle_url = "/account?show_inactive=1" if not show_inactive else "/account"
    toggle_label = "Show offline servers" if not show_inactive else "Show online servers"
    summary_text = f"<strong>{active_count} online</strong> / {inactive_count} offline"
    cards_html = views.render_server_cards(
        entries,
        header_title="Servers",
        summary_text=summary_text,
        toggle_url=toggle_url,
        toggle_label=toggle_label,
    )
    submit_html = """<div class="actions section-actions">
  <a class="admin-link" href="/submit">Add server</a>
</div>"""
    form_data = form_data or {}
    admin_input = webhttp.html_escape(form_data.get("admin_username", ""))
    admin_rows = "".join(
        f"""<tr>
  <td><a class="admin-user-link" href="/user?name={webhttp.html_escape(admin["username"])}">{webhttp.html_escape(admin["username"])}</a></td>
  <td>
    <form method="post" action="/account/admins/trust">
      <input type="hidden" name="username" value="{webhttp.html_escape(admin["username"])}">
      <label class="switch">
        <input type="checkbox" name="trust_admins" value="1" {"checked" if admin["trust_admins"] else ""} onchange="this.form.submit()">
        <span class="slider"></span>
      </label>
    </form>
  </td>
  <td>
    <form method="post" action="/account/admins/remove">
      <input type="hidden" name="username" value="{webhttp.html_escape(admin["username"])}">
      <button type="submit" class="secondary">Remove</button>
    </form>
  </td>
</tr>"""
        for admin in admins
    ) or "<tr><td colspan=\"3\">No admins assigned.</td></tr>"

    public_checked = "checked" if admin_list_public else ""
    body = f"""{cards_html}
{submit_html}
<h2>Admins</h2>
<p class="muted">Admins can manage your game worlds in the client/server binaries. Add trusted users here.</p>
<form method="post" action="/account/admins/public">
  <div class="row">
    <div>
      <label for="admins_public">Make Public</label>
      <label class="switch">
        <input id="admins_public" name="admins_public" type="checkbox" value="1" {public_checked} onchange="this.form.submit()">
        <span class="slider"></span>
      </label>
    </div>
  </div>
</form>
<table>
  <thead>
    <tr>
      <th>Username</th>
      <th>Trust user's admins</th>
      <th>Actions</th>
    </tr>
  </thead>
  <tbody>
    {admin_rows}
  </tbody>
</table>
<form method="post" action="/account/admins/add">
  <div class="row">
    <div>
      <label for="admin_username">Add admin by username</label>
      <input id="admin_username" name="username" required value="{admin_input}">
    </div>
  </div>
  <div class="actions">
    <button type="submit">Add admin</button>
  </div>
</form>
{lightbox.render_lightbox()}
{lightbox.render_lightbox_script()}
"""
    header_html = views.header(
        config.get_config().get("community_name", "Server List"),
        "/account",
        logged_in=True,
        error=message,
        user_name=auth.display_username(user),
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Servers", body)


def _normalize_key(value):
    return value.strip().lower()


def _owns_server(user, server):
    if server["owner_username"]:
        return _normalize_key(server["owner_username"]) == _normalize_key(user["username"])
    return server["user_id"] == user["id"]


def _is_root_admin(user, settings):
    admin_username = settings.get("admin_user", "Admin")
    return _normalize_key(user["username"]) == _normalize_key(admin_username)


def _sync_root_admin_privileges(conn, settings):
    admin_username = settings.get("admin_user", "Admin")
    root_user = db.get_user_by_username(conn, admin_username)
    if not root_user:
        return
    if not root_user["is_admin"]:
        db.set_user_admin(conn, root_user["id"], True)
    admin_rows = db.list_user_admins(conn, root_user["id"])
    for admin in admin_rows:
        target = db.get_user_by_username(conn, admin["username"])
        if target and not target["is_admin"]:
            db.set_user_admin(conn, target["id"], True)


def handle(request):
    path = request.path.rstrip("/")
    settings = config.get_config()
    conn = db.connect(db.default_db_path())
    try:
        if path == "/register":
            if request.method == "GET":
                return _render_register()
            if request.method == "POST":
                form = request.form()
                username = _first(form, "username")
                email = _first(form, "email").lower()
                password = _first(form, "password")
                form_data = _form_values(form, ["username", "email"])
                if not username or not email or not password:
                    return _render_register("Username, email, and password are required.", form_data=form_data)
                if " " in username:
                    return _render_register("Username cannot contain spaces.", form_data=form_data)
                admin_username = settings.get("admin_user", "Admin")
                if _normalize_key(username) == _normalize_key(admin_username):
                    return _render_register("Username is reserved.", form_data=form_data)
                if db.get_user_by_username(conn, username):
                    return _render_register(f"Username '{username}' already taken.", form_data=form_data)
                if db.get_user_by_email(conn, email):
                    return _render_register("Email already registered.", form_data=form_data)
                digest, salt = auth.new_password(password)
                db.add_user(conn, username, email, digest, salt)
                user = db.get_user_by_email(conn, email)
                headers = []
                token = auth.sign_user_session(user["id"])
                webhttp.set_cookie(headers, "user_session", token, max_age=8 * 3600)
                status, redirect_headers, body = webhttp.redirect("/account")
                return status, headers + redirect_headers, body
            return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

        if path == "/login":
            if request.method == "GET":
                return _render_login(list_name=settings.get("community_name", "Server List"))
            if request.method == "POST":
                form = request.form()
                identifier = _first(form, "email")
                password = _first(form, "password")
                form_data = {"identifier": identifier}
                if "@" in identifier:
                    user = db.get_user_by_email(conn, identifier.lower())
                else:
                    user = db.get_user_by_username(conn, identifier)
                if not user or not auth.verify_password(password, user["password_salt"], user["password_hash"]):
                    admin_user = settings.get("admin_user", "Admin")
                    if _normalize_key(identifier) == _normalize_key(admin_user) and admin_auth.verify_login(
                        identifier, password, settings
                    ):
                        user = auth.ensure_admin_user(settings, conn)
                    else:
                        return _render_login(
                            "Login failed.",
                            list_name=settings.get("community_name", "Server List"),
                            form_data=form_data,
                        )
                headers = []
                if isinstance(user, dict) and user.get("id") is None:
                    token = auth.sign_admin_session(user["username"])
                else:
                    token = auth.sign_user_session(user["id"])
                webhttp.set_cookie(headers, "user_session", token, max_age=8 * 3600)
                status, redirect_headers, body = webhttp.redirect("/account")
                return status, headers + redirect_headers, body
            return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

        if path == "/logout":
            if request.method not in ("GET", "POST"):
                return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")
            headers = []
            webhttp.set_cookie(headers, "user_session", "expired", max_age=0)
            status, redirect_headers, body = webhttp.redirect("/")
            return status, headers + redirect_headers, body

        if path == "/forgot":
            if request.method == "GET":
                return _render_forgot()
            if request.method == "POST":
                form = request.form()
                email = _first(form, "email").lower()
                form_data = _form_values(form, ["email"])
                user = db.get_user_by_email(conn, email)
                reset_link = None
                if user:
                    token = secrets.token_hex(24)
                    expires_at = int(time.time()) + 3600
                    db.add_password_reset(conn, user["id"], token, expires_at)
                    reset_link = f"/reset?token={token}"
                return _render_forgot(
                    "If the account exists, a reset link is ready.",
                    reset_link=reset_link,
                    level="info",
                    form_data=form_data,
                )
            return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

        if path == "/reset":
            if request.method == "GET":
                token = request.query.get("token", [""])[0]
                reset = db.get_password_reset(conn, token)
                if not reset or reset["expires_at"] < int(time.time()):
                    return _render_forgot("Reset link is invalid or expired.", level="error")
                return _render_reset(token)
            if request.method == "POST":
                form = request.form()
                token = _first(form, "token")
                password = _first(form, "password")
                reset = db.get_password_reset(conn, token)
                if not reset or reset["expires_at"] < int(time.time()):
                    return _render_forgot("Reset link is invalid or expired.", level="error")
                if not password:
                    return _render_reset(token, "Password is required.")
                digest, salt = auth.new_password(password)
                db.set_user_password(conn, reset["user_id"], digest, salt)
                db.delete_password_reset(conn, token)
                return _render_login("Password updated. Please sign in.", list_name=settings.get("community_name", "Server List"))
            return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

        if path == "/account":
            user = auth.get_user_from_request(request)
            if not user:
                return webhttp.redirect("/login")
            _sync_root_admin_privileges(conn, settings)
            servers = db.list_user_servers(conn, user["id"], user["username"])
            admins = db.list_user_admins(conn, user["id"])
            admin_list_public = bool(user["admin_list_public"])
            show_inactive = request.query.get("show_inactive", [""])[0] == "1"
            return _render_account(
                user,
                servers,
                admins,
                admin_list_public=admin_list_public,
                show_inactive=show_inactive,
            )

        if path == "/account/admins/add" and request.method == "POST":
            user = auth.get_user_from_request(request)
            if not user:
                return webhttp.redirect("/login")
            form = request.form()
            username = _first(form, "username")
            form_data = {"admin_username": username}
            if not username:
                servers = db.list_user_servers(conn, user["id"], user["username"])
                admins = db.list_user_admins(conn, user["id"])
                return _render_account(user, servers, admins, message="Username is required.", form_data=form_data)
            if _normalize_key(username) == _normalize_key(user["username"]):
                servers = db.list_user_servers(conn, user["id"], user["username"])
                admins = db.list_user_admins(conn, user["id"])
                return _render_account(user, servers, admins, message="You cannot add yourself.", form_data=form_data)
            admin_user = db.get_user_by_username(conn, username)
            if not admin_user:
                servers = db.list_user_servers(conn, user["id"], user["username"])
                admins = db.list_user_admins(conn, user["id"])
                return _render_account(user, servers, admins, message="User not found.", form_data=form_data)
            db.add_user_admin(conn, user["id"], admin_user["id"])
            if _is_root_admin(user, settings):
                db.set_user_admin(conn, admin_user["id"], True)
            return webhttp.redirect("/account")

        if path == "/account/admins/public" and request.method == "POST":
            user = auth.get_user_from_request(request)
            if not user:
                return webhttp.redirect("/login")
            form = request.form()
            admin_list_public = form.get("admins_public", [""])[0] == "1"
            db.set_user_admin_list_public(conn, user["id"], admin_list_public)
            return webhttp.redirect("/account")

        if path == "/account/admins/trust" and request.method == "POST":
            user = auth.get_user_from_request(request)
            if not user:
                return webhttp.redirect("/login")
            form = request.form()
            username = _first(form, "username")
            trust = form.get("trust_admins", [""])[0] == "1"
            admin_user = db.get_user_by_username(conn, username)
            if not admin_user:
                servers = db.list_user_servers(conn, user["id"], user["username"])
                admins = db.list_user_admins(conn, user["id"])
                return _render_account(user, servers, admins, message="User not found.")
            db.set_user_admin_trust(conn, user["id"], admin_user["id"], trust)
            return webhttp.redirect("/account")

        if path == "/account/admins/remove" and request.method == "POST":
            user = auth.get_user_from_request(request)
            if not user:
                return webhttp.redirect("/login")
            form = request.form()
            username = _first(form, "username")
            if username:
                admin_user = db.get_user_by_username(conn, username)
                if admin_user:
                    db.remove_user_admin(conn, user["id"], admin_user["id"])
                    if _is_root_admin(user, settings):
                        db.set_user_admin(conn, admin_user["id"], False)
            return webhttp.redirect("/account")
    finally:
        conn.close()

    return webhttp.html_response("<h1>Not Found</h1>", status="404 Not Found")
