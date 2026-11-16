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

Slice 5b adds three tools on the same record, inbox and settle path:

- `preflight_program(panel, options)`: reads `describe_program` (no lease needed), then in one
  transaction checks the lease, the panel, `blocked`, the channel, that the program declares
  prechecks, and the candidate options against its static contract (`pa_lab.preflight`); then
  sends `preflight <JSON>` (the app stages the options on its UI thread, runs `check_validity`
  and the prechecks in order on its channel worker, and restores the prior options on the UI
  thread). A pass stores a token bound to the app instance, panel, candidate hash, the app's hash
  of the program's full options, last transition, save status and lease, good for `TOKEN_TTL_S`.
- `run_program` needs a current token for exactly its panel and options and sends `set KEY JSON`
  lines for the options, then `start 10`, in one request (the app stops at a failing line).
- `cfr_batch(lines)`: allowlisted console lines (`pa_lab.cfr`) with the program stopped. The save
  fact is marked stale (`cfr_batch`) before the request is written, since any press may change
  the game.

A taken `preflight_program` or `cfr_batch` with no answer past its wait (30 s; a batch's worst case
+ 5 s), while state.json shows the app alive past that time, is `failed` / `timeout` with
`outcome_unknown`: neither changes program state, so nothing justifies keeping the console wedged.

Not yet: the app's `preflight` command is fork #601 (an app without it fails the line, which is
`app_unavailable`), claiming the channel's owner token (a claimed channel is
refused), and `select`.
"""

from __future__ import annotations

import functools
import time
from typing import Any, Callable, Dict, Optional, Tuple

from pa_lab import app_query, cfr, control, preflight, shot, timestamps
from pa_lab.channel import AppError, Command, ConsoleLine, ErrorKind, Journal, Preflight, Start, Stop
from pa_lab.control import Refused
from pa_lab.lab_reader import ConsoleReader, Unavailable

TOOLS = ('preflight_program', 'run_program', 'stop_program', 'cfr_batch')
# `start SECONDS`: how long the app waits for controller=ready before refusing. The daemon checks
# the controller first, so this only covers a reconnect between the check and the line.
START_CONTROLLER_WAIT_S = 10
# How long a call waits for its answer before returning a pending ticket. The app polls its inbox
# every 500 ms; a stop then runs in the background until the program reaches `stopped`.
ANSWER_WAIT_S = {'run_program': START_CONTROLLER_WAIT_S + 3.0, 'stop_program': 10.0,
                 # the prechecks may open a menu or go Home and back
                 'preflight_program': 30.0}
# A cfr_batch waits for its worst case (pa_lab.cfr) plus this.
CFR_SLACK_S = 5.0
POLL_S = 0.1

ENDED_SOURCES = ('program', 'error')
# Tools that change no program state: a taken request with no answer past its wait is ended
# (`outcome_unknown`) rather than kept pending, so a hung check never wedges the console.
LINE_TOOLS = ('preflight_program', 'cfr_batch')
CFR_UNKNOWN = ' (some of its presses may have run; the save is marked stale)'

__all__ = ['ProgramActions', 'TOOLS']


def _written_after(app: Dict[str, Any], answered_at: Optional[str]) -> bool:
    """True once state.json was written after the daemon first read the journal: the app renames
    the journal into place first and rewrites state.json on its next timer."""
    return answered_at is not None and timestamps.parse(app['written_at']) > timestamps.parse(answered_at)


def _alive_past(app: Dict[str, Any], written_at: str, wait_s: float) -> bool:
    """True when state.json was written at or after `written_at` + `wait_s`: the app is alive past
    the request's whole wait, not just slow to write its heartbeat."""
    return timestamps.parse(app['written_at']) >= timestamps.parse(written_at) + wait_s


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


def _need_panel(app: Dict[str, Any], panel: str) -> None:
    if app['panel']['kind'] == 'program' and app['panel']['id'] != panel:
        raise Refused('invalid_arguments', 'The selected panel is {}, not {}; the server does not change '
                      'the selection.'.format(app['panel']['id'], panel), field='panel',
                      evidence={'selected': app['panel']['id']})


