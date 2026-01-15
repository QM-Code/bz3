from urllib.parse import quote

from bz3web import auth, config, views, webhttp


def handle(request):
    if request.method != "GET":
        return webhttp.html_response("<h1>Method Not Allowed</h1>", status="405 Method Not Allowed")
    user = auth.get_user_from_request(request)
    if not user:
        return webhttp.redirect("/login")
    profile_url = f"/users/{quote(user['username'], safe='')}"
    header_html = views.header_with_title(
        config.get_config().get("community_name", "Server List"),
        "/admin-docs",
        logged_in=True,
        title="Admin Documentation",
        user_name=auth.display_username(user),
        is_admin=auth.is_admin(user),
        profile_url=profile_url,
    )
    body = f"""{header_html}
<div class="admin-docs-text">
  <p>You are a <strong>Community Admin</strong>.</p>
  <p>Community admins can:</p>
  <ol>
    <li>Administer the community website (this site).</li>
    <li>Act as admins in all game worlds.</li>
  </ol>
  <p>Admin propagation is one level deep. The root admin assigns primary admins. If a primary admin is marked as trusted, their direct admins become secondary admins.</p>
  <p>Admin abilities like modifying users, locking accounts, and editing worlds should only apply to admins below you in the hierarchy. This enforcement is still being expanded.</p>
</div>
<div class="diagram">
  <img src="/static/admin-flow.svg" alt="Admin flow diagram">
</div>
"""
    return views.render_page("Admin Documentation", body)
