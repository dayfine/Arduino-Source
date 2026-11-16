"""The version-1 app channel over the profile's `Lab/` folder (`contracts/v1/channel.md`).

The only module that knows the folder layout (`inbox/`, `outbox/`, `done/`, `state.json`,
`save.json`), the request and journal line formats, the command text and LabChannel's error text.
Everything else uses `pa_lab.channel`'s types.

Transport: a file in `inbox/` (atomic rename), answered by a journal in `outbox/` that the app
writes after its next 500 ms poll. A local socket is the likely replacement if poll latency or
missed events become a measured problem; the alternatives are in lab/tooling's
docs/design/Agent-Integration.md, "Transport: now and later".
"""

from __future__ import annotations

import json
import os
import re
import secrets
from pathlib import Path
from typing import Any, Dict, Optional, Tuple, Type

from pa_lab import hold
from pa_lab.channel import (AppChannel, AppError, Command, ConsoleLine, DescribeProgram, ErrorKind, GameOpen, Journal,
                            Preflight, SetOption, Snapshot, Start, Stop)

__all__ = ['FileChannel', 'parse_journal', 'render']

REQUEST_PREFIX = 'palab_'
JOURNAL_END = 'end'
JOURNAL_OK_RE = re.compile(r'^ok \d+ (\S+)')
JOURNAL_ERROR_RE = re.compile(r'^error (\d+) \(([^)]*)\): (.*)$')
# Journal lines count the request's lines from 1, and line 1 is `request <id>`.
FIRST_COMMAND_LINE = 2
PROGRAM_IS_RE = re.compile(r'^Program is ([A-Z_]+)\.$')
VERBS: Dict[str, Type[Command]] = {'start': Start, 'stop': Stop, 'snapshot': Snapshot,
                                   'describe_program': DescribeProgram, 'game_open': GameOpen, 'set': SetOption,
                                   'preflight': Preflight}


def _json(value: Any) -> str:
    """Canonical one-line JSON."""
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False)


def render(command: Command) -> str:
    """One command's request line."""
    if isinstance(command, Start):
        return 'start {}'.format(command.controller_wait_s)
    if isinstance(command, Stop):
        return 'stop'
    if isinstance(command, Snapshot):
        return 'snapshot {} {}'.format(command.label, command.timeout_ms)
    if isinstance(command, DescribeProgram):
        return 'describe_program'
    if isinstance(command, GameOpen):
        return 'game_open'
    if isinstance(command, SetOption):
        return 'set {} {}'.format(command.key, _json(command.value))
    if isinstance(command, Preflight):
        return 'preflight ' + _json(command.options)
    if isinstance(command, ConsoleLine):
        return command.line
    raise TypeError('no request line for {!r}'.format(command))


def _error(message: str, line: int) -> AppError:
    """LabChannel's error message, on request line `line`, as a kind."""
    index = line - FIRST_COMMAND_LINE if line >= FIRST_COMMAND_LINE else None
    if message.startswith('owned by'):
        return AppError(ErrorKind.OWNER_CLAIMED, message, command_index=index)
    if 'not a program' in message:
        return AppError(ErrorKind.PANEL_NOT_PROGRAM, message, command_index=index)
    if 'controller not connected' in message:
        return AppError(ErrorKind.CONTROLLER_NOT_READY, message, command_index=index)
    match = PROGRAM_IS_RE.match(message)
    if match:
        return AppError(ErrorKind.PROGRAM_STATE, message, program_state=match.group(1), command_index=index)
    return AppError(ErrorKind.OTHER, message, command_index=index)


def parse_journal(lines: Tuple[str, ...]) -> Journal:
    """A finished journal's lines (the last is `end`) as a Journal."""
    succeeded = set()
    error = None
    printed = []
    for line in lines[:-1]:
        ok = JOURNAL_OK_RE.match(line)
        failed = JOURNAL_ERROR_RE.match(line)
        if ok:
            if ok.group(1) in VERBS:
                succeeded.add(VERBS[ok.group(1)])
        elif failed:
            if error is None:
                error = _error(failed.group(3), int(failed.group(1)))
        else:
            printed.append(line)
    return Journal(lines=tuple(lines), succeeded=frozenset(succeeded), error=error, printed=tuple(printed))


class FileChannel(AppChannel):
    """The channel of one profile's `Lab/` folder."""

    state_label = 'Lab/state.json'
    save_label = 'Lab/save.json'

    def __init__(self, lab_dir: Path):
        self.lab_dir = Path(lab_dir)

    def __repr__(self) -> str:
        return 'FileChannel({!r})'.format(str(self.lab_dir))

    def _inbox(self, name: str) -> Path:
        return self.lab_dir / 'inbox' / (name + '.txt')

    def _journal(self, name: str) -> Path:
        return self.lab_dir / 'outbox' / (name + '.out')

    def request_name(self, label: str, request_id: str, unique: bool = False) -> str:
        name = REQUEST_PREFIX + label + '_' + request_id.replace('-', '')
        return name + '_' + secrets.token_hex(4) if unique else name

    def send(self, name: str, request_id: str, commands: Tuple[Command, ...]) -> None:
        # Written beside the inbox and renamed in: the app never reads half a request.
        part = self.lab_dir / 'outbox' / (name + '.part')
        lines = ['request {}'.format(request_id)] + [render(c) for c in commands]
        try:
            part.write_text('\n'.join(lines) + '\n', encoding='utf-8')
            os.replace(part, self._inbox(name))
        except OSError:
            part.unlink(missing_ok=True)
            raise

    def answer(self, name: str) -> Optional[Journal]:
        try:
            lines = self._journal(name).read_text(encoding='utf-8', errors='replace').splitlines()
        except FileNotFoundError:
            return None
        if not lines or lines[-1] != JOURNAL_END:  # the app may still be writing it
            return None
        return parse_journal(tuple(lines))

    def taken(self, name: str) -> bool:
        # LabChannel moves the request to done/ before it runs anything.
        return (self.lab_dir / 'done' / (name + '.txt')).exists() or self._journal(name).exists()

    def withdraw(self, name: str) -> bool:
        try:
            os.remove(self._inbox(name))
        except FileNotFoundError:
            return False
        return True

    @staticmethod
    def _read(path: Path, limit: int) -> Optional[bytes]:
        try:
            with open(path, 'rb') as f:
                return f.read(limit + 1)
        except FileNotFoundError:
            return None

    def read_hold(self) -> Optional[Dict[str, str]]:
        return hold.read_hold(self.lab_dir)

    def read_state(self, limit: int) -> Optional[bytes]:
        return self._read(self.lab_dir / 'state.json', limit)

    def read_save(self, limit: int) -> Optional[bytes]:
        return self._read(self.lab_dir / 'save.json', limit)

    def snapshot_path(self, name: str, label: str) -> str:
        return str(self.lab_dir / 'outbox' / '{}_{}.png'.format(name, label))
