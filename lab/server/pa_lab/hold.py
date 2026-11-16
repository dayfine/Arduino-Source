"""The user's hold on a console: the file `Lab/hold` (app `Lab.md`, "User hold").

While it names an owner, the app's lab channel refuses every command that presses, starts, stops
or changes the program, the lab scripts refuse to start, and the console's lease is `user`. The
app writes it when the user clicks Start or Stop or presses through the window; `hold.sh on` and
`lease claim-user` write it by hand. Nothing removes it by itself: only `hold.sh off` or
`lease release-user`.

The file is `key=value` lines: `owner`, `reason` (`ui_start`, `ui_stop`, `user_input`,
`hold_sh`, `operator_claim`) and `acquired_at` (a contract timestamp).
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any, Callable, Dict, Optional

from pa_lab import timestamps

HOLD_FILE = 'hold'

# Reasons the user put on by driving the console, as opposed to by hand.
DROVE = ('ui_start', 'ui_stop', 'user_input')


def read_hold(lab_dir: Path) -> Optional[Dict[str, str]]:
    """The hold `Lab/hold` names, or None when there is none or it names no owner. An
    `acquired_at` that is not a contract timestamp becomes the file's modification time. A file
    that is there but cannot be read (no permission, a folder, not UTF-8) is a hold: owner
    `unknown`, reason `unreadable`."""
    path = Path(lab_dir) / HOLD_FILE
    try:
        text = path.read_text(encoding='utf-8')
        mtime = path.stat().st_mtime
    except (FileNotFoundError, NotADirectoryError):
        return None
    except (OSError, UnicodeDecodeError):
        return {'owner': 'unknown', 'reason': 'unreadable', 'acquired_at': timestamps.format(_mtime(path))}
    fields = {}
    for line in text.splitlines():
        key, sep, value = line.partition('=')
        if sep and key in ('owner', 'reason', 'acquired_at'):
            fields[key] = value.strip()
    if not fields.get('owner'):
        return None
    acquired_at = fields.get('acquired_at', '')
    try:
        timestamps.parse(acquired_at)
    except ValueError:
        acquired_at = timestamps.format(mtime)
    return {'owner': fields['owner'], 'reason': fields.get('reason') or 'unknown', 'acquired_at': acquired_at}


def _mtime(path: Path) -> float:
    try:
        return path.stat().st_mtime
    except OSError:
        return timestamps.wall_clock()


def write_hold(lab_dir: Path, reason: str, now: Callable[[], float] = timestamps.wall_clock) -> Dict[str, str]:
    """Puts the user hold on, unless one is already on; returns the hold in force."""
    held = read_hold(lab_dir)
    if held is not None:
        return held
    hold = {'owner': 'user', 'reason': reason, 'acquired_at': timestamps.format(now())}
    path = Path(lab_dir) / HOLD_FILE
    tmp = path.with_name('{}.part.{}'.format(HOLD_FILE, os.getpid()))
    tmp.write_text(''.join('{}={}\n'.format(k, hold[k]) for k in ('owner', 'reason', 'acquired_at')),
                   encoding='utf-8')
    os.replace(tmp, path)
    return hold


def clear_hold(lab_dir: Path) -> bool:
    """Takes the hold off. Returns whether one was on."""
    try:
        (Path(lab_dir) / HOLD_FILE).unlink()
        return True
    except FileNotFoundError:
        return False


def hold_lease(hold: Dict[str, str]) -> Dict[str, Any]:
    """The console's lease while `hold` is on: the user's, with no timeout."""
    return {'state': 'user', 'owner': 'user', 'acquired_at': hold['acquired_at'], 'expires_at': None}


def takeover_reason(hold: Dict[str, str]) -> str:
    """The `lease_changed` reason a hold gives: the user drove the console, or claimed it by hand."""
    return 'ui_takeover' if hold['reason'] in DROVE else 'operator_claim'
