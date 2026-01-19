#!/usr/bin/env python3
import os
import sys


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: start.py <community-directory>")
    directory = sys.argv[1]

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)

    from bz3web import app
    from bz3web import cli

    cli.bootstrap(directory, "usage: start.py <community-directory>")
    app.main()


if __name__ == "__main__":
    main()
