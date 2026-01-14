import time

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


def _render_form(message=None, user=None, form_data=None):
    form_data = form_data or {}
    host_value = webhttp.html_escape(form_data.get("host", ""))
    port_value = webhttp.html_escape(form_data.get("port", ""))
    name_value = webhttp.html_escape(form_data.get("name", ""))
    description_value = webhttp.html_escape(form_data.get("description", ""))
    body = f"""<form method="post" action="/submit" enctype="multipart/form-data">
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
    <input id="name" name="name" placeholder="Optional friendly name" value="{name_value}">
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
    <a class="admin-link align-right" href="/">Cancel</a>
  </div>
</form>
"""
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/submit",
        logged_in=bool(user),
        title="Add Server",
        error=message,
        user_name=auth.display_username(user),
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Submit Server", body)


def _render_success(user=None):
    body = """<p class="muted">Thanks! Your server has been added. It will appear online after the next heartbeat.</p>
<div class="actions">
  <a class="admin-link" href="/">Return to home</a>
  <a class="admin-link" href="/submit">Submit another server</a>
</div>
"""
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/submit",
        logged_in=True,
        title="Server added",
        user_name=auth.display_username(user),
    )
    body = f"""{header_html}
{body}"""
    return views.render_page("Server added", body)

def handle(request):
    if request.method == "GET":
        user = auth.get_user_from_request(request)
        if not user:
            return webhttp.redirect("/login")
        return _render_form(user=user)
    if request.method != "POST":
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    settings = config.get_config()
    user = auth.get_user_from_request(request)
    if not user:
        return webhttp.redirect("/login")
    content_length = int(request.environ.get("CONTENT_LENGTH") or 0)
    max_bytes = int(settings.get("upload_max_bytes", 3 * 1024 * 1024))
    form, files = request.multipart()
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
        return _render_form("Upload too large.", user=user, form_data=form_data)

    if not host or not port_text:
        return _render_form("Host and port are required.", user=user, form_data=form_data)

    try:
        port = int(port_text)
    except ValueError:
        return _render_form("Port must be a number.", user=user, form_data=form_data)

    screenshot_id = None
    file_item = files.get("screenshot")
    if file_item is not None and file_item.filename:
        upload_info, error = uploads.handle_upload(file_item)
        if error:
            return _render_form(error, user=user, form_data=form_data)
        screenshot_id = upload_info.get("id")

    owner_username = user["username"]

    record = {
        "name": name or None,
        "description": description or None,
        "host": host,
        "port": port,
        "max_players": max_players,
        "num_players": num_players,
        "game_mode": None,
        "user_id": user["id"] if user else None,
        "owner_username": owner_username,
        "screenshot_id": screenshot_id,
        "last_heartbeat": None,
    }

    conn = db.connect(db.default_db_path())
    db.add_server(conn, record)
    conn.close()

    return _render_success(user=user)
