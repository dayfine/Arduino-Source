"""`console_status`: what `~/BDSPLab/checkin.sh` reports, read from one profile folder.

The lane tick's console-state read. It only reads files (and lists processes with `pgrep`): the
legacy `Lab/state`, the `Lab/state.json` resource when the app writes one, the tail of
`SerialPrograms.log`, `~/BDSPLab/keepawake.log` and the newest screenshots. It never writes into
`Lab/inbox`, presses a button or starts or stops anything.

| checkin.sh line | field |
|---|---|
| `lab app: <pid> <deployment>` | `app` |
| `Program State:` line | `program_state` |
| `[Checkpoint] ...: passed/failed`, `Starting Program:` | `last_run` (last two) |
| `last press` | `last_press` |
| `newest failure frame` | `newest_failure_frame` |
"""

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
from typing import Any, Callable, Dict, List, Optional

from pa_lab import legacy, timestamps
from pa_lab.lab_reader import ConsoleReader

LOG_TAIL_BYTES = 2_000_000
MAX_LOG_LINES = 200
DEFAULT_LOG_LINES = 20
MAX_LINE_CHARS = 300
SHOT_SUFFIXES = ('.png', '.jpg', '.jpeg')
MAX_DIR_ENTRIES = 5000

RUN_RE = re.compile(r'\[Checkpoint\] [0-9.a-z]+: (passed|failed)|Starting Program:')
DEPLOYMENT_RE = re.compile(r'deployments/([^/]+/[^/]+)/')


def _tail_lines(path: Path, max_bytes: int = LOG_TAIL_BYTES) -> List[str]:
    """The complete lines in the last `max_bytes` of a file; [] when it cannot be read."""
    try:
        with open(path, 'rb') as f:
            f.seek(0, os.SEEK_END)
            size = f.tell()
            f.seek(max(0, size - max_bytes))
            data = f.read(max_bytes)
    except OSError:
        return []
    lines = data.decode('utf-8', errors='replace').splitlines()
    if size > max_bytes and lines:
        lines = lines[1:]  # the first line was cut
    return lines


def _clip(line: str) -> str:
    return line if len(line) <= MAX_LINE_CHARS else line[:MAX_LINE_CHARS] + '...'


def _log_time(line: str) -> Optional[str]:
    """`2026-11-15 00:12:15.959076 - ...` -> `00:12:15`."""
    return line[11:19] if len(line) >= 19 and line[4] == '-' and line[13] == ':' else None


def _newest(paths: List[Path]) -> Optional[Path]:
    best, best_mtime = None, -1.0
    for path in paths:
        try:
            mtime = path.stat().st_mtime
        except OSError:
            continue
        if mtime > best_mtime:
            best, best_mtime = path, mtime
    return best


def _files(directory: Path) -> List[Path]:
    out = []
    try:
        with os.scandir(directory) as entries:
            for i, entry in enumerate(entries):
                if i >= MAX_DIR_ENTRIES:
                    break
                if entry.is_file() and entry.name.lower().endswith(SHOT_SUFFIXES):
                    out.append(Path(entry.path))
    except OSError:
        pass
    return out


def _subdirs(directory: Path) -> List[Path]:
    try:
        with os.scandir(directory) as entries:
            return [Path(e.path) for e in entries if e.is_dir()]
    except OSError:
        return []


def pgrep_app(profile: str) -> Optional[Dict[str, Any]]:
    """The lab app running with `--profile <profile>`, as checkin.sh finds it."""
    try:
        out = subprocess.run(['pgrep', '-lf', 'SerialPrograms --profile ' + profile],
                             capture_output=True, text=True, timeout=5).stdout
    except (OSError, subprocess.SubprocessError):
        return None
    for line in out.splitlines():
        pid, _, command = line.partition(' ')
        if not pid.isdigit() or not command.endswith('--profile ' + profile) or 'lablock.sh' in command:
            continue
        match = DEPLOYMENT_RE.search(command)
        return {'pid': int(pid), 'deployment': match.group(1) if match else None}
    return None


class ProfileStatus:
    """One profile folder (e.g. `.../Profiles/lab-B`) read the way checkin.sh reads it."""

    def __init__(self, console: str, profile_dir: Path, keepawake_log: Optional[Path] = None,
                 find_app: Callable[[str], Optional[Dict[str, Any]]] = pgrep_app,
                 now: Callable[[], float] = timestamps.wall_clock):
        self.console = console
        self.profile_dir = Path(profile_dir)
        self.keepawake_log = keepawake_log
        self.find_app = find_app
        self.now = now
        self.reader = ConsoleReader(console, self.profile_dir / 'Lab', profile=self.profile_dir.name, now=now)

    def _lab_state(self) -> Optional[Dict[str, Any]]:
        path = self.profile_dir / 'Lab' / 'state'
        try:
            text = path.read_text(encoding='utf-8')
            mtime = path.stat().st_mtime
        except OSError:
            return None
        try:
            fields = legacy.parse_legacy_state(text)
        except ValueError as e:
            return {'error': str(e)}
        fields.pop('owner', None)  # the owner token stays in the app's file
        fields['written_at'] = timestamps.format(mtime)
        return fields

    def _screenshots(self) -> Dict[str, Optional[str]]:
        root = self.profile_dir / 'Screenshots'
        failures = _newest(_files(root / 'AutoStory' / 'failures'))
        candidates = _files(root)
        for sub in _subdirs(root):
            candidates += _files(sub)
            for subsub in _subdirs(sub):
                candidates += _files(subsub)
        newest = _newest(candidates)
        return {'newest_failure_frame': str(failures) if failures else None,
                'newest_screenshot': str(newest) if newest else None}

    def console_status(self, log_lines: int = DEFAULT_LOG_LINES) -> Dict[str, Any]:
        log_lines = max(0, min(MAX_LOG_LINES, log_lines))
        lines = _tail_lines(self.profile_dir / 'SerialPrograms.log')
        program_state = [l for l in lines if 'Program State:' in l]
        runs = [l for l in lines if RUN_RE.search(l)]
        presses = [l for l in lines if 'issue_' in l and 'nop' not in l]
        keepawake = _tail_lines(self.keepawake_log, 4096) if self.keepawake_log else []
        doc = {
            'console': self.console,
            'observed_at': timestamps.format(self.now()),
            'profile_dir': str(self.profile_dir),
            'app': self.find_app(self.profile_dir.name),
            'lab_state': self._lab_state(),
            'console_state': self.reader.console_state(),
            'program_state': _clip(program_state[-1]) if program_state else None,
            'last_run': [_clip(l) for l in runs[-2:]],
            'last_press': _log_time(presses[-1]) if presses else None,
            'keepawake': keepawake[-1] if keepawake else None,
            'log_tail': [_clip(l) for l in lines[-log_lines:]] if log_lines else [],
        }
        doc.update(self._screenshots())
        return doc