def save_now(reader: ConsoleReader) -> Dict[str, Any]:
    """`console://B/save` as a token binds it, with the newest cfr_batch's time (`cfr_at`): a batch
    makes a token stale even when there is no save fact to mark. An unreadable save.json is its own
    status."""
    try:
        doc = dict(reader.save_resource())
    except Unavailable as e:
        doc = {'status': 'unreadable', 'reason': e.reason, 'since': None}
    mark = reader.control.save_mark(reader.console) if reader.control is not None else None
    doc['cfr_at'] = None if mark is None else mark['at']
    return doc


def check_preflight(state: Dict[str, Any], program: Dict[str, Any], lease: Any, args: Dict[str, Any]) -> None:
    """preflight_program's checks after the lease, against `describe_program`'s reply. Raises Refused."""
    app = _need_app(state)
    _need_panel(app, args['panel'])
    _first_blocked(state)
    _need_free_channel(app)
    if program['id'] != args['panel']:
        raise Refused('invalid_arguments', 'The app describes {}, not {}.'.format(program['id'], args['panel']),
                      field='panel', evidence={'selected': program['id']})
    if program['prechecks'] == app_query.PRECHECKS_UNSUPPORTED:
        raise Refused('prechecks_unsupported', '{} declares no start checks; the server does not run a program '
                      'whose checks it cannot run.'.format(args['panel']))
    preflight.check_options(program, args['options'])


def check_run(state: Dict[str, Any], save: Dict[str, Any], plane: control.ControlPlane, now: float,
              lease: Any, args: Dict[str, Any]) -> None:
    """run_program's checks after the lease, in order. Raises Refused; None means send `set` lines + `start`."""
    app = _need_app(state)
    _need_panel(app, args['panel'])
    _first_blocked(state)
    _need_free_channel(app)
    issued = plane.preflight(args['preflight_token'])
    if issued is None or issued['console'] != state['console']:
        raise Refused('preflight_stale', 'That preflight_token is not one this server issued for console {}; run '
                      'preflight_program first.'.format(state['console']), evidence={'reason': 'unknown_token'})
    age = now - timestamps.parse(issued['issued_at'])
    if age > preflight.TOKEN_TTL_S:
        raise Refused('preflight_stale', 'The preflight is {:.0f} s old (limit {} s); run preflight_program '
                      'again.'.format(age, preflight.TOKEN_TTL_S),
                      evidence={'reason': 'expired', 'issued_at': issued['issued_at']})
    now_bound = preflight.binding(state, save, args['panel'], args['options'], lease['token_hash'],
                                  preflight.held_options_hash(app))
    changed = preflight.changed(issued['binding'], now_bound)
    if changed:
        raise Refused('preflight_stale', 'Since the preflight, {} changed; run preflight_program again.'.format(
            ', '.join(changed)), evidence={'reason': 'changed', 'changed': changed,
                                           'preflight_request_id': issued['request_id']})


def check_cfr(state: Dict[str, Any], lease: Any, args: Dict[str, Any]) -> None:
    """cfr_batch's checks after the lease: the app is up, a program panel is selected and stopped,
    controller and video ready, the channel free, every line allowed. Raises Refused."""
    app = _need_app(state)
    if app['panel']['kind'] != 'program':
        raise Refused('panel_not_program', 'The selected panel is not a program; console lines run on its console.')
    _first_blocked(state)
    _need_free_channel(app)
    cfr.check(args['lines'])


