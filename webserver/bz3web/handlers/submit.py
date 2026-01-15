import time
from urllib.parse import quote

from bz3web import auth, config, db, uploads, views, webhttp


def _first(form, key):
    value = form.get(key, [""])
    return value[0].strip()


def _parse_int(value):
    if value == "":
        return None
    try:
        return int(value)
    except ValueError:
        return None


def _render_form(message=None, user=None, form_data=None, csrf_token=""):
    form_data = form_data or {}
    host_value = webhttp.html_escape(form_data.get("host", ""))
    port_value = webhttp.html_escape(form_data.get("port", ""))
    name_value = webhttp.html_escape(form_data.get("name", ""))
    description_value = webhttp.html_escape(form_data.get("description", ""))
    csrf_html = views.csrf_input(csrf_token)
    body = f"""<form method="post" action="/submit" enctype="multipart/form-data">
  {csrf_html}
  <div class="row">
    <div>
      <label for="host">Host</label>
      <input id="host" name="host" required placeholder="example.com or 10.0.0.5" value="{host_value}">
    </div>
    <div>
      <label for="port">Port</label>
      <input id="port" name="port" required placeholder="5154" value="{port_value}">
    </div>
  </div>
  <div>
    <label for="name">Server Name</label>
    <input id="name" name="name" required placeholder="Required unique name" value="{name_value}">
  </div>
  <div>
    <label for="description">Description</label>
    <textarea id="description" name="description" placeholder="Optional description">{description_value}</textarea>
  </div>
"""
    body += """
  <div>
    <label for="screenshot">World screenshot (PNG/JPG)</label>
    <input id="screenshot" name="screenshot" type="file" accept="image/*">
  </div>
  <div class="actions">
    <button type="submit">Submit for Approval</button>
    <a class="admin-link align-right" href="/servers">Cancel</a>
  </div>
</form>
"""
    profile_url = None
    if user:
        profile_url = f"/users/{quote(user['username'], safe='')}"
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/submit",
        logged_in=bool(user),
        title="Add Server",
        error=message,
        user_name=auth.display_username(user),
        is_admin=auth.is_admin(user),
        profile_url=profile_url,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Submit Server", body)


def _render_success(user=None):
    body = """<p class="muted">Thanks! Your server has been added. It will appear online after the next heartbeat.</p>
<div class="actions">
  <a class="admin-link" href="/servers">Return to servers</a>
  <a class="admin-link" href="/submit">Submit another server</a>
</div>
"""
    profile_url = None
    if user:
        profile_url = f"/users/{quote(user['username'], safe='')}"
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/submit",
        logged_in=True,
        title="Server added",
        user_name=auth.display_username(user),
        is_admin=auth.is_admin(user),
        profile_url=profile_url,
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Server added", body)

def handle(request):
    if request.method == "GET":
        user = auth.get_user_from_request(request)
        if not user:
            return webhttp.redirect("/login")
        return _render_form(user=user, csrf_token=auth.csrf_token(request))
    if request.method != "POST":
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    settings = config.get_config()
    user = auth.get_user_from_request(request)
    if not user:
        return webhttp.redirect("/login")
    content_length = int(request.environ.get("CONTENT_LENGTH") or 0)
    max_bytes = int(settings.get("upload_max_bytes", 3 * 1024 * 1024))
    form, files = request.multipart()
    if not auth.verify_csrf(request, form):
        return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
    host = _first(form, "host")
    port_text = _first(form, "port")
    name = _first(form, "name")
    description = _first(form, "description")
    max_players = None
    num_players = None
    form_data = {
        "host": host,
        "port": port_text,
        "name": name,
        "description": description,
    }
    if content_length > max_bytes + 1024 * 1024:
        return _render_form(
            "Upload too large.",
            user=user,
            form_data=form_data,
            csrf_token=auth.csrf_token(request),
        )

    if not host or not port_text or not name:
        return _render_form(
            "Host, port, and server name are required.",
            user=user,
            form_data=form_data,
            csrf_token=auth.csrf_token(request),
        )

    try:
        port = int(port_text)
    except ValueError:
        return _render_form(
            "Port must be a number.",
            user=user,
            form_data=form_data,
            csrf_token=auth.csrf_token(request),
        )

    conn = db.connect(db.default_db_path())
    existing = db.get_server_by_name(conn, name)
    if existing:
        conn.close()
        return _render_form(
            "Server name is already taken.",
            user=user,
            form_data=form_data,
            csrf_token=auth.csrf_token(request),
        )

    screenshot_id = None
    file_item = files.get("screenshot")
    if file_item is not None and file_item.filename:
        upload_info, error = uploads.handle_upload(file_item)
        if error:
            return _render_form(
                error,
                user=user,
                form_data=form_data,
                csrf_token=auth.csrf_token(request),
            )
        screenshot_id = upload_info.get("id")

    record = {
        "name": name or None,
        "description": description or None,
        "host": host,
        "port": port,
        "max_players": max_players,
        "num_players": num_players,
        "owner_user_id": user["id"],
        "screenshot_id": screenshot_id,
        "last_heartbeat": None,
    }

    db.add_server(conn, record)
    conn.close()

    return _render_success(user=user)
