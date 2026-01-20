import urllib.parse

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
          const label = form.getAttribute('data-confirm-label');
          const style = form.getAttribute('data-confirm-style');
          if (label) {
            ok.textContent = label;
          } else {
            ok.textContent = 'Delete';
          }
          ok.classList.remove('danger', 'success');
          if (style === 'success') {
            ok.classList.add('success');
          } else if (style === 'danger') {
            ok.classList.add('danger');
          }
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
    <script>
      (() => {
        document.addEventListener('change', (event) => {
          const input = event.target;
          if (!(input instanceof HTMLInputElement) || input.type !== 'checkbox') return;
          const form = input.closest('form.js-toggle-form');
          if (!form) return;
          event.preventDefault();
          const formData = new FormData(form);
          input.disabled = true;
          fetch(form.action, {
            method: (form.method || 'post').toUpperCase(),
            body: formData,
            credentials: 'same-origin',
          })
            .then((response) => {
              if (!response.ok) {
                throw new Error('toggle failed');
              }
            })
            .catch(() => {
              input.checked = !input.checked;
            })
            .finally(() => {
              input.disabled = false;
            });
        });
      })();
    </script>
    <script>
      (() => {
        const escapeHtml = (value) => {
          if (value === null || value === undefined) return "";
          return String(value)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#39;");
        };
        const buildCardElement = (server, allowActions) => {
          const rawName = server.name || `${server.host}:${server.port}`;
          const name = escapeHtml(rawName);
          const serverUrl = `/servers/${encodeURIComponent(rawName)}`;
          const owner = escapeHtml(server.owner || "");
          const ownerUrl = `/users/${encodeURIComponent(server.owner || "")}`;
          const overviewText = server.overview || "";
          const description = overviewText
            ? `<p>${escapeHtml(overviewText)}</p>`
            : '<p class="muted">No overview provided.</p>';
          const hasScreenshot = Boolean(server.screenshot_id);
          const cardClass = hasScreenshot ? "card-top" : "card-top no-thumb";
          const screenshotHtml = hasScreenshot
            ? `<img class="thumb" src="/uploads/${escapeHtml(server.screenshot_id)}_thumb.jpg" data-full="/uploads/${escapeHtml(server.screenshot_id)}_full.jpg" alt="Screenshot">`
            : "";
          let onlineValue = "—";
          if (server.num_players !== undefined && server.num_players !== null && server.max_players !== undefined && server.max_players !== null) {
            onlineValue = `${server.num_players} / ${server.max_players}`;
          } else if (server.num_players !== undefined && server.num_players !== null) {
            onlineValue = `${server.num_players}`;
          } else if (server.max_players !== undefined && server.max_players !== null) {
            onlineValue = `— / ${server.max_players}`;
          }
          const onlineLabel = server.active
            ? `<span class="online-label">Online:</span> <span class="online-value">${escapeHtml(onlineValue)}</span>`
            : '<span class="online-label inactive">Offline</span>';
          const onlineClass = server.active ? "online active" : "online inactive";
          const endpointHtml = server.active
            ? `<div class="endpoint">${escapeHtml(server.host)}:${escapeHtml(server.port)}</div>`
            : "";
          let actionsBlock = "";
          if (allowActions && server.id !== undefined && server.id !== null) {
            actionsBlock = `<div class="card-actions">
  <form method="get" action="/server/edit">
    <input type="hidden" name="id" value="${escapeHtml(server.id)}">
    <button type="submit" class="secondary small">Edit</button>
  </form>
  <form method="post" action="/server/delete" data-confirm="Delete this server permanently?">
    <input type="hidden" name="id" value="${escapeHtml(server.id)}">
    <button type="submit" class="secondary small">Delete</button>
  </form>
</div>`;
          }
          const html = `<article class="card">
  <header>
    <div class="title-block">
      <h3><a class="server-link" href="${serverUrl}">${name}</a></h3>
      <div class="owner-line">by <a class="owner-link" href="${ownerUrl}">${owner}</a></div>
    </div>
    <div class="online-block">
      <div class="${onlineClass}">${onlineLabel}</div>
      ${endpointHtml}
      ${actionsBlock}
    </div>
  </header>
  <div class="${cardClass}">
    ${screenshotHtml}
    <div class="card-details">
      ${description}
    </div>
  </div>
</article>`;
          const template = document.createElement("template");
          template.innerHTML = html.trim();
          return template.content.firstElementChild;
        };
        const renderCards = (section, servers) => {
          const container = section.querySelector('[data-role="server-cards"]');
          if (!container) return;
          const animate = section.dataset.animate === "1";
          if (!servers.length) {
            container.innerHTML = '<p class="muted">No servers to display.</p>';
            return;
          }
          const grid = container.querySelector(".card-grid") || document.createElement("div");
          grid.className = "card-grid";
          const existingCards = {};
          grid.querySelectorAll(".card[data-server-id]").forEach((card) => {
            existingCards[card.dataset.serverId] = card;
          });
          const oldRects = {};
          if (animate) {
            Object.values(existingCards).forEach((card) => {
              oldRects[card.dataset.serverId] = card.getBoundingClientRect();
            });
          }
          const allowActions = section.dataset.allowActions === "1";
          const newCards = [];
          servers.forEach((server) => {
            const key = String(server.id ?? (server.name || `${server.host}:${server.port}`));
            let card = existingCards[key];
            const nextCard = buildCardElement(server, allowActions);
            nextCard.dataset.serverId = key;
            if (card) {
              card.dataset.serverId = key;
              card.innerHTML = nextCard.innerHTML;
              newCards.push(card);
            } else {
              newCards.push(nextCard);
            }
          });
          grid.replaceChildren(...newCards);
          if (!container.contains(grid)) {
            container.replaceChildren(grid);
          }
          if (animate) {
            const movers = [];
            grid.querySelectorAll(".card[data-server-id]").forEach((card) => {
              const prev = oldRects[card.dataset.serverId];
              if (!prev) return;
              const next = card.getBoundingClientRect();
              const dx = prev.left - next.left;
              const dy = prev.top - next.top;
              if (!dx && !dy) return;
              card.style.transform = `translate(${dx}px, ${dy}px)`;
              movers.push(card);
            });
            if (movers.length) {
              movers.forEach((card) => {
                void card.offsetHeight;
              });
              requestAnimationFrame(() => {
                movers.forEach((card) => {
                  card.style.transition = "transform 420ms ease";
                  card.style.transform = "";
                });
                const cleanup = () => {
                  movers.forEach((card) => {
                    card.style.transition = "";
                  });
                };
                setTimeout(cleanup, 460);
              });
            }
          }
        };
        const updateSummary = (section, activeCount, inactiveCount) => {
          const summary = section.querySelector('[data-role="server-summary"]');
          if (!summary) return;
          summary.innerHTML = `<strong>${activeCount} online</strong> / ${inactiveCount} offline`;
        };
        const refreshSection = (section) => {
          const url = section.dataset.refreshUrl;
          if (!url) return;
          fetch(url, { credentials: "same-origin" })
            .then((response) => response.json())
            .then((payload) => {
              if (!payload || !Array.isArray(payload.servers)) return;
              renderCards(section, payload.servers);
              if (typeof payload.active_count === "number" && typeof payload.inactive_count === "number") {
                updateSummary(section, payload.active_count, payload.inactive_count);
              }
            })
            .catch(() => {});
        };
        document.querySelectorAll("[data-refresh-url]").forEach((section) => {
          const interval = parseInt(section.dataset.refreshInterval || "10", 10);
          refreshSection(section);
          setInterval(() => refreshSection(section), interval * 1000);
        });
      })();
    </script>
    """
    html = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
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


def csrf_input(token):
    safe_token = webhttp.html_escape(token or "")
    return f'<input type="hidden" name="csrf_token" value="{safe_token}">'


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
    is_admin=False,
    profile_url=None,
):
    links = []
    user_html = ""
    if logged_in and user_name:
        safe_name = webhttp.html_escape(user_name)
        user_html = f'<div class="header-user">Signed in as <span class="header-username">{safe_name}</span></div>'
    if current_path != "/info":
        links.append('<a class="admin-link" href="/info">Info</a>')
    if current_path != "/servers":
        links.append('<a class="admin-link" href="/servers">Servers</a>')
    if logged_in and profile_url and current_path != profile_url:
        links.append(f'<a class="admin-link" href="{profile_url}">Account</a>')
    if logged_in and is_admin and current_path != "/users":
        links.append('<a class="admin-link" href="/users">Users</a>')
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
  <div class="header-actions">
    <div class="header-links">
      {links_html}
    </div>
    {user_html}
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
    is_admin=False,
    profile_url=None,
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
        is_admin=is_admin,
        profile_url=profile_url,
    )
    return f"""{header_html}
