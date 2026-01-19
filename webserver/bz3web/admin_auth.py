from bz3web import auth, config, db, webhttp


def config_ready(settings):
    required = ("session_secret",)
    for key in required:
        value = settings.get(key, "")
        if not value or value == "CHANGE_ME":
            return False
    return True


def is_authenticated(request, settings):
    cookies = webhttp.parse_cookies(request.environ)
    token = cookies.get("session", "")
    if not token:
        return False
    payload = webhttp.verify_session(token, settings.get("session_secret", ""))
    return payload == settings.get("admin_user")


def verify_login(username, password, settings):
    expected_user = settings.get("admin_user")
    if username.lower() != str(expected_user).lower():
        return False
    conn = db.connect(db.default_db_path())
    try:
        user = db.get_user_by_username(conn, expected_user)
        if not user or user["is_locked"] or user["deleted"]:
            return False
        return auth.verify_password(password, user["password_salt"], user["password_hash"])
    finally:
        conn.close()
