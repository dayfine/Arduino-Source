"""`run_program` and `stop_program`: leased actions on the console (Slice 5a of fork #131).

Each call is one request record in the control plane (`pa_lab.control`), keyed by
`(client_id, tool, request_id)`, so a retried call never sends a second start or stop:

1. Read `console://B/state` (that applies any UI takeover, run end or app restart first).
2. `ControlPlane.begin_action`, one transaction: an exact retry gets its stored record; otherwise
   the request's lease token must hold the console and the checks below must pass, or the refusal
   is stored. A passing request is stored `pending` with the app instance it was checked against.
3. Send `Start` (or `Stop`) through the console's app channel (`pa_lab.channel`; the version-1
   wire format is `contracts/v1/channel.md`). The app records the request id in
   `program.last_transition`.
4. Settle: read the journal and the app's state until the outcome is known, then resolve the
   record (`completed` or `failed`) and emit its `request_completed` / `request_failed` event.

How an outcome is recognised, from what Slice 1 writes (`last_transition` holds only the newest
start or stop):

- **By request id.** `last_transition` is `{action, source: lab, request_id: ours}`: our start
  (the lease enters `agent_running`) or our stop (completed once `program.state` is `stopped`).
- **By time** (the app does not keep the start once a stop replaces it, and `run_id` is still
  null). After our start's `ok` line, a stop by `program` or `error` dated after our request means
  the run started and already ended: `completed` with `ended` set, `correlated_by: time`, and the
  lease stays idle. For `stop_program` the same stop is the terminal event that completes it.
- A stop by a lab script (source `lab`, another request id) dated after our request counts the same
  way: the run started and was stopped before we read the answer.
- A UI transition dated after our request is the user taking over: `failed`,
  `console_held_by_user` (the control plane has already installed the user lease), with
  `evidence.started` (the start's `ok` line was read: the run did start, and `evidence.run_id` is
  its id) so a caller resuming later knows the game state moved.
- A new `instance.id` is an app restart: `failed`, `app_unavailable` / `instance_replaced`,
  unless the new instance shows our request id.

A request the app has not answered in time stays `pending` (a ticket) if the app took it from the
inbox (`AppChannel.taken`), since it may still act; it is settled on the next `request_status`,
`next_events`, `console://B/state` read or action call. The first time the daemon sees the request
taken, and its journal finished, it stores the time (`actions.taken_at`, `answered_at`): the channel
may prune taken requests and journals (channel.md, "Retention"), so a request once seen taken stays
taken.

The app writes the finished journal first and `state.json` only on its next 500 ms timer (later
while another request runs), so a journal is judged against `state.json` only once `written_at` is
after the daemon first read the journal. A journal with the start's `ok` line means the program
started: it is never failed for want of a match, and stays pending until `state.json` shows the
start, a later stop, a UI takeover or another app instance. Past the answer wait it is `failed`
with `timeout` when:

- it is still in the inbox (the app never took it): it is withdrawn and nothing was sent;
- it was never seen taken and has no journal: it never reached the app (the inbox write
  failed, or the daemon died between storing the record and writing it), `never_sent`;
- its journal is complete without an error and without the start's `ok`, `state.json` was written
  after it, and nothing above matches it, `outcome_unknown`.

A failed inbox write is answered at once: `failed`, `app_unavailable` / `inbox_write_failed`.
Each attempt has its own inbox name, so an old journal of a reused request id never answers it.

Not in 5a: `preflight_program` (the app has no command that runs a program's prechecks without
starting it, so the program's own prechecks at Start are the only ones run), staging options
(`options` must be `{}`: the panel's current options run; `preflight_token` is required by the v1
contract but not checked until 5b binds it to a preflight), `select`, `cfr_batch`, and claiming the
channel's owner token (a claimed channel is refused).
"""

from __future__ import annotations

import functools
import time
from typing import Any, Callable, Dict, Optional

from pa_lab import control, timestamps
from pa_lab.channel import AppError, Command, ErrorKind, Journal, Start, Stop
from pa_lab.control import Refused
from pa_lab.lab_reader import ConsoleReader

