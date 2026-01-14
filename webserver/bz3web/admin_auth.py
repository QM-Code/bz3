import hashlib
import hmac

from bz3web import config, webhttp


def hash_password(password, salt):
    return hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt.encode("utf-8"), 100_000).hex()


def config_ready(settings):
    required = ("admin_password_hash", "admin_password_salt", "session_secret")
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
    expected_hash = settings.get("admin_password_hash", "")
    salt = settings.get("admin_password_salt", "")
    supplied_hash = hash_password(password, salt)
    return username.lower() == str(expected_user).lower() and hmac.compare_digest(supplied_hash, expected_hash)
