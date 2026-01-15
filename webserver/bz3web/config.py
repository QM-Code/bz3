import json
import os


_CONFIG = None
_CONFIG_PATH = None


def _find_config_path():
    env_path = os.environ.get("BZ3WEB_CONFIG")
    if env_path:
        return env_path
    base_dir = os.path.dirname(__file__)
    candidates = [
        os.path.join(base_dir, "config.json"),
        os.path.normpath(os.path.join(base_dir, "..", "config.json")),
    ]
    for path in candidates:
        if os.path.isfile(path):
            return path
    raise FileNotFoundError("config.json not found.")


def _load_config():
    global _CONFIG_PATH
    config_path = _find_config_path()
    try:
        with open(config_path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except json.JSONDecodeError as exc:
        location = f"line {exc.lineno} column {exc.colno}" if exc.lineno and exc.colno else "unknown location"
        raise ValueError(f"[bz3web] Error: corrupt config.json file ({location}).") from exc
    _CONFIG_PATH = config_path
    return data


def get_config():
    global _CONFIG
    if _CONFIG is None:
        _CONFIG = _load_config()
    return _CONFIG


def get_config_path():
    if _CONFIG_PATH is None:
        _ = get_config()
    return _CONFIG_PATH


def get_config_dir():
    return os.path.dirname(get_config_path())
