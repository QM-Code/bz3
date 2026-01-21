#!/usr/bin/env python3
import json
import os
import sys
import zipfile
from datetime import datetime


def _parse_args():
    args = sys.argv[1:]
    if not args:
        raise SystemExit("usage: db-snapshot.py <community-directory> [output.json|-] [-z]")
    directory = args.pop(0)
    output_path = None
    zip_output = False
    while args:
        token = args.pop(0)
        if token == "-z":
            zip_output = True
        elif output_path is None:
            output_path = token
        else:
            raise SystemExit("usage: db-snapshot.py <community-directory> [output.json|-] [-z]")
    return directory, output_path, zip_output


def main():
    directory, output_path, zip_output = _parse_args()

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)

    from bz3web import cli
    from bz3web import db
    from bz3web.tools import export_data as tool

    cli.bootstrap(directory, "usage: db-snapshot.py <community-directory> [output.json|-] [-z]")

    payload = tool.export_data()
    output = json.dumps(payload, indent=2)
    if output_path == "-":
        print(output)
        return

    path = output_path
    if not path:
        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        database_dir = os.path.dirname(db.default_db_path())
        path = os.path.join(database_dir, f"snapshot-{timestamp}.json")
        print(f"Exporting snapshot to {path}")
    path = os.path.abspath(path)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(output + "\n")

    if zip_output:
        zip_path = f"{os.path.splitext(path)[0]}.zip"
        with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.write(path, arcname=os.path.basename(path))
        os.remove(path)
        print(f"Wrote {zip_path}")


if __name__ == "__main__":
    main()
