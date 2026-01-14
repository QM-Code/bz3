import os

from bz3web import uploads, webhttp
from bz3web.handlers import account, admin, api, index, servers, submit, user_profile


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
    path = request.path.rstrip("/") or "/"
    if path == "/servers":
        return servers.handle(request)
    if path == "/":
        return index.handle(request)
    if path in (
        "/register",
        "/login",
        "/logout",
        "/forgot",
        "/reset",
        "/account",
        "/account/server/edit",
        "/account/server/delete",
        "/account/admins/add",
        "/account/admins/trust",
        "/account/admins/remove",
    ):
        return account.handle(request)
    if path == "/submit":
        return submit.handle(request)
    if path == "/user":
        return user_profile.handle(request)
    if path.startswith("/static/"):
        return _serve_static(path)
    if path.startswith("/uploads/"):
        return _serve_upload(path)
    if path in ("/auth", "/heartbeat", "/admins"):
        return api.handle(request)
    if path.startswith("/admin"):
        return admin.handle(request)
    return None
