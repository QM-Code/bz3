import base64
import cgi
import hashlib
import hmac
import html
import json
import time
import urllib.parse


class Request:
    def __init__(self, environ):
        self.environ = environ
        self.method = environ.get("REQUEST_METHOD", "GET").upper()
        self.path = environ.get("PATH_INFO", "/")
        self.query_string = environ.get("QUERY_STRING", "")
        self.query = urllib.parse.parse_qs(self.query_string, keep_blank_values=True)
        self._body = None
        self._form = None
        self._multipart = None

    def header(self, name, default=""):
        key = "HTTP_" + name.upper().replace("-", "_")
        return self.environ.get(key, default)

    def body(self):
        if self._body is None:
            try:
                length = int(self.environ.get("CONTENT_LENGTH") or 0)
            except ValueError:
                length = 0
            self._body = self.environ["wsgi.input"].read(length) if length > 0 else b""
        return self._body

    def form(self):
        if self._form is None:
            content_type = self.environ.get("CONTENT_TYPE", "")
            if "application/x-www-form-urlencoded" in content_type:
                body = self.body().decode("utf-8", errors="replace")
                self._form = urllib.parse.parse_qs(body, keep_blank_values=True)
            elif "multipart/form-data" in content_type:
                form = {}
                files = {}
                storage = cgi.FieldStorage(fp=self.environ["wsgi.input"], environ=self.environ, keep_blank_values=True)
                if storage.list:
                    for item in storage.list:
                        if item.filename:
                            files[item.name] = item
                        else:
                            form.setdefault(item.name, []).append(item.value)
                self._form = form
                self._multipart = (form, files)
            else:
                self._form = {}
        return self._form

    def multipart(self):
        if self._multipart is None:
            form = {}
            files = {}
            content_type = self.environ.get("CONTENT_TYPE", "")
            if "multipart/form-data" in content_type:
                storage = cgi.FieldStorage(fp=self.environ["wsgi.input"], environ=self.environ, keep_blank_values=True)
                if storage.list:
                    for item in storage.list:
                        if item.filename:
                            files[item.name] = item
                        else:
                            form.setdefault(item.name, []).append(item.value)
            self._multipart = (form, files)
        return self._multipart


def html_escape(value):
    return html.escape(value or "", quote=True)


def html_response(body, status="200 OK", headers=None):
    headers = headers or []
    headers = [("Content-Type", "text/html; charset=utf-8")] + headers
    return status, headers, body.encode("utf-8")


def json_response(payload, status="200 OK", headers=None):
    headers = headers or []
    headers = [("Content-Type", "application/json; charset=utf-8")] + headers
    body = json.dumps(payload, indent=2, sort_keys=False)
    return status, headers, body.encode("utf-8")


def redirect(location):
    headers = [("Location", location)]
    return "302 Found", headers, b""


def file_response(body, content_type, status="200 OK", headers=None):
    headers = headers or []
    headers = [("Content-Type", content_type)] + headers
    return status, headers, body


def parse_cookies(environ):
    raw = environ.get("HTTP_COOKIE", "")
    cookies = {}
    for part in raw.split(";"):
        if "=" not in part:
            continue
        name, value = part.split("=", 1)
        cookies[name.strip()] = value.strip()
    return cookies


def set_cookie(headers, name, value, path="/", max_age=None, http_only=True, same_site="Lax"):
    parts = [f"{name}={value}", f"Path={path}", f"SameSite={same_site}"]
    if max_age is not None:
        parts.append(f"Max-Age={max_age}")
    if http_only:
        parts.append("HttpOnly")
    headers.append(("Set-Cookie", "; ".join(parts)))


def sign_session(payload, secret, expires_in=3600):
    expiry = int(time.time()) + expires_in
    message = f"{payload}|{expiry}"
    signature = hmac.new(secret.encode("utf-8"), message.encode("utf-8"), hashlib.sha256).hexdigest()
    token = f"{message}|{signature}"
    return base64.urlsafe_b64encode(token.encode("utf-8")).decode("utf-8")


def verify_session(token, secret):
    try:
        decoded = base64.urlsafe_b64decode(token.encode("utf-8")).decode("utf-8")
        payload, expiry, signature = decoded.rsplit("|", 2)
        message = f"{payload}|{expiry}"
        expected = hmac.new(secret.encode("utf-8"), message.encode("utf-8"), hashlib.sha256).hexdigest()
        if not hmac.compare_digest(signature, expected):
            return None
        if int(expiry) < int(time.time()):
            return None
        return payload
    except Exception:
        return None
