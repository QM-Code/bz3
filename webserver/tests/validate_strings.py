#!/usr/bin/env python3
import json
import os
import sys


def _usage():
    return "usage: validate_strings.py (--all | <lang-code> | <path-to-json>)"


def _load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _missing_paths(source, target, prefix=""):
    missing = []
    if isinstance(source, dict):
        if not isinstance(target, dict):
            for key in source.keys():
                path = f"{prefix}{key}" if not prefix else f"{prefix}.{key}"
                missing.append(path)
            return missing
        for key, value in source.items():
            path = f"{prefix}{key}" if not prefix else f"{prefix}.{key}"
            if key not in target:
                missing.append(path)
                continue
            missing.extend(_missing_paths(value, target.get(key), path))
    return missing


def _strings_dir():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    return os.path.join(root_dir, "strings")


def _resolve_target(arg):
    if os.path.isfile(arg):
        return os.path.abspath(arg)
    if arg.endswith(".json"):
        return os.path.abspath(arg)
    strings_dir = _strings_dir()
    candidate = os.path.join(strings_dir, f"{arg}.json")
    if os.path.isfile(candidate):
        return candidate
    return None


def _check_file(path, en_data):
    target = _load_json(path)
    missing = _missing_paths(en_data, target)
    return missing


def main():
    args = sys.argv[1:]
    if not args:
        raise SystemExit(_usage())

    strings_dir = _strings_dir()
    en_path = os.path.join(strings_dir, "en.json")
    if not os.path.isfile(en_path):
        raise SystemExit("strings/en.json not found.")
    en_data = _load_json(en_path)

    if args[0] == "--all":
        paths = []
        for name in os.listdir(strings_dir):
            if name.endswith(".json") and name != "en.json":
                paths.append(os.path.join(strings_dir, name))
        if not paths:
            print("No language files found.")
            return
        failed = False
        for path in sorted(paths):
            missing = _check_file(path, en_data)
            if missing:
                failed = True
                print(f"{os.path.basename(path)} missing keys:")
                for key in missing:
                    print(f"  - {key}")
        if failed:
            raise SystemExit(1)
        print("All language files include required keys.")
        return

    target_path = _resolve_target(args[0])
    if not target_path:
        raise SystemExit(_usage())

    missing = _check_file(target_path, en_data)
    if missing:
        print(f"Missing keys in {target_path}:")
        for key in missing:
            print(f"  - {key}")
        raise SystemExit(1)
    print("All required keys present.")


if __name__ == "__main__":
    main()
