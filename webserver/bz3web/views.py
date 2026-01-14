from bz3web import webhttp


def render_page(title, body_html, message=None, header_links_html=None):
    nav_html = ""
    if header_links_html:
        nav_html = f"""
    <div class="nav-strip">
      <div class="header-links">
        {header_links_html}
      </div>
    </div>
"""
    confirm_html = """
    <div class="confirm-modal" id="confirm-modal" hidden>
      <div class="confirm-card" role="dialog" aria-modal="true" aria-labelledby="confirm-title">
        <h3 id="confirm-title">Confirm action</h3>
        <p id="confirm-message">Are you sure?</p>
        <div class="actions">
          <button type="button" class="secondary" id="confirm-cancel">Cancel</button>
          <button type="button" id="confirm-ok">Delete</button>
        </div>
      </div>
    </div>
    <script>
      (() => {
        const modal = document.getElementById('confirm-modal');
        if (!modal) return;
        const message = document.getElementById('confirm-message');
        const cancel = document.getElementById('confirm-cancel');
        const ok = document.getElementById('confirm-ok');
        let pendingForm = null;
        document.addEventListener('submit', (event) => {
          const form = event.target;
          if (!(form instanceof HTMLFormElement)) return;
          const text = form.getAttribute('data-confirm');
          if (!text) return;
          event.preventDefault();
          pendingForm = form;
          message.textContent = text;
          modal.hidden = false;
          cancel.focus();
        });
        cancel.addEventListener('click', () => {
          modal.hidden = true;
          pendingForm = null;
        });
        ok.addEventListener('click', () => {
          if (pendingForm) {
            pendingForm.submit();
          }
          modal.hidden = true;
          pendingForm = null;
        });
        modal.addEventListener('click', (event) => {
          if (event.target === modal) {
            modal.hidden = true;
            pendingForm = null;
          }
        });
        document.addEventListener('keydown', (event) => {
          if (event.key === 'Escape' && !modal.hidden) {
            modal.hidden = true;
            pendingForm = null;
          }
        });
      })();
    </script>
    """
    html = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>{webhttp.html_escape(title)}</title>
  <link rel="stylesheet" href="/static/style.css">
</head>
<body>
  <div class="container">
    {nav_html}
    {body_html}
  </div>
  {confirm_html}
</body>
</html>
"""
    return webhttp.html_response(html)


def nav_links(show_account=True):
    account_html = '<a class="admin-link" href="/account">Account</a>' if show_account else ""
    return f'<a class="admin-link" href="/">Home</a>{account_html}'


def header(
    list_name,
    current_path,
    logged_in,
    show_login=True,
    info=None,
    warning=None,
    error=None,
    logout_url="/logout",
    user_name=None,
):
    links = []
    if logged_in and user_name:
        safe_name = webhttp.html_escape(user_name)
        links.append(f'<div class="header-user">Signed in as <span class="header-username">{safe_name}</span></div>')
    if current_path != "/":
        links.append('<a class="admin-link" href="/">Home</a>')
    if logged_in and current_path != "/account":
        links.append('<a class="admin-link" href="/account">Account</a>')
    if logged_in:
        links.append(f'<a class="admin-link secondary" href="{logout_url}">Logout</a>')
    elif show_login:
        links.append('<a class="admin-link secondary" href="/login">Login</a>')
    links_html = "".join(links)
    notice_html = ""
    if info:
        notice_html = f'<div class="notice notice-info">{webhttp.html_escape(info)}</div>'
    elif warning:
        notice_html = f'<div class="notice notice-warning">{webhttp.html_escape(warning)}</div>'
    elif error:
        notice_html = f'<div class="notice notice-error">{webhttp.html_escape(error)}</div>'
    return f"""<div class="page-header">
  <h1 class="site-title">{webhttp.html_escape(list_name)}</h1>
  <div class="header-links">
    {links_html}
  </div>
</div>
<hr class="section-divider">
{notice_html}"""


def header_with_title(
    list_name,
    current_path,
    logged_in,
    title,
    show_login=True,
    info=None,
    warning=None,
    error=None,
    logout_url="/logout",
    user_name=None,
):
    header_html = header(
        list_name,
        current_path,
        logged_in,
        show_login=show_login,
        info=info,
        warning=warning,
        error=error,
        logout_url=logout_url,
        user_name=user_name,
    )
    return f"""{header_html}
