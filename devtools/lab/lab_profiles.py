"""Seed a new lab profile from display preferences, never a full settings clone."""

import json
import os
from pathlib import Path
import re
import shutil
import tempfile


SETTINGS = "UserSettings/SerialPrograms-Settings.json"
PREFERENCES = {"THEME", "WINDOW_SIZE", "LOG_WINDOW_SIZE", "CONSOLE_SETTINGS_COLLAPSED"}


def lab_profile(name):
    """Require an explicit lab name, distinct from default and unit-test profiles."""
    if not re.fullmatch(r"lab-[A-Za-z0-9_][A-Za-z0-9_-]*", name):
        raise ValueError("Profile must be named lab-NAME (for example lab-A).")
    return name


def read_json(path):
    with Path(path).open(encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError("Expected a JSON object.")
    return value


def write_json(path, value):
    with Path(path).open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")


def seed(source, root, profile):
    """Create an unused Profiles/lab-NAME; leave the source and existing profiles alone."""
    profile = lab_profile(profile)
    source, root = Path(source).resolve(), Path(root).resolve()
    destination = root / "Profiles" / profile
    if destination.exists() or destination.is_symlink():
        raise ValueError("Destination profile already exists; refusing to overwrite it.")
    original = read_json(source / SETTINGS)
    if not isinstance(original, dict):
        raise ValueError("Settings root must be an object.")
    settings = original.get("20-GlobalSettings", {})
    if not isinstance(settings, dict):
        raise ValueError("Global settings must be an object.")
    # Allowlist rather than deleting known device keys: future settings default to omitted.
    safe = {key: value for key, value in settings.items() if key in PREFERENCES}
    safe.update({"LOG_WINDOW_STARTUP": False, "RICH_PRESENCE": False})
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.parent.resolve() != root / "Profiles":
        raise ValueError("Profiles must not redirect outside the requested root.")
    temporary = Path(tempfile.mkdtemp(prefix=".seed-", dir=destination.parent))
    os.chmod(temporary, 0o700)
    try:
        (temporary / "UserSettings").mkdir()
        write_json(temporary / SETTINGS, {"20-GlobalSettings": safe, "99-Panels": {}})
        write_json(temporary / "lab-profile.json", {
            "schema": 1, "profile": profile, "hardware_configured": False,
            "seed_policy": "display-preferences-only",
        })
        # mkdir is exclusive, including against an empty destination created concurrently.
        destination.mkdir(mode=0o700)
        for child in temporary.iterdir():
            child.rename(destination / child.name)
    finally:
        shutil.rmtree(temporary)
    return destination