TOOLS = ('run_program', 'stop_program')
# `start SECONDS`: how long the app waits for controller=ready before refusing. The daemon checks
# the controller first, so this only covers a reconnect between the check and the line.
START_CONTROLLER_WAIT_S = 10
# How long a call waits for its answer before returning a pending ticket. The app polls its inbox
# every 500 ms; a stop then runs in the background until the program reaches `stopped`.
ANSWER_WAIT_S = {'run_program': START_CONTROLLER_WAIT_S + 3.0, 'stop_program': 10.0}
POLL_S = 0.1

ENDED_SOURCES = ('program', 'error')

__all__ = ['ProgramActions', 'TOOLS']


def _written_after(app: Dict[str, Any], answered_at: Optional[str]) -> bool:
    """True once state.json was written after the daemon first read the journal: the app renames
    the journal into place first and rewrites state.json on its next timer."""
    return answered_at is not None and timestamps.parse(app['written_at']) > timestamps.parse(answered_at)


def _refusal(code: str, message: str, **extra: Any) -> Dict[str, Any]:
    return dict(code=code, message=message, **extra)


def _app_error(tool: str, error: AppError, journal: Journal) -> Dict[str, Any]:
    """The refusal for a failed start or stop command."""
    if error.kind == ErrorKind.OWNER_CLAIMED:
        # The app's message names the owner token: keep it out of the record.
        return _refusal('console_held_by_agent', "A lab script holds the app channel's owner lock; "
                        'the daemon does not claim it in Slice 5a.', evidence={'reason': 'channel_owned'})
    if error.kind == ErrorKind.PANEL_NOT_PROGRAM:
        return _refusal('panel_not_program', 'The selected panel is not a program.')
    if error.kind == ErrorKind.CONTROLLER_NOT_READY:
        return _refusal('controller_not_ready', 'The app refused the start: {}'.format(error.detail))
    if error.kind == ErrorKind.PROGRAM_STATE and tool == 'run_program':
        code = 'program_running' if error.program_state in ('RUNNING', 'STOPPING') else 'program_not_stopped'
        return _refusal(code, 'The app refused the start: {}'.format(error.detail))
    return _refusal('app_unavailable', 'The app refused the {}: {}'.format(
        'start' if tool == 'run_program' else 'stop', error.detail), evidence={'journal': list(journal.lines)})


def _first_blocked(state: Dict[str, Any]) -> None:
    """Raises the first refusal console://B/state lists for a run (the lease is checked apart)."""
    for entry in state['blocked']:
        if entry['code'] != 'console_held_by_user':
            extra = {k: v for k, v in entry.items() if k not in ('code', 'message')}
            raise Refused(entry['code'], entry['message'], **extra)


def _need_app(state: Dict[str, Any]) -> Dict[str, Any]:
    if state['app'] is None:
        reason = state['availability']['reason']
        message = state['blocked'][0]['message'] if state['blocked'] else 'The app is unavailable.'
        raise Refused('app_unavailable', message, evidence={'reason': reason})
    return state['app']


def _need_free_channel(app: Dict[str, Any]) -> None:
    if (app.get('channel') or {}).get('owner_claimed'):
        raise Refused('console_held_by_agent', "A lab script holds the app channel's owner lock (owner claim); "
                      'the daemon does not claim it in Slice 5a.', evidence={'reason': 'channel_owned'})


def check_run(state: Dict[str, Any], lease: Any, args: Dict[str, Any]) -> None:
    """run_program's checks after the lease, in order. Raises Refused; None means send `start`."""
    app = _need_app(state)
    if args['options']:
        raise Refused('invalid_arguments', "Slice 5a runs the panel's current options; pass options {}. "
                      'Staging options comes with preflight_program (Slice 5b).', field='options')
    if app['panel']['kind'] == 'program' and app['panel']['id'] != args['panel']:
        raise Refused('invalid_arguments', 'The selected panel is {}, not {}; run_program does not change '
                      'the selection.'.format(app['panel']['id'], args['panel']), field='panel',
                      evidence={'selected': app['panel']['id']})
    _first_blocked(state)
    _need_free_channel(app)


