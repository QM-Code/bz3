#!/usr/bin/env python3
import os
import sys


def main():
    args = sys.argv[1:]
    if not args:
        raise SystemExit("usage: start.py <community-directory> [-p <port>]")
    directory = args.pop(0)
    port_override = None
    while args:
        token = args.pop(0)
        if token == "-p":
            if not args:
                raise SystemExit("usage: start.py <community-directory> [-p <port>]")
            try:
                port_override = int(args.pop(0))
            except ValueError:
                raise SystemExit("Port must be an integer.")
        else:
            raise SystemExit("usage: start.py <community-directory> [-p <port>]")

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)

    from bz3web import app, config
    from bz3web import cli

    cli.bootstrap(directory, "usage: start.py <community-directory>")
    if port_override is not None:
        config.get_config()["port"] = port_override
    app.main()


if __name__ == "__main__":
    main()
