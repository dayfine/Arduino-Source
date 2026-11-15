"""Leased actions (Slice 5 of fork #131). First: `stop_program`.

A leased action is a long operation: the tool stores a ticket, sends one fixed request into the
app's channel and returns; the app's own report in `Lab/state.json` completes it. Agents do not
park on the call: they read `request_status` or wait in `next_events` for the ticket's
`request_completed` / `request_failed` event.

`stop_program(console, request_id, lease_token)`:

1. Read `Lab/state.json` (the app is up; its instance) and apply it (`observe`), so a UI click
   that already happened takes the console before this call is decided.
2. `ControlPlane.begin_action`: the lease must be held by the token; a program that is not
   `running` completes at once (`outcome: not_running`); otherwise a `pending` ticket bound to
   that app instance. An exact retry returns the stored record and sends nothing.
3. Send `owner T` / `request <request_id>` / `owner claim T` / `stop` (Lab.md "Requests",
   "Owner"). `T` is the console's channel owner from the config; the app names `request_id` in
   `program.last_transition` and keeps it from `stopping` to `stopped`.
4. Wait for the journal (the line itself is immediate; the program stops in the background):
   - `ok`: read the state again; the ticket completes when the app reports `stopped` with this
     request id, possibly already in this read (a terminal event before the response);
   - a failed line: if the program is no longer running the stop is moot (`not_running`),
     otherwise the ticket fails with the app's error;
   - no journal in time: if the request is still in the inbox it is withdrawn and the ticket is
     `refused` with `timeout` (nothing was sent); if the app took it, the ticket stays pending
     and resolves from the state as above.

No caller string reaches the inbox: the request id is a checked UUID and the rest is fixed text.
"""

from __future__ import annotations

import os
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from pa_lab import control, shot
from pa_lab.lab_reader import ConsoleReader, Unavailable

STOP_ANSWER_MS = 5000
JOURNAL_POLL_S = 0.05

__all__ = ['stop_program', 'request_text']


def request_text(owner: str, request_id: str, commands: List[str]) -> str:
    """A channel request that claims the channel for `owner` and runs `commands` under it.

    `owner T` must be the first line and `request <uuid>` follows (Lab.md "Requests");
    `owner claim T` succeeds when the channel is free or already T's.
    """
    return ''.join('{}\n'.format(line) for line in
                   ['owner ' + owner, 'request ' + request_id, 'owner claim ' + owner] + commands)


def _send(lab_dir: Path, name: str, text: str) -> Path:
    part = lab_dir / 'outbox' / (name + '.part')
    inbox = lab_dir / 'inbox' / (name + '.txt')
    part.write_text(text, encoding='utf-8')
    os.replace(part, inbox)
    return inbox


def _journal(reader: ConsoleReader, sleep: Callable[[float], None], deadline: float, name: str) -> Optional[List[str]]:
    path = reader.lab_dir / 'outbox' / (name + '.out')
    while True:
        if path.exists():
            lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
            if lines and lines[-1] == 'end':
                return lines
        if reader.now() >= deadline:
            return None
        sleep(JOURNAL_POLL_S)


def _withdrawn(inbox: Path) -> bool:
    """True when the request was still in the inbox and is now gone: the app never saw it."""
    try:
        os.remove(inbox)
        return True
    except FileNotFoundError:
        return False


def _not_running(app: Dict[str, Any]) -> Dict[str, Any]:
    program = app['program']
    return {'outcome': 'not_running', 'instance_id': app['instance']['id'], 'run_id': program['run_id'],
            'program_state': program['state']}


def stop_program(plane: control.ControlPlane, reader: ConsoleReader, client_id: str, request: Dict[str, Any],
                 owner: str, sleep: Callable[[float], None] = time.sleep) -> Dict[str, Any]:
    """Runs one stop_program request; returns its request_record (pending, completed, refused or failed).

    Raises control.Refused for a request that cannot be keyed (bad console, request id or
    arguments, or an id reused with other arguments); those are not stored.
    """
    started = reader.now()
    try:
        app = reader.app_state()
    except Unavailable as e:
        app, unavailable = None, e
    else:
        plane.observe(reader.console, app)

    def decide(lease_row) -> Optional[Dict[str, Any]]:
        if app is None:
            refusal = unavailable.refusal()
            raise control.Refused(refusal.pop('code'), refusal.pop('message'), **refusal)
        if app['program']['state'] != 'running':
            return _not_running(app)
        return None

    record, send = plane.begin_action(client_id, request, None if app is None else app['instance']['id'], decide)
    if not send:
        return record
    request_id = record['request_id']
    name = 'palab_stop_program_{}'.format(request_id.replace('-', ''))
    inbox = _send(reader.lab_dir, name, request_text(owner, request_id, ['stop']))
    lines = _journal(reader, sleep, started + (STOP_ANSWER_MS + shot.ANSWER_GRACE_MS) / 1000.0, name)
    if lines is None:
        if _withdrawn(inbox):
            return plane.resolve_ticket(client_id, 'stop_program', request_id, refusal={
                'code': 'timeout', 'message': 'The app did not take the stop request in time; nothing was sent.',
                'evidence': {'request': name}}, acted=False)
        return plane.record(client_id, 'stop_program', request_id)
    errors = [line for line in lines if line.startswith('error ')]
    if errors:
        try:
            now = reader.app_state()
        except Unavailable:
            now = None
        if now is not None and now['instance']['id'] == app['instance']['id'] and \
                now['program']['state'] != 'running':
            reader.console_state()
            return plane.resolve_ticket(client_id, 'stop_program', request_id, result=_not_running(now))
        return plane.resolve_ticket(client_id, 'stop_program', request_id, refusal={
            'code': 'app_unavailable', 'message': "The app refused the stop: {}".format(errors[0]),
            'evidence': {'journal': lines}})
    reader.console_state()  # applies observe: the stop may already be reported
    return plane.record(client_id, 'stop_program', request_id)
