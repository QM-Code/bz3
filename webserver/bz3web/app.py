from wsgiref.simple_server import make_server

from bz3web import config, db, router, webhttp


DB_PATH = db.default_db_path()
db.init_db(DB_PATH)


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
    settings = config.get_config()
    host = settings.get("host", "127.0.0.1")
    port = int(settings.get("port", 8080))
    with make_server(host, port, application) as server:
        print(f"Server list server listening on http://{host}:{port}")
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")


if __name__ == "__main__":
    main()
