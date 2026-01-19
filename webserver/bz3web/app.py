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
        db.init_db(db.default_db_path())
    except ValueError as exc:
        print(str(exc))
        return
    host = settings.get("host", "127.0.0.1")
    port = int(settings.get("port", 8080))
    print(f"Server list server listening on http://{host}:{port}")
    try:
        if serve_app is not None:
            threads = int(settings.get("httpserver", {}).get("threads", 8))
            serve_app(application, host=host, port=port, threads=threads)
            return
        with make_server(host, port, application) as server:
            server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")


if __name__ == "__main__":
    main()
