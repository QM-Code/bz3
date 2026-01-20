import secrets
import time
from urllib.parse import quote

from bz3web import auth, config, db, ratelimit, views, webhttp


def _first(form, key):
    value = form.get(key, [""])
    return value[0].strip()


def _form_values(form, keys):
    return {key: _first(form, key) for key in keys}


def _render_register(message=None, form_data=None, csrf_token=""):
    form_data = form_data or {}
    username_value = webhttp.html_escape(form_data.get("username", ""))
    email_value = webhttp.html_escape(form_data.get("email", ""))
    csrf_html = views.csrf_input(csrf_token)
    body = f"""<form method="post" action="/register">
  {csrf_html}
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
        config.require_setting(config.get_config(), "community_name"),
        "/register",
        logged_in=False,
        title="Create account",
        error=message,
        is_admin=False,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Create account", body)


def _render_login(message=None, list_name=None, logged_in=False, form_data=None, csrf_token=""):
    form_data = form_data or {}
    identifier_value = webhttp.html_escape(form_data.get("identifier", ""))
    csrf_html = views.csrf_input(csrf_token)
    body = f"""<form method="post" action="/login">
  {csrf_html}
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
            is_admin=False,
        )
    body = f"""{header_html}
{body}"""
    return views.render_page("User Login", body)


def _render_forgot(message=None, reset_link=None, level="info", form_data=None, csrf_token=""):
    link_html = ""
    if reset_link:
        link_html = f'<p class="muted">Reset link: <a href="{reset_link}">{reset_link}</a></p>'
    form_data = form_data or {}
    email_value = webhttp.html_escape(form_data.get("email", ""))
    csrf_html = views.csrf_input(csrf_token)
    body = f"""<form method="post" action="/forgot">
  {csrf_html}
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
            config.require_setting(config.get_config(), "community_name"),
            "/forgot",
            logged_in=False,
            title="Reset password",
        **notice_kwargs,
        is_admin=False,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Reset password", body)


def _render_reset(token, message=None, csrf_token=""):
    csrf_html = views.csrf_input(csrf_token)
    body = f"""<form method="post" action="/reset">
  {csrf_html}
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
        config.require_setting(config.get_config(), "community_name"),
        "/reset",
        logged_in=False,
        title="Set a new password",
        error=message,
        is_admin=False,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Reset password", body)


def _normalize_key(value):
    return value.strip().lower()


def _owns_server(user, server):
    return server["owner_user_id"] == user["id"]


def _is_root_admin(user, settings):
    admin_username = config.require_setting(settings, "admin_user")
    return _normalize_key(user["username"]) == _normalize_key(admin_username)


def _sync_root_admin_privileges(conn, settings):
    admin_username = config.require_setting(settings, "admin_user")
    root_user = db.get_user_by_username(conn, admin_username)
    if not root_user:
        return
    db.recompute_admin_flags(conn, root_user["id"])


def _recompute_root_admins(conn, user, settings):
    if not _is_root_admin(user, settings):
        admin_username = config.require_setting(settings, "admin_user")
        root_user = db.get_user_by_username(conn, admin_username)
        if not root_user:
            return
        admins = db.list_user_admins(conn, root_user["id"])
        trusted_admin_ids = {
            admin["admin_user_id"] for admin in admins if admin["trust_admins"]
        }
        if user["id"] not in trusted_admin_ids:
            return
    else:
        admin_username = config.require_setting(settings, "admin_user")
        root_user = db.get_user_by_username(conn, admin_username)
        if not root_user:
            return
    if root_user:
        db.recompute_admin_flags(conn, root_user["id"])


