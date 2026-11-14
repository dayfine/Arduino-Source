"""`shot`: the console's first frame after the call starts (Slice 2 of fork #131).

The app's channel command `snapshot LABEL TIMEOUT_MS` (#481, Lab.md "Snapshot") waits for the
first video frame that arrives after the line starts, saves it and names it with the same
`instance.id` and `frame_seq` that `Lab/state.json` uses. It needs no owner token, program state
or controller. So:

1. Read `Lab/state.json` at call start: the app must be up (fresh heartbeat), and its
   `instance.id` is the one the reply must name.
2. Write the request `request <uuid>` / `snapshot frame <timeout_ms>` into `Lab/inbox` at once.
3. Read the journal: `snapshot saved ...` gives the exact `frame_seq` and `captured_at`;
   `snapshot timeout` and `snapshot unavailable` map to refusals.
4. Open only the PNG this call asked for, never a path read from the journal, check it is
   complete, and return it.

The request is fixed text; no caller input but the checked `timeout_ms` reaches the inbox.
"""

from __future__ import annotations

import os
import re
import time
import uuid
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from pa_lab import timestamps
from pa_lab.lab_reader import BAD_SIGNALS, ConsoleReader, Unavailable
from pa_lab.tools import InvalidArguments, check_arguments

DEFAULT_MAX_AGE_MS = 2000
DEFAULT_TIMEOUT_MS = 5000
# The app reads its inbox every 500 ms and its own wait starts when the line does; the journal
# may come this long after the caller's timeout.
ANSWER_GRACE_MS = 2000
POLL_S = 0.05
MAX_PNG_BYTES = 32 << 20
LABEL = 'frame'
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'
PNG_END = b'\x00\x00\x00\x00IEND\xaeB`\x82'
JOURNAL_ERROR_RE = re.compile(r'^error \d+ \([^)]*\): (.*)$')
JOURNAL_OK_RE = re.compile(r'^ok \d+ snapshot ' + LABEL + r'\b')
SAVED_RE = re.compile(r'^snapshot saved instance=(\S+) frame_seq=(\d+) captured_at=(\S+) after=(\d+) path=(.+)$')
TIMEOUT_RE = re.compile(r'^snapshot timeout instance=(\S+) after=(\d+) timeout_ms=(\d+)$')
UNAVAILABLE_RE = re.compile(r'^snapshot unavailable instance=(\S+) reason=(\S+)$')
# `snapshot unavailable` reasons the app has no frame for; the rest mean the app could not serve.
APP_REASONS = ('panel_changed', 'app_closing', 'save_failed')

__all__ = ['take_shot', 'Refused', 'InvalidArguments']


class Refused(Exception):
    def __init__(self, code: str, message: str, evidence: Optional[Dict[str, Any]] = None):
        super().__init__(message)
        self.refusal: Dict[str, Any] = {'code': code, 'message': message}
        if evidence is not None:
            self.refusal['evidence'] = evidence


def _check_start(app: Dict[str, Any]) -> None:
    signal = app['video']['signal']
    if signal in BAD_SIGNALS:
        raise Refused('video_bad_signal', 'The video shows {}.'.format(signal.replace('_', ' ')))


def _write_request(lab_dir: Path, name: str, request_id: str, timeout_ms: int) -> Path:
    part = lab_dir / 'outbox' / (name + '.part')
    inbox = lab_dir / 'inbox' / (name + '.txt')
    part.write_text('request {}\nsnapshot {} {}\n'.format(request_id, LABEL, timeout_ms), encoding='utf-8')
    os.replace(part, inbox)
    return inbox


def _withdraw(inbox: Path) -> None:
    try:
        os.remove(inbox)
    except FileNotFoundError:
        pass


def _wait_for_journal(console: ConsoleReader, sleep: Callable[[float], None], deadline: float,
                      lab_dir: Path, name: str, inbox: Path) -> List[str]:
    journal = lab_dir / 'outbox' / (name + '.out')
    while True:
        if journal.exists():
            return journal.read_text(encoding='utf-8', errors='replace').splitlines()
        if console.now() >= deadline:
            break
        sleep(POLL_S)
    _withdraw(inbox)
    raise Refused('timeout', 'The app did not answer the snapshot request in time (busy with another '
                  'request?).', {'request': name})


def _check_instance(reply_instance: str, instance_id: str) -> None:
    if reply_instance != instance_id:
        raise Refused('app_unavailable', 'The app restarted during the call; its frames are not '
                      'comparable with the old instance.', {
                          'reason': 'instance_replaced', 'instance_id': instance_id,
                          'new_instance_id': reply_instance})


