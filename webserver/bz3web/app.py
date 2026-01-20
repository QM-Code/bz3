try:
    from waitress import serve as serve_app
except ImportError:
    serve_app = None
from wsgiref.simple_server import make_server

from bz3web import config, db, router, webhttp


def application(environ, start_response):
    request = webhttp.Request(environ)
    result = router.dispatch(request)
    if result is None:
        status, headers, body = webhttp.html_response("<h1>Not Found</h1>", status="404 Not Found")
    else:
        status, headers, body = result
    start_response(status, headers)
    return [body]


def main():
    try:
        settings = config.get_config()
        if not config.get_community_dir():
            raise ValueError("[bz3web] Error: community directory must be provided.")
        community_settings = config.get_community_config()
        session_secret = community_settings.get("session_secret", "")
        if not session_secret or session_secret == "CHANGE_ME":
            raise ValueError("[bz3web] Error: community config.json must define a unique session_secret.")
        config.require_setting(settings, "data_dir")
        config.require_setting(settings, "uploads_dir")
        config.require_setting(settings, "pages.servers.overview_max_chars")
        config.require_setting(settings, "uploads.screenshots")
        config.require_setting(settings, "uploads.screenshots.limits")
        config.require_setting(settings, "uploads.screenshots.thumbnail")
        config.require_setting(settings, "uploads.screenshots.full")
        config.require_setting(settings, "heartbeat_timeout_seconds")
        config.require_setting(settings, "debug.heartbeat")
        config.require_setting(settings, "debug.auth")
        config.require_setting(settings, "pages.servers.auto_refresh")
        config.require_setting(settings, "pages.servers.auto_refresh_animate")
        config.require_setting(settings, "httpserver.threads")
        config.require_setting(settings, "rate_limits")
        config.require_setting(settings, "rate_limits.login")
        config.require_setting(settings, "rate_limits.register")
        config.require_setting(settings, "rate_limits.forgot")
        config.require_setting(settings, "rate_limits.reset")
        config.require_setting(settings, "rate_limits.api_auth")
        config.require_setting(settings, "rate_limits.api_user_registered")
        db.init_db(db.default_db_path())
    except ValueError as exc:
        print(str(exc))
        return
    host = config.require_setting(settings, "host")
    port = settings.get("port")
    if port is None:
        print("[bz3web] Error: No port specified in config.json. Use -p <port> to specify a port.")
        return
    port = int(port)
    print(f"Server list server listening on http://{host}:{port}")
    try:
        if serve_app is not None:
            threads = int(config.require_setting(settings, "httpserver.threads"))
            serve_app(application, host=host, port=port, threads=threads)
            return
        with make_server(host, port, application) as server:
            server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")


if __name__ == "__main__":
    main()
