import json
import os


_CONFIG = None
_BASE_CONFIG = None
_COMMUNITY_CONFIG = None
_CONFIG_PATH = None
_COMMUNITY_DIR = None


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
    global _CONFIG_PATH, _BASE_CONFIG
    config_path = _find_config_path()
    try:
        with open(config_path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except json.JSONDecodeError as exc:
        location = f"line {exc.lineno} column {exc.colno}" if exc.lineno and exc.colno else "unknown location"
        raise ValueError(f"[bz3web] Error: corrupt config.json file ({location}).") from exc
    _CONFIG_PATH = config_path
    _BASE_CONFIG = data
    return data


def set_community_dir(path):
    global _COMMUNITY_DIR, _COMMUNITY_CONFIG, _CONFIG
    _COMMUNITY_DIR = path
    _COMMUNITY_CONFIG = None
    _CONFIG = None


def get_community_dir():
    return _COMMUNITY_DIR


def get_community_config_path():
    community_dir = get_community_dir()
    if not community_dir:
        return None
    return os.path.join(community_dir, "config.json")


def _load_community_config():
    global _COMMUNITY_CONFIG
    community_path = get_community_config_path()
    if not community_path:
        _COMMUNITY_CONFIG = None
        return None
    try:
        with open(community_path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except FileNotFoundError:
        _COMMUNITY_CONFIG = None
        return None
    except json.JSONDecodeError as exc:
        location = f"line {exc.lineno} column {exc.colno}" if exc.lineno and exc.colno else "unknown location"
        raise ValueError(f"[bz3web] Error: corrupt community config.json file ({location}).") from exc
    _COMMUNITY_CONFIG = data
    return data


def _deep_merge(base, override):
    if not isinstance(base, dict) or not isinstance(override, dict):
        return override
    merged = dict(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(merged.get(key), dict):
            merged[key] = _deep_merge(merged[key], value)
        else:
            merged[key] = value
    return merged


def get_config():
    global _CONFIG
    if _CONFIG is None:
        base = _BASE_CONFIG or _load_config()
        community = _COMMUNITY_CONFIG or _load_community_config()
        _CONFIG = _deep_merge(base, community) if community else dict(base)
    return _CONFIG


def save_community_config(data):
    global _CONFIG, _COMMUNITY_CONFIG
    community_path = get_community_config_path()
    if not community_path:
        raise ValueError("[bz3web] Error: community directory not configured.")
    os.makedirs(os.path.dirname(community_path), exist_ok=True)
    with open(community_path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2)
        handle.write("\n")
    _COMMUNITY_CONFIG = data
    _CONFIG = None


def get_community_config():
    if _COMMUNITY_CONFIG is None:
        _load_community_config()
    return _COMMUNITY_CONFIG or {}


def get_config_path():
    if _CONFIG_PATH is None:
        _ = get_config()
    return _CONFIG_PATH


def get_config_dir():
    return os.path.dirname(get_config_path())