<h2>{webhttp.html_escape(title)}</h2>"""


def render_server_cards(
    servers,
    header_title=None,
    summary_text=None,
    toggle_url=None,
    toggle_label=None,
    header_title_html=None,
):
    from bz3web import lightbox, uploads

    header_html = ""
    if header_title or header_title_html:
        title_html = header_title_html or webhttp.html_escape(header_title)
        summary_html = f'<div class="summary server-summary" data-role="server-summary">{summary_text}</div>' if summary_text else ""
        toggle_html = f'<a class="admin-link secondary" href="{toggle_url}">{toggle_label}</a>' if toggle_url else ""
        header_html = f"""<div class="section-header">
  <h2>{title_html}</h2>
  {summary_html}
  {toggle_html}
</div>
"""

    cards = []
    for entry in servers:
        raw_name = entry.get("name") or f"{entry['host']}:{entry['port']}"
        name = webhttp.html_escape(raw_name)
        host = webhttp.html_escape(entry["host"])
        port = webhttp.html_escape(entry["port"])
        overview = webhttp.html_escape(entry.get("overview", ""))
        overview_html = entry.get("overview_html")
        num_players = entry.get("num_players")
        max_players = entry.get("max_players")
        active = entry.get("active", False)
        owner_name = entry["owner"]
        owner = webhttp.html_escape(owner_name)
        owner_url = f"/users/{urllib.parse.quote(owner_name, safe='')}"
        screenshot_id = entry.get("screenshot_id")
        screenshot = None
        full_image = None
        if screenshot_id:
            urls = uploads.screenshot_urls(screenshot_id)
            screenshot = urls.get("thumb")
            full_image = urls.get("full") or screenshot
        actions_html = entry.get("actions_html") or ""
        actions_block = f"<div class=\"card-actions\">{actions_html}</div>" if actions_html else ""
        if overview_html:
            description_block = overview_html
        else:
            description_block = (
                f"<p>{overview}</p>" if overview else "<p class=\"muted\">No overview provided.</p>"
            )
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

        name_link = f'<a class="server-link" href="/servers/{urllib.parse.quote(raw_name, safe="")}">{name}</a>'
        card_id = entry.get("id")
        if card_id is None:
            card_id = raw_name
        cards.append(
            f"""<article class="card" data-server-id="{webhttp.html_escape(str(card_id))}">
  <header>
    <div class="title-block">
      <h3>{name_link}</h3>
      <div class="owner-line">by <a class="owner-link" href="{owner_url}">{owner}</a></div>
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
      {description_block}
    </div>
  </div>
