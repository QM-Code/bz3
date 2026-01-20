from urllib.parse import quote

from bz3web import auth, config, markdown_utils, views, webhttp


def _render_info_page(request, message=None, error=None, edit_mode=False):
    settings = config.get_config()
    community_name = config.require_setting(settings, "community_name")
    community_description = settings.get("community_description", "")

    user = auth.get_user_from_request(request)
    is_admin = auth.is_admin(user)
    profile_url = None
    if user:
        profile_url = f"/users/{quote(user['username'], safe='')}"

    header_html = views.header_with_title(
        community_name,
        "/info",
        logged_in=user is not None,
        title="Community Info",
        user_name=auth.display_username(user),
        is_admin=is_admin,
        profile_url=profile_url,
        info=message,
        error=error,
    )

    description_html = markdown_utils.render_markdown(community_description)
    if not description_html:
        description_html = '<p class="muted">No description yet.</p>'
    info_panel = f"""<div class="info-panel">
  <strong>{webhttp.html_escape(community_name)}</strong>
  <div>{description_html}</div>
</div>"""

    edit_link_html = ""
    if is_admin and not edit_mode:
        edit_link_html = """<div class="actions section-actions">
  <a class="admin-link" href="/info?edit=1">Edit info</a>
</div>"""

    form_html = ""
    if is_admin and edit_mode:
        csrf_html = views.csrf_input(auth.csrf_token(request))
        safe_name = webhttp.html_escape(community_name)
        safe_description = webhttp.html_escape(community_description)
        form_html = f"""<form method="post" action="/info">
  {csrf_html}
  <label for="community_name">Community name</label>
  <input id="community_name" name="community_name" value="{safe_name}" required>
  <label for="community_description">Community description</label>
  <textarea id="community_description" name="community_description" rows="6">{safe_description}</textarea>
  <div class="actions section-actions">
    <button type="submit">Save</button>
    <a class="admin-link secondary" href="/info">Cancel</a>
  </div>
</form>"""

    body = f"""{header_html}
{info_panel}
{edit_link_html}
{form_html}
"""
    return views.render_page("Community Info", body)


def handle(request):
    if request.method not in ("GET", "POST"):
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")

    user = auth.get_user_from_request(request)
    is_admin = auth.is_admin(user)

    if request.method == "POST":
        if not is_admin:
            return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
        form = request.form()
        if not auth.verify_csrf(request, form):
            return webhttp.html_response("<h1>Forbidden</h1>", status="403 Forbidden")
        name = form.get("community_name", [""])[0].strip()
        description = form.get("community_description", [""])[0].strip()
        if not name:
            return _render_info_page(request, error="Community name is required.", edit_mode=True)
        community_settings = dict(config.get_community_config())
        community_settings["community_name"] = name
        community_settings["community_description"] = description
        config.save_community_config(community_settings)
        return webhttp.redirect("/info?updated=1")

    updated = request.query.get("updated", [""])[0] == "1"
    edit_mode = request.query.get("edit", [""])[0] == "1"
    message = "Community info updated." if updated else None
    return _render_info_page(request, message=message, edit_mode=edit_mode)
