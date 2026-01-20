import threading
import time

from bz3web import config, webhttp


_LOCK = threading.Lock()
_BUCKETS = {}


def _prune(entries, cutoff):
    return [stamp for stamp in entries if stamp >= cutoff]


def allow(key, max_requests, window_seconds):
    now = time.time()
    cutoff = now - window_seconds
    with _LOCK:
        entries = _BUCKETS.get(key, [])
        entries = _prune(entries, cutoff)
        if len(entries) >= max_requests:
            _BUCKETS[key] = entries
            return False
        entries.append(now)
        _BUCKETS[key] = entries
    return True


def check(settings, request, action):
    limit_cfg = config.require_setting(settings, f"rate_limits.{action}")
    max_requests = config.require_setting(limit_cfg, "max_requests", f"config.json rate_limits.{action}")
    window_seconds = config.require_setting(limit_cfg, "window_seconds", f"config.json rate_limits.{action}")
    try:
        max_requests = int(max_requests)
        window_seconds = int(window_seconds)
    except (TypeError, ValueError):
        raise ValueError(f"[bz3web] Error: rate_limits.{action} must include integer max_requests and window_seconds.")
    client_ip = webhttp.client_ip(request.environ)
    key = f"{action}:{client_ip}"
    return allow(key, max_requests, window_seconds)