def check_stop(state: Dict[str, Any], lease: Any, args: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """stop_program's checks after the lease. Returns the result when nothing runs (no stop is sent)."""
    app = _need_app(state)
    if app['panel']['kind'] != 'program':
        raise Refused('panel_not_program', 'The selected panel is not a program.')
    program = app['program']
    if program['state'] != 'running':
        return {'console': state['console'], 'instance_id': app['instance']['id'], 'sent': False,
                'state': program['state'], 'last_transition': program['last_transition']}
    _need_free_channel(app)
    return None


class ProgramActions:
    """run_program / stop_program over a control plane and the allowlisted consoles' readers."""

    def __init__(self, plane: control.ControlPlane, consoles: Dict[str, ConsoleReader],
                 sleep: Callable[[float], None] = time.sleep):
        self.plane = plane
        self.consoles = consoles
        self.sleep = sleep

    def run_program(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        return self._act(client_id, dict(request, tool='run_program'), check_run,
                         Start(controller_wait_s=START_CONTROLLER_WAIT_S))

    def stop_program(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        return self._act(client_id, dict(request, tool='stop_program'), check_stop, Stop())

    def settle_pending(self, console: str) -> None:
        """Settles every pending action on `console` from what the app shows now (no waiting)."""
        reader = self.consoles.get(console) if isinstance(console, str) else None
        if reader is None:
            return
        for action in self.plane.pending_actions(console):
            self._settle(reader, action, wait_s=0)

    # ---- internals

    def _reader(self, console: Any) -> ConsoleReader:
        reader = self.consoles.get(console) if isinstance(console, str) else None
        if reader is None:
            raise Refused('console_not_allowed', 'Console {!r} is not served; only {}.'.format(
                console, ', '.join(sorted(self.consoles))))
        return reader

    def _act(self, client_id: str, request: Dict[str, Any],
             check: Callable[..., Optional[Dict[str, Any]]], command: Command) -> Dict[str, Any]:
        reader = self._reader(request.get('console'))
        self.settle_pending(reader.console)
        state = reader.console_state()
        instance = state['app']['instance']['id'] if state['app'] is not None else ''
        name = reader.channel.request_name(request['tool'], str(request.get('request_id')), unique=True)
        record, act = self.plane.begin_action(client_id, request, instance, functools.partial(check, state), name)
        tool = request['tool']
        action = self.plane.action(client_id, tool, record['request_id'])
        if not act:
            if record['status'] == 'pending' and action is not None:
                return self._settle(reader, action, wait_s=0)
            return record
        key = (client_id, tool, record['request_id'])
        try:
            reader.channel.send(action['inbox_name'], record['request_id'], (command,))
        except OSError as e:
            return self.plane.finish_action(*key, refusal=_refusal(
                'app_unavailable', 'Could not write the {} request into the app\'s inbox ({}); nothing was '
                'sent.'.format(tool, e.strerror or e), evidence={'reason': 'inbox_write_failed'}))
        return self._settle(reader, action, wait_s=ANSWER_WAIT_S[tool])

    def _settle(self, reader: ConsoleReader, action: Dict[str, Any], wait_s: float) -> Dict[str, Any]:
        key = (action['client_id'], action['tool'], action['request_id'])
        deadline = reader.now() + wait_s
        wait = ANSWER_WAIT_S[action['tool']]
        while True:
            action = self._observe(reader, action)
            late = reader.now() - timestamps.parse(action['written_at']) >= wait
            outcome = self._judge(reader, action, late)
            if outcome is not None:
                return self.plane.finish_action(*key, **outcome)
            if reader.now() >= deadline:
                break
            self.sleep(POLL_S)
        if late:
            if reader.channel.withdraw(action['inbox_name']):
                return self.plane.finish_action(*key, refusal=_refusal(
                    'timeout', 'The app did not take the {} request within {:g} s (busy with another '
                    'request?); it was withdrawn and nothing was sent.'.format(action['tool'], wait),
                    evidence={'request': action['inbox_name']}))
            if action['taken_at'] is None:
                return self.plane.finish_action(*key, refusal=_refusal(
                    'timeout', 'The {} request never reached the app (its inbox write failed or the daemon '
                    'stopped first); nothing was sent.'.format(action['tool']),
                    evidence={'reason': 'never_sent', 'request': action['inbox_name']}))
            state = reader.console_state()
            if state['app'] is None and state['availability']['reason'] == 'app_not_running':
                return self.plane.finish_action(*key, refusal=_refusal(
                    'app_unavailable', 'The app exited before the {} settled.'.format(action['tool']),
                    evidence={'reason': 'app_not_running'}))
        return self.plane.record(*key)

    def _observe(self, reader: ConsoleReader, action: Dict[str, Any]) -> Dict[str, Any]:
        """Stores the first sighting of the app taking and answering the request; returns the action."""
        name = action['inbox_name']
        answered = action['answered_at'] is None and reader.channel.answer(name) is not None
        taken = action['taken_at'] is None and (answered or reader.channel.taken(name))
        if not taken and not answered:
            return action
        stamp = timestamps.format(reader.now())
        return self.plane.mark_action(action['client_id'], action['tool'], action['request_id'],
                                      taken_at=stamp if taken else None, answered_at=stamp if answered else None)

    def _judge(self, reader: ConsoleReader, action: Dict[str, Any], late: bool) -> Optional[Dict[str, Any]]:
        """The outcome the app shows now, as finish_action keyword arguments, or None to wait.
        `late`: the answer wait is over, so a complete journal that nothing matches is final."""
        journal = reader.channel.answer(action['inbox_name'])
        app = reader.console_state()['app']
        if app is None:
            return None
        tool, request_id = action['tool'], action['request_id']
        program = app['program']
        transition = program['last_transition']
        instance = app['instance']['id']
        ours = transition is not None and transition['source'] == 'lab' and transition['request_id'] == request_id
        after = transition is not None and transition['at'] >= action['written_at']
        base = {'console': reader.console, 'instance_id': instance}
        if tool == 'run_program':
            if ours and transition['action'] == 'start':
                result = dict(base, started_at=transition['at'], run_id=program['run_id'], ended=None,
                              correlated_by='request_id')
                return {'result': result, 'run_started': (instance, transition)}
        elif ours and transition['action'] == 'stop':
            if program['state'] != 'stopped':
                return None
            return {'result': dict(base, sent=True, stopped_at=transition['at'], stopped_by='lab',
                                   correlated_by='request_id')}
        if instance != action['instance_id']:
            return {'refusal': _refusal('app_unavailable', 'The app restarted before the {} settled.'.format(tool),
                                        evidence={'reason': 'instance_replaced', 'instance_id': action['instance_id'],
                                                  'new_instance_id': instance})}
        started = tool == 'run_program' and journal is not None and journal.ok(Start)
        if after and transition['source'] == 'ui':
            evidence = {'transition': transition}
            if tool == 'run_program':
                evidence['started'] = started
                if started:
                    evidence['run_id'] = program['run_id']
            return {'refusal': _refusal('console_held_by_user', 'The user clicked {} in the app during the {}; '
                                        'console {} is theirs.'.format(transition['action'].capitalize(), tool,
                                                                       reader.console),
                                        evidence=evidence)}
        foreign_lab = transition is not None and transition['source'] == 'lab' and not ours
        ended = after and transition['action'] == 'stop' and (transition['source'] in ENDED_SOURCES or foreign_lab) \
            and program['state'] == 'stopped'
        if tool == 'run_program':
            if ended and started:
                return {'result': dict(base, started_at=None, run_id=program['run_id'],
                                       ended={'source': transition['source'], 'at': transition['at']},
                                       correlated_by='time')}
        elif ended:
            return {'result': dict(base, sent=True, stopped_at=transition['at'], stopped_by=transition['source'],
                                   correlated_by='time')}
        error = journal.error if journal is not None else None
        if error is None:
            if late and journal is not None and not ours and not started and _written_after(app, action['answered_at']):
                return {'refusal': _refusal(
                    'timeout', 'The app answered the {} but shows nothing this request can be matched to; '
                    'read console://B/state for what runs.'.format(tool),
                    evidence={'reason': 'outcome_unknown', 'journal': list(journal.lines),
                              'transition': transition})}
            return None
        if tool == 'stop_program' and error.kind == ErrorKind.PROGRAM_STATE and program['state'] == 'stopped':
            # The run ended between the check and the stop line, before anything this request can date.
            stopped_by = transition['source'] if transition is not None and transition['action'] == 'stop' else None
            return {'result': dict(base, sent=True, stopped_at=None, stopped_by=stopped_by, correlated_by='state')}
        return {'refusal': _app_error(tool, error, journal)}
