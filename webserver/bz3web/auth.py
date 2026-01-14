import hashlib
import hmac
import secrets

from bz3web import config, db, webhttp


def hash_password(password, salt):
    return hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt.encode("utf-8"), 100_000).hex()


def new_password(password):
    salt = secrets.token_hex(16)
    digest = hash_password(password, salt)
    return digest, salt


def verify_password(password, salt, digest):
    supplied = hash_password(password, salt)
    return hmac.compare_digest(supplied, digest)


def sign_user_session(user_id):
    secret = config.get_config().get("session_secret", "")
    return webhttp.sign_session(str(user_id), secret, expires_in=8 * 3600)


def sign_admin_session(username):
    secret = config.get_config().get("session_secret", "")
    return webhttp.sign_session(f"admin:{username}", secret, expires_in=8 * 3600)


def get_user_from_request(request):
    cookies = webhttp.parse_cookies(request.environ)
    token = cookies.get("user_session", "")
    if not token:
        return None
    payload = webhttp.verify_session(token, config.get_config().get("session_secret", ""))
    if not payload or not payload.isdigit():
        if payload and payload.startswith("admin:"):
            username = payload.split("admin:", 1)[1] or config.get_config().get("admin_user", "Admin")
            return {
                "id": None,
                "username": username,
                "email": "",
                "auto_approve": 1,
                "is_admin": 1,
            }
        return None
    conn = db.connect(db.default_db_path())
    try:
        return db.get_user_by_id(conn, int(payload))
    finally:
        conn.close()


def ensure_admin_user(settings, conn):
    admin_user = settings.get("admin_user", "Admin")
    admin_row = db.get_user_by_username(conn, admin_user)
    if admin_row:
        return dict(admin_row)
    password_hash = settings.get("admin_password_hash", "")
    password_salt = settings.get("admin_password_salt", "")
    email = f"{admin_user.lower()}@local"
    db.add_user(conn, admin_user, email, password_hash, password_salt, is_admin=True)
    admin_row = db.get_user_by_username(conn, admin_user)
    if admin_row:
        return dict(admin_row)
    return {
        "id": None,
        "username": admin_user,
        "email": "",
        "auto_approve": 1,
        "is_admin": 1,
    }


def display_username(user):
    if not user:
        return None
    admin_user = config.get_config().get("admin_user", "Admin")
    name = None
    if isinstance(user, dict):
        name = user.get("username")
    else:
        try:
            name = user["username"]
        except Exception:
            name = None
    if not name:
        return None
    if str(name).lower() == str(admin_user).lower():
        return admin_user
    return name