def _trusted_primary_notice(conn, target_user, settings):
    admin_username = config.require_setting(settings, "admin_user")
    root_user = db.get_user_by_username(conn, admin_username)
    if not root_user:
        return ""
    admins = db.list_user_admins(conn, root_user["id"])
    trusted_ids = {
        admin["admin_user_id"] for admin in admins if admin["trust_admins"]
    }
    if target_user["id"] == root_user["id"]:
        return (
            '<div class="admin-notice">'
            '<div>You are logged in as the <span class="admin-notice-strong">Root Admin</span>.</div>'
            '<ul>'
            '<li>Any admins you create will also be granted Community Admin status.</li>'
            '<li class="admin-notice-warning">If you enable "Trust User\'s Admins", all of that user\'s admins will also '
            'become Community Admins.</li>'
            '<li>Please read the <a href="/admin-docs">Admin Documentation</a> and proceed with caution.</li>'
            '</ul>'
            '</div>'
        )
    if target_user["id"] not in trusted_ids:
        return ""
    return (
        '<div class="admin-notice">'
        '<div>You are a <span class="admin-notice-strong">Trusted Primary Community Admin</span>.</div>'
        '<ul>'
        '<li>Any admins you create will also be granted Community Admin status.</li>'
        '<li>Please read the <a href="/admin-docs">Admin Documentation</a> and proceed with caution.</li>'
        '</ul>'
        '</div>'
    )


