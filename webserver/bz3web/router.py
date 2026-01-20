import os
import urllib.parse

from bz3web import auth, config, uploads, views, webhttp
from bz3web.handlers import account, admin_docs, api, api_server, api_servers, api_user, info, servers, submit, user_profile, users, server_edit, server_profile


def _serve_static(path):
    base_dir = os.path.dirname(__file__)
    static_dir = os.path.join(base_dir, "static")
    rel_path = path[len("/static/") :]
    if not rel_path:
        return None
    normalized = os.path.normpath(rel_path)
    if normalized.startswith(".."):
        return None
    file_path = os.path.join(static_dir, normalized)
    if not os.path.isfile(file_path):
        return None
    with open(file_path, "rb") as handle:
        data = handle.read()
    content_type = "text/plain"
    if file_path.endswith(".css"):
        content_type = "text/css; charset=utf-8"
    if file_path.endswith(".svg"):
        content_type = "image/svg+xml"
    return webhttp.file_response(data, content_type)


def _serve_upload(path):
    upload_dir = uploads._uploads_dir()
    rel_path = path[len("/uploads/") :]
    if not rel_path:
        return None
    normalized = os.path.normpath(rel_path)
    if normalized.startswith(".."):
        return None
    file_path = os.path.join(upload_dir, normalized)
    if not os.path.isfile(file_path):
        return None
    with open(file_path, "rb") as handle:
        data = handle.read()
    content_type = "application/octet-stream"
    if file_path.endswith(".png"):
        content_type = "image/png"
    if file_path.endswith(".jpg") or file_path.endswith(".jpeg"):
        content_type = "image/jpeg"
    return webhttp.file_response(data, content_type)


def dispatch(request):
    language = None
    try:
        available = config.get_available_languages()
        user = auth.get_user_from_request(request)
        if user:
            preferred = config.normalize_language(user.get("language"))
            if preferred in available:
                language = preferred
        settings = config.get_config()
        disable_browser = bool(config.require_setting(settings, "debug.disable_browser_language_detection"))
        if not language and not disable_browser:
            accept = request.header("Accept-Language", "")
            language = config.match_language(accept, available)
        if not language:
            community = config.get_community_config()
            base = config.get_base_config()
            fallback = config.normalize_language((community or {}).get("language") or base.get("language") or "en")
            language = fallback if fallback in available else "en"
    except Exception:
        language = "en"
    config.set_request_language(language)
    path = request.path.rstrip("/") or "/"
    try:
        if path == "/api/servers":
            return api_servers.handle(request)
        if path.startswith("/api/servers/"):
            token = urllib.parse.unquote(path[len("/api/servers/") :])
            if token:
                if token.isdigit():
                    request.query.setdefault("id", [token])
                else:
                    request.query.setdefault("name", [token])
                return api_server.handle(request)
        if path.startswith("/api/users/"):
            name = urllib.parse.unquote(path[len("/api/users/") :])
            if name:
                request.query.setdefault("name", [name])
                return api_user.handle(request)
        if path == "/":
            return webhttp.redirect("/servers")
        if path == "/servers":
            return servers.handle(request)
        if path == "/info":
            return info.handle(request)
        if path.startswith("/servers/"):
            name = urllib.parse.unquote(path[len("/servers/") :])
            if name:
                request.query.setdefault("name", [name])
                return server_profile.handle(request)
        if path in ("/server/edit", "/server/delete"):
            return server_edit.handle(request)
        if path.startswith("/users/") and path.endswith("/edit"):
            rel = path[len("/users/") :]
            username = urllib.parse.unquote(rel.rsplit("/edit", 1)[0].rstrip("/"))
            if username:
                request.query.setdefault("name", [username])
                return users.handle(request)
        if path.startswith("/users/") and "/admins/" in path:
            rel = path[len("/users/") :]
            username = urllib.parse.unquote(rel.split("/admins/", 1)[0])
            if username:
                request.query.setdefault("name", [username])
                return user_profile.handle(request)
        if path in ("/users", "/users/create", "/users/edit", "/users/delete", "/users/reinstate", "/users/lock"):
            user = auth.get_user_from_request(request)
            if not user:
                return webhttp.redirect("/login")
            if not auth.is_admin(user):
                return views.error_page("403 Forbidden", "forbidden")
            return users.handle(request)
        if path.startswith("/users/"):
            username = urllib.parse.unquote(path[len("/users/") :])
            if username:
                request.query.setdefault("name", [username])
                return user_profile.handle(request)
        if path in (
            "/register",
            "/login",
            "/logout",
            "/forgot",
            "/reset",
        ):
            return account.handle(request)
        if path == "/submit":
            return submit.handle(request)
        if path == "/admin-docs":
            return admin_docs.handle(request)
        if path.startswith("/static/"):
            return _serve_static(path)
        if path.startswith("/uploads/"):
            return _serve_upload(path)
        if path in ("/api/auth", "/api/heartbeat", "/api/admins", "/api/user_registered", "/api/info", "/api/health"):
            return api.handle(request)
        return None
    finally:
        config.clear_request_language()
