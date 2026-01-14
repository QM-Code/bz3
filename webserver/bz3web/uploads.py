import io
import os
import secrets

from bz3web import config

try:
    from PIL import Image
except ImportError:  # Optional dependency
    Image = None


def _uploads_dir():
    uploads_dir = config.get_config().get("uploads_dir", "uploads")
    base_dir = config.get_config_dir()
    if os.path.isabs(uploads_dir):
        return uploads_dir
    return os.path.normpath(os.path.join(base_dir, uploads_dir))


def ensure_upload_dir():
    os.makedirs(_uploads_dir(), exist_ok=True)


def _save_bytes(filename, data):
    path = os.path.join(_uploads_dir(), filename)
    with open(path, "wb") as handle:
        handle.write(data)
    return filename


def _screenshot_names(token):
    return {
        "original": f"{token}_original.jpg",
        "full": f"{token}_full.jpg",
        "thumb": f"{token}_thumb.jpg",
    }


def screenshot_urls(token):
    if not token:
        return {}
    names = _screenshot_names(token)
    return {key: f"/uploads/{value}" for key, value in names.items()}


def _scale_image(image, max_width, max_height):
    scaled = image.copy()
    scaled.thumbnail((max_width, max_height), Image.LANCZOS)
    return scaled


def handle_upload(file_item):
    settings = config.get_config()
    max_bytes = int(settings.get("upload_max_bytes", 3 * 1024 * 1024))
    min_width = int(settings.get("upload_min_width", 640))
    min_height = int(settings.get("upload_min_height", 360))
    max_width = int(settings.get("upload_max_width", 3840))
    max_height = int(settings.get("upload_max_height", 2160))
    thumb_width = int(settings.get("thumbnail_width", 480))
    thumb_height = int(settings.get("thumbnail_height", 270))
    full_width = int(settings.get("fullsize_width", 1600))
    full_height = int(settings.get("fullsize_height", 900))

    if Image is None:
        return None, "Image processing library (Pillow) is not installed."

    data = file_item.file.read()
    if not data:
        return None, "Uploaded file was empty."
    if len(data) > max_bytes:
        return None, f"File too large (max {max_bytes} bytes)."

    try:
        image = Image.open(io.BytesIO(data))
        image.load()
    except Exception:
        return None, "Uploaded file is not a valid image."

    width, height = image.size
    if width < min_width or height < min_height:
        return None, "Image resolution too small."
    if width > max_width or height > max_height:
        return None, "Image resolution too large."

    ensure_upload_dir()
    token = secrets.token_hex(12)
    names = _screenshot_names(token)

    original = image.convert("RGB")
    original_bytes = io.BytesIO()
    original.save(original_bytes, format="JPEG", quality=90, optimize=True)
    _save_bytes(names["original"], original_bytes.getvalue())

    full_img = _scale_image(original, full_width, full_height)
    full_bytes = io.BytesIO()
    full_img.save(full_bytes, format="JPEG", quality=85, optimize=True)
    _save_bytes(names["full"], full_bytes.getvalue())

    thumb_img = _scale_image(original, thumb_width, thumb_height)
    thumb_bytes = io.BytesIO()
    thumb_img.save(thumb_bytes, format="JPEG", quality=80, optimize=True)
    _save_bytes(names["thumb"], thumb_bytes.getvalue())

    return {"id": token}, None
