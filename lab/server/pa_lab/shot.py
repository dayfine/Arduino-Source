"""`shot`: a console screenshot strictly newer than the call (Slice 2 of fork #131).

What the app offers today (#481, lab/observation-seam) is the channel's read-only `shot LABEL`
line: it needs no owner token, but it runs only on a console program panel with a ready
controller, and its journal does not name the frame it saved. So the newer-than-call-start rule
is kept this way:

1. Read `(instance.id, frame_seq)` from `Lab/state.json` at call start.
2. Wait until state.json shows the same instance with a strictly greater `frame_seq` (a new
   instance is refused: its sequence restarted). That frame arrived before step 3.
3. Only then write the request `request <uuid>` / `shot frame` into `Lab/inbox`; the app saves
   its current frame, which is that frame or a later one.
4. Read the journal, check the PNG is complete, and return it with `frame_seq_at_least`.

The app's frame token for the exact PNG, and a snapshot without a ready controller, wait for
#131 Slice 1's snapshot command. The request is fixed text; no caller input reaches the inbox.
"""

from __future__ import annotations

import os
import re
import time
import uuid
from pathlib import Path
from typing import Any, Callable, Dict, Optional, Tuple

from pa_lab import timestamps
from pa_lab.lab_reader import BAD_SIGNALS, ConsoleReader, Unavailable
from pa_lab.tools import InvalidArguments, check_arguments

DEFAULT_MAX_AGE_MS = 2000
DEFAULT_TIMEOUT_MS = 5000
POLL_S = 0.05
MAX_PNG_BYTES = 32 << 20
LABEL = 'frame'
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'
PNG_END = b'\x00\x00\x00\x00IEND\xaeB`\x82'
JOURNAL_ERROR_RE = re.compile(r'^error \d+ \([^)]*\): (.*)$')
JOURNAL_OK_RE = re.compile(r'^ok \d+ shot ' + LABEL + r'\b')

__all__ = ['take_shot', 'Refused', 'InvalidArguments']


class Refused(Exception):
    def __init__(self, code: str, message: str, evidence: Optional[Dict[str, Any]] = None):
        super().__init__(message)
        self.refusal: Dict[str, Any] = {'code': code, 'message': message}
        if evidence is not None:
            self.refusal['evidence'] = evidence


def _check_start(app: Dict[str, Any]) -> None:
    video = app['video']
    if video['state'] == 'missing' or video['frame_seq'] is None:
        raise Refused('video_stale', 'The app has no video frame (video.state {}).'.format(video['state']))
    if video['signal'] in BAD_SIGNALS:
        raise Refused('video_bad_signal', 'The video shows {}.'.format(video['signal'].replace('_', ' ')))
    if app['panel']['kind'] != 'program':
        raise Refused('panel_not_program', "The selected panel is not a program; the app's shot line "
                      'needs a console program panel (no snapshot command yet).')
    if app['controller']['state'] != 'ready':
        raise Refused('controller_not_ready', "The controller is {}; the app's shot line needs a ready "
                      'controller (no snapshot command yet).'.format(app['controller']['state'].replace('_', ' ')))


class _Call:
    def __init__(self, console: ConsoleReader, timeout_ms: int, sleep: Callable[[float], None]):
        self.console = console
        self.sleep = sleep
        self.started = console.now()
        self.deadline = self.started + timeout_ms / 1000.0
        self.timeout_ms = timeout_ms

    def expired(self) -> bool:
        return self.console.now() >= self.deadline

    def app_or_none(self) -> Tuple[Optional[Dict[str, Any]], Optional[Unavailable]]:
        try:
            return self.console.app_state(), None
        except Unavailable as e:
            return None, e


def _wait_for_newer_frame(call: _Call, instance_id: str, start_seq: int) -> Dict[str, Any]:
    last_error = None
    last_app = None
    while True:
        app, error = call.app_or_none()
        if app is not None:
            last_app = app
            if app['instance']['id'] != instance_id:
                raise Refused('app_unavailable', 'The app restarted during the call; its frames are not '
                              'comparable with the old instance.', {
                                  'reason': 'instance_replaced', 'instance_id': instance_id,
                                  'new_instance_id': app['instance']['id']})
            seq = app['video']['frame_seq']
            if seq is not None and seq > start_seq:
                return app
        else:
            last_error = error
        if call.expired():
            break
        call.sleep(POLL_S)
    if last_app is None and last_error is not None:
        raise Refused(last_error.refusal()['code'], last_error.message, last_error.evidence)
    raise Refused('timeout', 'No frame newer than seq {} within {} ms.'.format(start_seq, call.timeout_ms),
                  {'instance_id': instance_id, 'frame_seq': start_seq})


