import os

try:
    from waitress import serve as serve_app
except ImportError:
    serve_app = None
from wsgiref.simple_server import make_server

from bz3web import auth, config, db, router, views, webhttp


def application(environ, start_response):
    request = webhttp.Request(environ)
    pending_headers = []
    if request.method == "GET":
        token = auth.ensure_csrf_cookie(request, pending_headers)
        if token:
            request.environ["BZ3WEB_CSRF_TOKEN"] = token
    result = router.dispatch(request)
    if result is None:
        result = views.error_page("404 Not Found", "not_found")
    status, headers, body = result
    if pending_headers:
        content_type = ""
        for key, value in headers:
            if key.lower() == "content-type":
                content_type = value or ""
                break
        if content_type.startswith("text/html"):
            headers = headers + pending_headers
    start_response(status, headers)
    return [body]


def main():
    try:
        settings = config.get_config()
        config.validate_startup(settings)
        if not config.get_community_dir():
            raise ValueError("[bz3web] Error: community directory must be provided.")
        community_settings = config.get_community_config()
        session_secret = community_settings.get("session_secret", "")
        if not session_secret or session_secret == "CHANGE_ME":
            raise ValueError("[bz3web] Error: community config.json must define a unique session_secret.")
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
        if os.environ.get("BZ3WEB_SIGINT_HANDLED") != "1":
            print("\nServer stopped.")


if __name__ == "__main__":
    main()
