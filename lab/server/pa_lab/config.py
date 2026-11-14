"""The daemon's own config (Slice 2 of fork #131): a one-entry allowlist (console B), the loopback
port and the bearer token file. It never reads `~/BDSPLab/consoles.json`, which also lists A.

  {"schema": 1, "host": "127.0.0.1", "port": 8765, "token_file": "~/.config/pa-lab/token",
   "consoles": {"B": {"lab_dir": "~/Library/Application Support/SerialPrograms/Profiles/lab-B/Lab",
                      "profile": "lab-B"}}}
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import secrets
import stat
from typing import Dict, Optional

ALLOWED_CONSOLES = ('B',)
LOOPBACK_HOSTS = ('127.0.0.1',)
TOP_KEYS = ('schema', 'host', 'port', 'token_file', 'consoles')
CONSOLE_KEYS = ('lab_dir', 'profile')


class ConfigError(ValueError):
    pass


class ConsoleConfig:
    def __init__(self, lab_dir: Path, profile: Optional[str]):
        self.lab_dir = lab_dir
        self.profile = profile


class Config:
    def __init__(self, host: str, port: int, token_file: Path, consoles: Dict[str, ConsoleConfig]):
        self.host = host
        self.port = port
        self.token_file = token_file
        self.consoles = consoles


def _path(value, field: str) -> Path:
    if not isinstance(value, str) or not value:
        raise ConfigError('{}: expected a path'.format(field))
    path = Path(os.path.expanduser(value))
    if not path.is_absolute():
        raise ConfigError('{}: {!r} is not an absolute path'.format(field, value))
    return path


def load(path: Path) -> Config:
    try:
        doc = json.loads(Path(path).read_text(encoding='utf-8'))
    except (OSError, ValueError) as e:
        raise ConfigError('{}: {}'.format(path, e))
    if not isinstance(doc, dict):
        raise ConfigError('$: expected an object')
    for key in doc:
        if key not in TOP_KEYS:
            raise ConfigError('{}: unknown key (the config is closed)'.format(key))
    if doc.get('schema') != 1:
        raise ConfigError('schema: expected 1, got {!r}'.format(doc.get('schema')))
    host = doc.get('host', '127.0.0.1')
    if host not in LOOPBACK_HOSTS:
        raise ConfigError('host: {!r} is not loopback; the daemon binds only to 127.0.0.1'.format(host))
    port = doc.get('port')
    if isinstance(port, bool) or not isinstance(port, int) or not 0 <= port <= 65535:
        raise ConfigError('port: expected 0..65535, got {!r}'.format(port))
    consoles = doc.get('consoles')
    if not isinstance(consoles, dict) or sorted(consoles) != sorted(ALLOWED_CONSOLES):
        raise ConfigError('consoles: must list exactly {} (console A is never exposed), got {}'.format(
            ', '.join(ALLOWED_CONSOLES), sorted(consoles) if isinstance(consoles, dict) else consoles))
    out = {}
    for name, entry in consoles.items():
        field = 'consoles.' + name
        if not isinstance(entry, dict):
            raise ConfigError('{}: expected an object'.format(field))
        for key in entry:
            if key not in CONSOLE_KEYS:
                raise ConfigError('{}.{}: unknown key'.format(field, key))
        profile = entry.get('profile')
        if profile is not None and (not isinstance(profile, str) or not profile):
            raise ConfigError('{}.profile: expected a profile name'.format(field))
        out[name] = ConsoleConfig(_path(entry.get('lab_dir'), field + '.lab_dir'), profile)
    return Config(host, port, _path(doc.get('token_file'), 'token_file'), out)


def read_token(path: Path) -> str:
    """The bearer token; refused unless only the owner can read the file."""
    try:
        mode = stat.S_IMODE(os.stat(path).st_mode)
        token = Path(path).read_text(encoding='utf-8').strip()
    except OSError as e:
        raise ConfigError('token_file: {}'.format(e))
    if mode & 0o077:
        raise ConfigError('token_file: {} has mode {:04o}; it must be owner-only (0600)'.format(path, mode))
    if len(token) < 32:
        raise ConfigError('token_file: the token is shorter than 32 characters')
    return token


def create_token(path: Path) -> None:
    """Writes a new random token readable only by the owner. Never overwrites."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        fd = os.open(str(path), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        raise ConfigError('token_file: {} already exists'.format(path))
    with os.fdopen(fd, 'w') as f:
        f.write(secrets.token_urlsafe(32) + '\n')