</article>"""
        )

    if not cards:
        cards_html = "<p class=\"muted\">No servers to display.</p>"
    else:
        cards_html = "<div class=\"card-grid\">" + "".join(cards) + "</div>"

    return f"""{header_html}<div class="server-cards" data-role="server-cards">{cards_html}</div>
{lightbox.render_lightbox()}
{lightbox.render_lightbox_script()}"""


def render_server_section(
    rows,
    timeout,
    show_inactive,
    entry_builder,
    header_title=None,
    header_title_html=None,
    toggle_on_url=None,
    toggle_off_url=None,
    refresh_url=None,
    refresh_interval=10,
    allow_actions=False,
    refresh_animate=False,
):
    from bz3web.server_status import is_active

    active_count = 0
    inactive_count = 0
    entries = []
    for row in rows:
        active = is_active(row, timeout)
        if active:
            active_count += 1
        else:
            inactive_count += 1
        if show_inactive:
            if active:
                continue
        else:
            if not active:
                continue
        entry = entry_builder(row, active)
        if entry is None:
            continue
        if "active" not in entry:
            entry["active"] = active
        entries.append(entry)

    entries.sort(key=lambda item: item.get("num_players") if item.get("num_players") is not None else -1, reverse=True)
    summary_text = f"<strong>{active_count} online</strong> / {inactive_count} offline"
    toggle_url = toggle_on_url if not show_inactive else toggle_off_url
    toggle_label = "Show offline servers" if not show_inactive else "Show online servers"
    section_html = render_server_cards(
        entries,
        header_title=header_title,
        header_title_html=header_title_html,
        summary_text=summary_text,
        toggle_url=toggle_url,
        toggle_label=toggle_label,
    )
    if refresh_url:
        refresh_attr = webhttp.html_escape(refresh_url)
        allow_value = "1" if allow_actions else "0"
        inactive_value = "1" if show_inactive else "0"
        animate_value = "1" if refresh_animate else "0"
        return (
            f'<section class="server-section" data-refresh-url="{refresh_attr}" '
            f'data-refresh-interval="{refresh_interval}" data-allow-actions="{allow_value}" '
            f'data-show-inactive="{inactive_value}" data-animate="{animate_value}">{section_html}</section>'
        )
    return section_html


def render_admins_section(
    admins,
    show_controls=False,
    show_add_form=False,
    admin_input="",
    form_prefix="/users",
    notice_html=None,
    header_title_html=None,
    csrf_token="",
):
    csrf_html = csrf_input(csrf_token)
    if show_controls:
        admin_rows = "".join(
            f"""<tr>
  <td><a class="admin-user-link" href="/users/{urllib.parse.quote(admin["username"], safe='')}">{webhttp.html_escape(admin["username"])}</a></td>
  <td class="center-cell">
    <form method="post" action="{form_prefix}/admins/trust" class="js-toggle-form admin-toggle">
      {csrf_html}
      <input type="hidden" name="username" value="{webhttp.html_escape(admin["username"])}">
      <label class="switch">
        <input type="checkbox" name="trust_admins" value="1" {"checked" if admin["trust_admins"] else ""}>
        <span class="slider"></span>
      </label>
    </form>
  </td>
  <td class="center-cell">
    <form method="post" action="{form_prefix}/admins/remove" class="admin-action">
      {csrf_html}
      <input type="hidden" name="username" value="{webhttp.html_escape(admin["username"])}">
      <button type="submit" class="secondary small">Remove</button>
    </form>
  </td>
