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

`cfr_batch(console, request_id, lease_token, lines)` (5b) sends console lines (presses, waits,
screen reads) that `pa_lab/cfr.py` allows, and waits for their journal:

1. As above, the app's state is read and applied first.
2. `begin_action` checks the lease; the batch is refused (stored) unless every line is allowed
   and its worst case fits in `cfr.BATCH_MAX_MS`, the program is `stopped` on a program panel,
   the controller is `ready` and the video `live`.
3. `console://<id>/save` is marked stale (`cfr_batch`) before anything is sent: from here on the
   game may differ from the last save fact, even if the batch never runs.
4. Send `owner T` / `request <request_id>` / `owner claim T` / the lines, and wait for the journal
   for the batch's worst case plus the answer time:
   - all `ok`: completed, `outcome: done`, with the journal;
   - a batch line failed (an `expect` timed out, a press refused): completed,
     `outcome: line_failed`, `failed_line` (1-based, as in `lines`), `error` and the journal: the
     batch ran up to that line, which is an answer, not a server fault;
   - the owner lines failed: failed, `app_unavailable`, nothing ran;
   - no journal in time: `refused`, `timeout` if the request was still in the inbox (withdrawn,
     nothing ran); otherwise `failed`, `timeout`: the batch may have run in part.
5. An exact retry sends nothing; if the stored ticket is still `pending` (the daemon stopped while
   waiting), the retry collects the journal the same way. An app restart fails a pending batch
   (`observe`).
"""

from __future__ import annotations

import os
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

import re

from pa_lab import cfr, control, shot, timestamps
from pa_lab.lab_reader import ConsoleReader, Unavailable

STOP_ANSWER_MS = 5000
JOURNAL_POLL_S = 0.05

__all__ = ['stop_program', 'cfr_batch', 'ACTIONS', 'request_text']

JOURNAL_ERROR_RE = re.compile(r'^error ([0-9]+) ')
OWNER_LINES = 3  # owner, request, owner claim: a journal line number above this is a batch line


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


def _batch_refusal(app: Dict[str, Any]) -> Optional[control.Refused]:
    """Why a batch cannot run on the app as it is now, or None. Stricter than the app, which only
    refuses presses while a program runs: a batch needs a stopped program panel, a ready
    controller and live video (so its shots and detectors see the console)."""
    state = app['program']['state']
    panel = app['panel']['id']
    if state == 'no_program':
        return control.Refused('panel_not_program', 'The selected panel is not a program; a batch needs its console.')
    if state in ('running', 'stopping'):
        return control.Refused('program_running', '{} is {}; stop it first.'.format(panel, state))
    if state != 'stopped':
        return control.Refused('program_not_stopped', '{} is {}.'.format(panel, state.replace('_', ' ')))
    if app['controller']['state'] != 'ready':
        return control.Refused('controller_not_ready', 'The controller is {}.'.format(
            app['controller']['state'].replace('_', ' ')))
    if app['video']['state'] != 'live':
        return control.Refused('video_stale', 'The video is {}.'.format(app['video']['state']))
    return None


def _batch_name(request_id: str) -> str:
    return 'palab_cfr_batch_{}'.format(request_id.replace('-', ''))


def _batch_result(plane: control.ControlPlane, client_id: str, request_id: str, instance_id: str,
                  lines: List[str]) -> Dict[str, Any]:
    errors = [JOURNAL_ERROR_RE.match(line) for line in lines]
    failed = next(((m, line) for m, line in zip(errors, lines) if m is not None), None)
    if failed is not None and int(failed[0].group(1)) <= OWNER_LINES:
        return plane.resolve_ticket(client_id, 'cfr_batch', request_id, refusal={
            'code': 'app_unavailable', 'message': 'The app refused the batch: {}'.format(failed[1]),
            'evidence': {'journal': lines}})
    result = {'outcome': 'done', 'instance_id': instance_id, 'journal': lines}
    if failed is not None:
        result.update(outcome='line_failed', failed_line=int(failed[0].group(1)) - OWNER_LINES, error=failed[1])
    return plane.resolve_ticket(client_id, 'cfr_batch', request_id, result=result)


def _collect(plane: control.ControlPlane, reader: ConsoleReader, client_id: str, request_id: str,
             instance_id: str, deadline: float, sleep: Callable[[float], None]) -> Dict[str, Any]:
    name = _batch_name(request_id)
    lines = _journal(reader, sleep, deadline, name)
    if lines is not None:
        return _batch_result(plane, client_id, request_id, instance_id, lines)
    if _withdrawn(reader.lab_dir / 'inbox' / (name + '.txt')):
        return plane.resolve_ticket(client_id, 'cfr_batch', request_id, refusal={
            'code': 'timeout', 'message': 'The app did not take the batch in time; nothing was sent.',
            'evidence': {'request': name}}, acted=False)
    return plane.resolve_ticket(client_id, 'cfr_batch', request_id, refusal={
        'code': 'timeout', 'message': 'The app took the batch but wrote no journal in time; it may have run in part.',
        'evidence': {'request': name}})


def cfr_batch(plane: control.ControlPlane, reader: ConsoleReader, client_id: str, request: Dict[str, Any],
              owner: str, sleep: Callable[[float], None] = time.sleep) -> Dict[str, Any]:
    """Runs one cfr_batch request; returns its request_record (completed, refused or failed; pending
    only when the daemon is stopped while waiting). Raises control.Refused as stop_program does."""
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
        cfr.check(request['arguments']['lines'])
        refused = _batch_refusal(app)
        if refused is not None:
            raise refused
        return None

    record, send = plane.begin_action(client_id, request, None if app is None else app['instance']['id'], decide)
    if record['status'] != 'pending':
        return record
    request_id = record['request_id']
    budget_s = (cfr.check(record['arguments']['lines']) + STOP_ANSWER_MS + shot.ANSWER_GRACE_MS) / 1000.0
    if not send:
        deadline = timestamps.parse(record['created_at']) + budget_s
        return _collect(plane, reader, client_id, request_id, app['instance']['id'] if app else None,
                        deadline, sleep)
    plane.mark_save_stale(reader.console, 'cfr_batch')
    _send(reader.lab_dir, _batch_name(request_id), request_text(owner, request_id, record['arguments']['lines']))
    return _collect(plane, reader, client_id, request_id, app['instance']['id'], started + budget_s, sleep)


ACTIONS = {'stop_program': stop_program, 'cfr_batch': cfr_batch}