<h2>{webhttp.html_escape(title)}</h2>"""


def render_server_cards(servers, header_title=None, summary_text=None, toggle_url=None, toggle_label=None):
    from bz3web import lightbox, uploads

    header_html = ""
    if header_title:
        summary_html = f'<div class="summary">{summary_text}</div>' if summary_text else ""
        toggle_html = f'<a class="admin-link secondary" href="{toggle_url}">{toggle_label}</a>' if toggle_url else ""
        header_html = f"""<div class="section-header">
  <h2>{webhttp.html_escape(header_title)}</h2>
  {summary_html}
  {toggle_html}
</div>
"""

    cards = []
    for entry in servers:
        name = webhttp.html_escape(entry.get("name") or f"{entry['host']}:{entry['port']}")
        host = webhttp.html_escape(entry["host"])
        port = webhttp.html_escape(entry["port"])
        description = webhttp.html_escape(entry.get("description", ""))
        num_players = entry.get("num_players")
        max_players = entry.get("max_players")
        flags = entry.get("flags", [])
        active = entry.get("active", False)
        owner_name = entry.get("owner") or "Admin"
        owner = webhttp.html_escape(owner_name)
        owner_url = f"/user?name={webhttp.html_escape(owner_name)}"
        approval_note = entry.get("approval_note")
        approval_html = ""
        if approval_note:
            approval_html = f"<div class=\"approval-note\">{webhttp.html_escape(approval_note)}</div>"
        screenshot_id = entry.get("screenshot_id")
        screenshot = None
        full_image = None
        if screenshot_id:
            urls = uploads.screenshot_urls(screenshot_id)
            screenshot = urls.get("thumb")
            full_image = urls.get("full") or screenshot
        actions_html = entry.get("actions_html") or ""
        actions_block = f"<div class=\"card-actions\">{actions_html}</div>" if actions_html else ""
        flags_html = ""
        if flags:
            tags = "".join(f"<span class=\"tag\">{webhttp.html_escape(flag)}</span>" for flag in flags)
            flags_html = f"<div class=\"tags\">{tags}</div>"

        meta_html = ""

        description_html = f"<p>{description}</p>" if description else "<p class=\"muted\">No description provided.</p>"
        screenshot_html = ""
        card_class = "card-top"
        if screenshot:
            card_class = "card-top"
            screenshot_html = (
                f"<img class=\"thumb\" src=\"{webhttp.html_escape(screenshot)}\" "
                f"data-full=\"{webhttp.html_escape(full_image)}\" alt=\"Screenshot\">"
            )
        else:
            card_class = "card-top no-thumb"
        if num_players is not None and max_players is not None:
            online_value = f"{num_players} / {max_players}"
        elif num_players is not None:
            online_value = f"{num_players}"
        elif max_players is not None:
            online_value = f"— / {max_players}"
        else:
            online_value = "—"
        online_label = (
            f"<span class=\"online-label\">Online:</span> <span class=\"online-value\">{online_value}</span>"
            if active
            else "<span class=\"online-label inactive\">Offline</span>"
        )
        online_class = "online active" if active else "online inactive"
        endpoint_html = f"<div class=\"endpoint\">{host}:{port}</div>" if active else ""

        cards.append(
            f"""<article class="card">
  <header>
    <div class="title-block">
      <h3>{name}</h3>
      <div class="owner-line">by <a class="owner-link" href="{owner_url}">{owner}</a></div>
      {approval_html}
    </div>
    <div class="online-block">
      <div class="{online_class}">{online_label}</div>
      {endpoint_html}
      {actions_block}
    </div>
  </header>
  <div class="{card_class}">
    {screenshot_html}
    <div class="card-details">
      {description_html}
      {flags_html}
    </div>
  </div>
</article>"""
        )

    if not cards:
        cards_html = "<p class=\"muted\">No servers to display.</p>"
    else:
        cards_html = "<div class=\"card-grid\">" + "".join(cards) + "</div>"

    return f"""{header_html}{cards_html}
{lightbox.render_lightbox()}
{lightbox.render_lightbox_script()}"""