def _write_request(lab_dir: Path, name: str, request_id: str) -> Path:
    part = lab_dir / 'outbox' / (name + '.part')
    inbox = lab_dir / 'inbox' / (name + '.txt')
    part.write_text('request {}\nshot {}\n'.format(request_id, LABEL), encoding='utf-8')
    os.replace(part, inbox)
    return inbox


def _withdraw(inbox: Path) -> None:
    try:
        os.remove(inbox)
    except FileNotFoundError:
        pass


def _wait_for_journal(call: _Call, lab_dir: Path, name: str, inbox: Path) -> str:
    journal = lab_dir / 'outbox' / (name + '.out')
    while True:
        if journal.exists():
            return journal.read_text(encoding='utf-8', errors='replace')
        if call.expired():
            break
        call.sleep(POLL_S)
    _withdraw(inbox)
    raise Refused('timeout', 'The app did not answer the shot request within {} ms (busy with another '
                  'request?).'.format(call.timeout_ms), {'request': name})


def _read_png(journal: str, expected: Path) -> bytes:
    lines = journal.splitlines()
    for line in lines:
        match = JOURNAL_ERROR_RE.match(line)
        if match:
            raise Refused('app_unavailable', "The app's shot line failed: {}".format(match.group(1)),
                          {'journal': lines})
    if not any(JOURNAL_OK_RE.match(line) for line in lines):
        raise Refused('app_unavailable', 'The shot journal has no ok line.', {'journal': lines})
    printed = [line[len('shot '):] for line in lines if line.startswith('shot ')]
    if printed == ['(no video frame)']:
        raise Refused('video_stale', 'The app had no video frame to save.', {'journal': lines})
    # Only the file this call asked for is opened, never a path read from the journal.
    if len(printed) != 1 or Path(printed[0]).name != expected.name:
        raise Refused('app_unavailable', 'The shot journal names {!r}, not {}.'.format(printed, expected.name),
                      {'journal': lines})
    try:
        with open(expected, 'rb') as f:
            data = f.read(MAX_PNG_BYTES + 1)
    except OSError as e:
        raise Refused('app_unavailable', 'The saved shot cannot be read: {}.'.format(e.strerror))
    if len(data) > MAX_PNG_BYTES or not data.startswith(PNG_SIGNATURE) or not data.endswith(PNG_END):
        raise Refused('app_unavailable', 'The saved shot is not a complete PNG ({} bytes).'.format(len(data)))
    return data


def take_shot(console: ConsoleReader, arguments: Dict[str, Any],
              sleep: Callable[[float], None] = time.sleep,
              new_uuid: Callable[[], uuid.UUID] = uuid.uuid4) -> Dict[str, Any]:
    """Raises InvalidArguments or Refused (with a contract refusal object)."""
    args = check_arguments('shot', arguments)
    max_age_ms = args.get('max_age_ms', DEFAULT_MAX_AGE_MS)
    call = _Call(console, args.get('timeout_ms', DEFAULT_TIMEOUT_MS), sleep)
    try:
        app = console.app_state()
    except Unavailable as e:
        raise Refused(e.refusal()['code'], e.message, e.evidence)
    _check_start(app)
    instance_id = app['instance']['id']
    start_seq = app['video']['frame_seq']

    newer = _wait_for_newer_frame(call, instance_id, start_seq)
    captured_at = newer['video']['captured_at']
    age_ms = int(round((console.now() - timestamps.parse(captured_at)) * 1000))
    if age_ms > max_age_ms:
        raise Refused('video_stale', 'The newest frame is {} ms old (max_age_ms {}).'.format(age_ms, max_age_ms),
                      {'instance_id': instance_id, 'frame_seq': newer['video']['frame_seq'],
                       'captured_at': captured_at})

    request_id = str(new_uuid())
    name = 'palab_shot_' + request_id.replace('-', '')
    inbox = _write_request(console.lab_dir, name, request_id)
    journal = _wait_for_journal(call, console.lab_dir, name, inbox)
    png_path = console.lab_dir / 'outbox' / '{}_{}.png'.format(name, LABEL)
    png = _read_png(journal, png_path)
    return {
        'console': console.console,
        'path': str(png_path),
        'png': png,
        'instance_id': instance_id,
        'frame_seq_at_least': newer['video']['frame_seq'],
        'captured_at_at_least': captured_at,
        'call_started_at': timestamps.format(call.started),
        'request': name,
    }