</tr>"""
            for admin in admins
        ) or "<tr><td colspan=\"3\">No admins assigned.</td></tr>"
        table_head = """<thead>
    <tr>
      <th>Username</th>
      <th class="center-cell">Trust user's admins</th>
      <th class="center-cell">Actions</th>
    </tr>
  </thead>"""
    else:
        admin_rows = "".join(
            f"""<tr>
  <td><a class="admin-user-link" href="/users/{urllib.parse.quote(admin["username"], safe='')}">{webhttp.html_escape(admin["username"])}</a></td>
  <td>{'Yes' if admin["trust_admins"] else 'No'}</td>
</tr>"""
            for admin in admins
        ) or "<tr><td colspan=\"2\">No admins assigned.</td></tr>"
        table_head = """<thead>
    <tr>
      <th>Username</th>
      <th class="center-cell">Trust user's admins</th>
    </tr>
  </thead>"""

    add_form_html = ""
    if show_add_form:
        safe_input = webhttp.html_escape(admin_input)
        add_form_html = f"""<form method="post" action="{form_prefix}/admins/add">
  {csrf_html}
  <div class="row">
    <div>
      <label for="admin_username">Add admin by username</label>
      <input id="admin_username" name="username" required value="{safe_input}">
    </div>
  </div>
  <div class="actions">
    <button type="submit">Add admin</button>
  </div>
</form>
"""

    notice_block = notice_html or ""
    title_html = header_title_html or "Admins"
    return f"""<h2>{title_html}</h2>
{notice_block}
<table>
  {table_head}
  <tbody>
    {admin_rows}
  </tbody>
</table>
{add_form_html}"""
