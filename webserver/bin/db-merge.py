#!/usr/bin/env python3
import os
import sys


def _parse_args():
    directory = ""
    json_path = ""
    args = sys.argv[1:]
    if not args:
        raise SystemExit("usage: db-merge.py <community-directory> -f <json-file>")
    if args:
        directory = args[0]
        args = args[1:]
    while args:
        token = args.pop(0)
        if token == "-f":
            if not args:
                raise SystemExit("-f <json-file> is mandatory")
            json_path = args.pop(0)
        else:
            raise SystemExit("usage: db-merge.py <community-directory> -f <json-file>")
    if not json_path:
        raise SystemExit("-f <json-file> is mandatory")
    return directory, json_path


def main():
    directory, json_path = _parse_args()

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)

    from bz3web import cli
    from bz3web.tools import import_data as tool

    cli.bootstrap(directory, "usage: db-merge.py <community-directory> -f <json-file>")

    path = os.path.abspath(json_path)
    cli.validate_json_file(path)
    users_ok, users_skip, servers_ok, servers_skip = tool.import_data(
        path, allow_merge=True, overwrite=False
    )
    print(f"Imported {users_ok} users; skipped {users_skip}.")
    print(f"Imported {servers_ok} servers; skipped {servers_skip}.")


if __name__ == "__main__":
    main()