def handle(request):
    path = request.path.rstrip("/")
    settings = config.get_config()
    conn = db.connect(db.default_db_path())
    try:
        if path == "/register":
            if request.method == "GET":
                headers = []
                csrf_token = auth.ensure_csrf_cookie(request, headers)
                status, response_headers, body = _render_register(csrf_token=csrf_token)
                return status, response_headers + headers, body
            if request.method == "POST":
                form = request.form()
                if not auth.verify_csrf(request, form):
                    return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
                if not ratelimit.check(settings, request, "register"):
                    return _render_register(
                        "Too many attempts. Please wait and try again.",
                        csrf_token=auth.csrf_token(request),
                    )
                username = _first(form, "username")
                email = _first(form, "email").lower()
                password = _first(form, "password")
                form_data = _form_values(form, ["username", "email"])
                if not username or not email or not password:
                    return _render_register(
                        "Username, email, and password are required.",
                        form_data=form_data,
                        csrf_token=auth.csrf_token(request),
                    )
                if " " in username:
                    return _render_register(
                        "Username cannot contain spaces.",
                        form_data=form_data,
                        csrf_token=auth.csrf_token(request),
                    )
                admin_username = config.require_setting(settings, "admin_user")
                if _normalize_key(username) == _normalize_key(admin_username):
                    return _render_register(
                        "Username is reserved.",
                        form_data=form_data,
                        csrf_token=auth.csrf_token(request),
                    )
                if db.get_user_by_username(conn, username):
                    return _render_register(
                        f"Username '{username}' already taken.",
                        form_data=form_data,
                        csrf_token=auth.csrf_token(request),
                    )
                if db.get_user_by_email(conn, email):
                    return _render_register(
                        "Email already registered.",
                        form_data=form_data,
                        csrf_token=auth.csrf_token(request),
                    )
                digest, salt = auth.new_password(password)
                db.add_user(conn, username, email, digest, salt)
                user = db.get_user_by_email(conn, email)
                headers = []
                token = auth.sign_user_session(user["id"])
                webhttp.set_cookie(headers, "user_session", token, max_age=8 * 3600)
                profile_url = "/servers"
                if user and "username" in user:
                    profile_url = f"/users/{quote(user['username'], safe='')}"
                status, redirect_headers, body = webhttp.redirect(profile_url)
                return status, headers + redirect_headers, body
            return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

        if path == "/login":
            if request.method == "GET":
                headers = []
                csrf_token = auth.ensure_csrf_cookie(request, headers)
                list_name = config.require_setting(settings, "community_name")
                status, response_headers, body = _render_login(list_name=list_name, csrf_token=csrf_token)
                return status, response_headers + headers, body
            if request.method == "POST":
                form = request.form()
                if not auth.verify_csrf(request, form):
                    return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
                if not ratelimit.check(settings, request, "login"):
                    return _render_login(
                        "Too many attempts. Please wait and try again.",
                        list_name=config.require_setting(settings, "community_name"),
                        csrf_token=auth.csrf_token(request),
                    )
                identifier = _first(form, "email")
                password = _first(form, "password")
                form_data = {"identifier": identifier}
                if "@" in identifier:
                    user = db.get_user_by_email(conn, identifier.lower())
                else:
                    user = db.get_user_by_username(conn, identifier)
                if not user or not auth.verify_password(password, user["password_salt"], user["password_hash"]):
                    return _render_login(
                        "Login failed.",
                        list_name=config.require_setting(settings, "community_name"),
                        form_data=form_data,
                        csrf_token=auth.csrf_token(request),
                    )
                if user and (user["is_locked"] or user["deleted"]):
                    return _render_login(
                        "Account locked.",
                        list_name=config.require_setting(settings, "community_name"),
                        form_data=form_data,
                        csrf_token=auth.csrf_token(request),
                    )
                headers = []
                if isinstance(user, dict) and user.get("id") is None:
                    token = auth.sign_admin_session(user["username"])
                else:
                    token = auth.sign_user_session(user["id"])
                webhttp.set_cookie(headers, "user_session", token, max_age=8 * 3600)
                profile_url = "/servers"
                if user and "username" in user:
                    profile_url = f"/users/{quote(user['username'], safe='')}"
                status, redirect_headers, body = webhttp.redirect(profile_url)
                return status, headers + redirect_headers, body
            return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

        if path == "/logout":
            if request.method not in ("GET", "POST"):
                return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")
            headers = []
            webhttp.set_cookie(headers, "user_session", "expired", max_age=0)
            status, redirect_headers, body = webhttp.redirect("/servers")
            return status, headers + redirect_headers, body

        if path == "/forgot":
            if request.method == "GET":
                headers = []
                csrf_token = auth.ensure_csrf_cookie(request, headers)
                status, response_headers, body = _render_forgot(csrf_token=csrf_token)
                return status, response_headers + headers, body
            if request.method == "POST":
                form = request.form()
                if not auth.verify_csrf(request, form):
                    return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
                if not ratelimit.check(settings, request, "forgot"):
                    return _render_forgot(
                        "Too many attempts. Please wait and try again.",
                        level="warning",
                        csrf_token=auth.csrf_token(request),
                    )
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
                    csrf_token=auth.csrf_token(request),
                )
            return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

        if path == "/reset":
            if request.method == "GET":
                token = request.query.get("token", [""])[0]
                reset = db.get_password_reset(conn, token)
                if not reset or reset["expires_at"] < int(time.time()):
                    headers = []
                    csrf_token = auth.ensure_csrf_cookie(request, headers)
                    status, response_headers, body = _render_forgot(
                        "Reset link is invalid or expired.",
                        level="error",
                        csrf_token=csrf_token,
                    )
                    return status, response_headers + headers, body
                headers = []
                csrf_token = auth.ensure_csrf_cookie(request, headers)
                status, response_headers, body = _render_reset(token, csrf_token=csrf_token)
                return status, response_headers + headers, body
            if request.method == "POST":
                form = request.form()
                if not auth.verify_csrf(request, form):
                    return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
                if not ratelimit.check(settings, request, "reset"):
                    return _render_forgot(
                        "Too many attempts. Please wait and try again.",
                        level="warning",
                        csrf_token=auth.csrf_token(request),
                    )
                token = _first(form, "token")
                password = _first(form, "password")
                reset = db.get_password_reset(conn, token)
                if not reset or reset["expires_at"] < int(time.time()):
                    return _render_forgot(
                        "Reset link is invalid or expired.",
                        level="error",
                        csrf_token=auth.csrf_token(request),
                    )
                if not password:
                    return _render_reset(token, "Password is required.", csrf_token=auth.csrf_token(request))
                digest, salt = auth.new_password(password)
                db.set_user_password(conn, reset["user_id"], digest, salt)
                db.delete_password_reset(conn, token)
                return _render_login(
                    "Password updated. Please sign in.",
                    list_name=config.require_setting(settings, "community_name"),
                )
            return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    finally:
        conn.close()

    return webhttp.html_response("<h1>Not Found</h1>", status="404 Not Found")