def _replaced(described: str, instance: str, lease: Any, args: Dict[str, Any]) -> None:
    raise Refused('app_unavailable', 'The app restarted between describe_program and the preflight.',
                  evidence={'reason': 'instance_replaced', 'instance_id': described, 'new_instance_id': instance})


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

    def preflight_program(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        request = dict(request, tool='preflight_program')
        reader = self._reader(request.get('console'))
        program = None
        if not self._known(client_id, request):
            # Read-only and outside the transaction: the program's contract to check the options against.
            try:
                described = app_query.describe_program(reader, {'console': reader.console}, sleep=self.sleep)
            except shot.Refused as e:
                raise Refused(**e.refusal) from None
            program = described['program']
        check = (lambda state: functools.partial(check_preflight, state, program))
        commands = (lambda args: (Preflight(options=args['options']),))
        return self._act(client_id, request, check, commands, expect_instance=described['instance_id']
                         if program is not None else None)

    def run_program(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        request = dict(request, tool='run_program')
        reader = self._reader(request.get('console'))
        save = save_now(reader)
        check = (lambda state: functools.partial(check_run, state, save, self.plane, reader.now()))
        commands = (lambda args: tuple(preflight.set_commands(args['options'])) +
                    (Start(controller_wait_s=START_CONTROLLER_WAIT_S),))
        return self._act(client_id, request, check, commands)

    def stop_program(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        return self._act(client_id, dict(request, tool='stop_program'),
                         lambda state: functools.partial(check_stop, state), lambda args: (Stop(),))

    def cfr_batch(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        return self._act(client_id, dict(request, tool='cfr_batch'),
                         lambda state: functools.partial(check_cfr, state),
                         lambda args: tuple(ConsoleLine(line) for line in args['lines']), mark_save=True)

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

    def _known(self, client_id: str, request: Dict[str, Any]) -> bool:
        """True when this (client, tool, request_id) already has a record: a retry, answered from it."""
        request_id = request.get('request_id')
        return isinstance(request_id, str) and self.plane.record(client_id, request['tool'], request_id) is not None

    def _act(self, client_id: str, request: Dict[str, Any], make_check: Callable[[Dict[str, Any]], Callable[..., Any]],
             commands: Callable[[Dict[str, Any]], Tuple[Command, ...]], expect_instance: Optional[str] = None,
             mark_save: bool = False) -> Dict[str, Any]:
        reader = self._reader(request.get('console'))
        self.settle_pending(reader.console)
        state = reader.console_state()
        instance = state['app']['instance']['id'] if state['app'] is not None else ''
        check = make_check(state)
        if expect_instance is not None and state['app'] is not None and expect_instance != instance:
            check = functools.partial(_replaced, expect_instance, instance)
        # Only used once begin_action has validated request_id; until then it may be any value.
        name = reader.channel.request_name(request['tool'], str(request.get('request_id')), unique=True)
        record, act = self.plane.begin_action(client_id, request, instance, check, name)
        tool = request['tool']
        action = self.plane.action(client_id, tool, record['request_id'])
        if not act:
            if record['status'] == 'pending' and action is not None:
                return self._settle(reader, action, wait_s=0)
            return record
        if mark_save:
            self.plane.mark_save_stale(reader.console, tool, record['request_id'])
        key = (client_id, tool, record['request_id'])
        try:
            reader.channel.send(action['inbox_name'], record['request_id'], commands(record['arguments']))
        except OSError as e:
            return self.plane.finish_action(*key, refusal=_refusal(
                'app_unavailable', 'Could not write the {} request into the app\'s inbox ({}); nothing was '
                'sent.'.format(tool, e.strerror or e), evidence={'reason': 'inbox_write_failed'}))
        return self._settle(reader, action, wait_s=self._answer_wait(action))

    def _answer_wait(self, action: Dict[str, Any]) -> float:
        if action['tool'] == 'cfr_batch':
            record = self.plane.record(action['client_id'], action['tool'], action['request_id'])
            return cfr.check(record['arguments']['lines']) / 1000.0 + CFR_SLACK_S
        return ANSWER_WAIT_S[action['tool']]

    def _settle(self, reader: ConsoleReader, action: Dict[str, Any], wait_s: float) -> Dict[str, Any]:
        key = (action['client_id'], action['tool'], action['request_id'])
        deadline = reader.now() + wait_s
        wait = self._answer_wait(action)
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
            if action['tool'] in LINE_TOOLS and state['app'] is not None and \
                    _alive_past(state['app'], action['written_at'], wait):
                return self.plane.finish_action(*key, refusal=_refusal(
                    'timeout', 'The app took the {} but did not answer within {:g} s while alive; its outcome '
                    'is unknown{}.'.format(action['tool'], wait, CFR_UNKNOWN if action['tool'] == 'cfr_batch'
                                           else ''),
                    evidence={'reason': 'outcome_unknown', 'request': action['inbox_name']}))
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
        state = reader.console_state()
        app = state['app']
        if app is None:
            return None
        if action['tool'] in LINE_TOOLS:
            return self._judge_line(reader, state, action, journal)
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

    def _judge_line(self, reader: ConsoleReader, state: Dict[str, Any], action: Dict[str, Any],
                    journal: Optional[Journal]) -> Optional[Dict[str, Any]]:
        """A preflight's or cfr_batch's outcome: they change no program state, so only the journal answers.
        An app restart or a UI Start/Stop after the request ends it first."""
        app = state['app']
        instance = app['instance']['id']
        tool = action['tool']
        transition = app['program']['last_transition']
        if instance != action['instance_id']:
            return {'refusal': _refusal('app_unavailable', 'The app restarted before the {} settled.'.format(tool),
                                        evidence={'reason': 'instance_replaced', 'instance_id': action['instance_id'],
                                                  'new_instance_id': instance})}
        if transition is not None and transition['source'] == 'ui' and transition['at'] >= action['written_at']:
            return {'refusal': _refusal('console_held_by_user', 'The user clicked {} in the app during the {}; '
                                        'console {} is theirs.'.format(transition['action'].capitalize(), tool,
                                                                       reader.console),
                                        evidence={'transition': transition})}
        if journal is None:
            return None
        base = {'console': reader.console, 'instance_id': instance}
        record = self.plane.record(action['client_id'], tool, action['request_id'])
        if tool == 'cfr_batch':
            return self._cfr_outcome(base, journal)
        lines = list(journal.lines)
        error = journal.error
        if error is not None:
            if error.kind == ErrorKind.OWNER_CLAIMED:
                return {'refusal': _app_error(tool, error, journal)}
            return {'refusal': _refusal('app_unavailable', "The app's preflight line failed: {}".format(error.detail),
                                        evidence={'journal': lines})}
        args = record['arguments']
        try:
            reply = preflight.parse_reply(list(journal.printed), lines)
        except Refused as e:
            return {'refusal': e.refusal}
        refusal = preflight.outcome(reply, args['panel'], lines)
        if refusal is not None:
            return {'refusal': refusal}
        token_hash = self.plane.request_token_hash(action['client_id'], tool, action['request_id'])
        # The reply's hash is read after the restore: the options the program holds, which the run's
        # state.json must still show. An app without it in the reply may still write it in state.json.
        held = reply.get('options_hash') or preflight.held_options_hash(app)
        bound = preflight.binding(state, save_now(reader), args['panel'], args['options'], token_hash, held)
        token = preflight.new_token(bound)
        issued = self.plane.issue_preflight(reader.console, action['request_id'], token, bound)
        return {'result': dict(base, panel=args['panel'], checks=reply['checks'],
                               candidate_hash=bound['candidate_hash'], options_hash=held,
                               preflight_token=token, issued_at=issued,
                               expires_at=timestamps.format(timestamps.parse(issued) + preflight.TOKEN_TTL_S))}

    @staticmethod
    def _cfr_outcome(base: Dict[str, Any], journal: Journal) -> Dict[str, Any]:
        """Every line ran (`done`), or the batch stopped at `failed_line` (1-based in the batch)."""
        error = journal.error
        if error is not None and error.kind == ErrorKind.OWNER_CLAIMED:
            return {'refusal': _app_error('cfr_batch', error, journal)}
        output = list(journal.printed)
        if error is None:
            return {'result': dict(base, outcome='done', output=output)}
        failed_line = error.command_index + 1 if error.command_index is not None else None
        return {'result': dict(base, outcome='line_failed', failed_line=failed_line, error=error.detail,
                               output=output)}