def _refuse_failed_reply(lines: List[str], instance_id: str) -> None:
    """Raises the refusal for a `snapshot timeout` or `snapshot unavailable` line, if any."""
    for line in lines:
        match = TIMEOUT_RE.match(line)
        if match:
            _check_instance(match.group(1), instance_id)
            after = int(match.group(2))
            raise Refused('timeout', 'No frame newer than seq {} within {} ms.'.format(after, match.group(3)),
                          {'instance_id': instance_id, 'frame_seq': after})
        match = UNAVAILABLE_RE.match(line)
        if match:
            _check_instance(match.group(1), instance_id)
            reason = match.group(2)
            if reason == 'no_video':
                raise Refused('video_stale', 'The selected panel has no video.',
                              {'instance_id': instance_id, 'reason': reason})
            if reason in APP_REASONS:
                raise Refused('app_unavailable', 'The app could not save a frame ({}).'.format(
                    reason.replace('_', ' ')), {'instance_id': instance_id, 'reason': reason})
            raise Refused('app_unavailable', 'The app gave an unknown snapshot reason {!r}.'.format(reason),
                          {'instance_id': instance_id, 'reason': 'unknown', 'journal': lines})


def _parse_saved(lines: List[str], instance_id: str, expected: Path) -> Dict[str, Any]:
    _refuse_failed_reply(lines, instance_id)
    for line in lines:
        match = JOURNAL_ERROR_RE.match(line)
        if match:
            raise Refused('app_unavailable', "The app's snapshot line failed: {}".format(match.group(1)),
                          {'journal': lines})
    saved = [m for m in (SAVED_RE.match(line) for line in lines) if m]
    if len(saved) != 1 or not any(JOURNAL_OK_RE.match(line) for line in lines):
        raise Refused('app_unavailable', 'The snapshot journal has no single saved line and ok line.',
                      {'journal': lines})
    match = saved[0]
    _check_instance(match.group(1), instance_id)
    frame_seq, after = int(match.group(2)), int(match.group(4))
    try:
        timestamps.parse(match.group(3))
    except ValueError:
        raise Refused('app_unavailable', 'The snapshot names a bad capture time.', {'journal': lines})
    # The journal's path is compared by name only; the file opened is the one this call asked for.
    if frame_seq <= after or Path(match.group(5)).name != expected.name:
        raise Refused('app_unavailable', 'The snapshot line does not match this request.', {'journal': lines})
    return {'frame_seq': frame_seq, 'captured_at': match.group(3)}


def _read_png(path: Path) -> bytes:
    try:
        with open(path, 'rb') as f:
            data = f.read(MAX_PNG_BYTES + 1)
    except OSError as e:
        raise Refused('app_unavailable', 'The saved snapshot cannot be read: {}.'.format(e.strerror))
    if len(data) > MAX_PNG_BYTES or not data.startswith(PNG_SIGNATURE) or not data.endswith(PNG_END):
        raise Refused('app_unavailable', 'The saved snapshot is not a complete PNG ({} bytes).'.format(len(data)))
    return data


def take_shot(console: ConsoleReader, arguments: Dict[str, Any],
              sleep: Callable[[float], None] = time.sleep,
              new_uuid: Callable[[], uuid.UUID] = uuid.uuid4) -> Dict[str, Any]:
    """Raises InvalidArguments or Refused (with a contract refusal object)."""
    args = check_arguments('shot', arguments)
    max_age_ms = args.get('max_age_ms', DEFAULT_MAX_AGE_MS)
    timeout_ms = args.get('timeout_ms', DEFAULT_TIMEOUT_MS)
    started = console.now()
    try:
        app = console.app_state()
    except Unavailable as e:
        raise Refused(e.refusal()['code'], e.message, e.evidence)
    _check_start(app)
    instance_id = app['instance']['id']

    request_id = str(new_uuid())
    name = 'palab_shot_' + request_id.replace('-', '')
    inbox = _write_request(console.lab_dir, name, request_id, timeout_ms)
    deadline = started + (timeout_ms + ANSWER_GRACE_MS) / 1000.0
    lines = _wait_for_journal(console, sleep, deadline, console.lab_dir, name, inbox)
    png_path = console.lab_dir / 'outbox' / '{}_{}.png'.format(name, LABEL)
    frame = _parse_saved(lines, instance_id, png_path)
    age_ms = int(round((console.now() - timestamps.parse(frame['captured_at'])) * 1000))
    if age_ms > max_age_ms:
        raise Refused('video_stale', 'The saved frame is {} ms old (max_age_ms {}).'.format(age_ms, max_age_ms),
                      dict(frame, instance_id=instance_id))
    return {
        'console': console.console,
        'path': str(png_path),
        'png': _read_png(png_path),
        'instance_id': instance_id,
        'frame_seq': frame['frame_seq'],
        'captured_at': frame['captured_at'],
        'call_started_at': timestamps.format(started),
        'request': name,
    }
