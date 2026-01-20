from bz3web import auth, config, db, webhttp


def config_ready(settings):
    try:
        value = config.require_setting(settings, "session_secret")
    except ValueError:
        return False
    return value != "CHANGE_ME"


def is_authenticated(request, settings):
    cookies = webhttp.parse_cookies(request.environ)
    token = cookies.get("session", "")
    if not token:
        return False
    payload = webhttp.verify_session(token, config.require_setting(settings, "session_secret"))
    return payload == config.require_setting(settings, "admin_user")


def verify_login(username, password, settings):
    expected_user = config.require_setting(settings, "admin_user")
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
