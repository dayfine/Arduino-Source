"""`run_program` and `stop_program`: leased actions on the console (Slice 5a of fork #131).

Each call is one request record in the control plane (`pa_lab.control`), keyed by
`(client_id, tool, request_id)`, so a retried call never sends a second start or stop:

1. Read `console://B/state` (that applies any UI takeover, run end or app restart first).
2. `ControlPlane.begin_action`, one transaction: an exact retry gets its stored record; otherwise
   the request's lease token must hold the console and the checks below must pass, or the refusal
   is stored. A passing request is stored `pending` with the app instance it was checked against.
3. Write the fixed request `request <request_id>` / `start 10` (or `stop`) into `Lab/inbox`, the
   same channel commands `labctl.sh start|stop` sends (Lab.md "Channel"). The app records the
   request id in `program.last_transition`.
4. Settle: read the journal and `Lab/state.json` until the outcome is known, then resolve the
   record (`completed` or `failed`) and emit its `request_completed` / `request_failed` event.

How an outcome is recognised, from what Slice 1 writes (`last_transition` holds only the newest
start or stop):

- **By request id.** `last_transition` is `{action, source: lab, request_id: ours}`: our start
  (the lease enters `agent_running`) or our stop (completed once `program.state` is `stopped`).
- **By time** (the app does not keep the start once a stop replaces it, and `run_id` is still
  null). After our start's `ok` line, a stop by `program` or `error` dated after our request means
  the run started and already ended: `completed` with `ended` set, `correlated_by: time`, and the
  lease stays idle. For `stop_program` the same stop is the terminal event that completes it.
- A UI transition dated after our request is the user taking over: `failed`,
  `console_held_by_user` (the control plane has already installed the user lease).
- A new `instance.id` is an app restart: `failed`, `app_unavailable` / `instance_replaced`,
  unless the new instance shows our request id.

A request the app has not answered in time stays `pending` (a ticket) if the app took it from the
inbox, since it may still act; it is settled on the next `request_status`, `next_events`,
`console://B/state` read or action call. If the app never took it, it is withdrawn from the inbox
and `failed` with `timeout`.

Slice 5b adds three tools on the same record, inbox and settle path:

- `preflight_program(panel, options)`: reads `describe_program` (no lease needed), then in one
  transaction checks the lease, the panel, `blocked`, the channel, that the program declares
  prechecks, and the candidate options against its static contract (`pa_lab.preflight`); then
  sends `preflight <JSON>` (the app stages the options, runs `check_validity` and the prechecks
  in order, and restores the prior options). A pass stores a token bound to the app instance,
  panel, options hash, last transition, save status and lease, good for `TOKEN_TTL_S`.
- `run_program` needs a current token for exactly its panel and options and sends `set KEY JSON`
  lines for the options, then `start 10`, in one request (the app stops at a failing line).
- `cfr_batch(lines)`: allowlisted console lines (`pa_lab.cfr`) with the program stopped. The save
  fact is marked stale (`cfr_batch`) before the request is written, since any press may change
  the game.

Not yet: the app's `preflight` command (none exists, so a preflight sent to today's app fails its
line and is `app_unavailable`), claiming the channel's owner token (a claimed channel is
refused), and `select`.
"""

from __future__ import annotations

import functools
import os
import re
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

from pa_lab import app_query, cfr, control, preflight, shot, timestamps
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

JOURNAL_ERROR_RE = re.compile(r'^error \d+ \([^)]*\): (.*)$')
JOURNAL_LINE_RE = re.compile(r'^(?:ok|error) (\d+) ')
PROGRAM_IS_RE = re.compile(r'^Program is ([A-Z_]+)\.$')
ENDED_SOURCES = ('program', 'error')

__all__ = ['ProgramActions', 'TOOLS']


def _journal(lab_dir: Path, name: str) -> Optional[List[str]]:
    """The request's finished journal, or None while there is none."""
    try:
        lines = (lab_dir / 'outbox' / (name + '.out')).read_text(encoding='utf-8', errors='replace').splitlines()
    except FileNotFoundError:
        return None
    return lines if lines and lines[-1] == 'end' else None


def _line_error(lines: Optional[List[str]]) -> Optional[str]:
    for line in lines or ():
        match = JOURNAL_ERROR_RE.match(line)
        if match:
            return match.group(1)
    return None


def _line_ok(lines: Optional[List[str]], command: str) -> bool:
    ok = re.compile(r'^ok \d+ ' + command + r'\b')
    return any(ok.match(line) for line in lines or ())


def _withdraw(inbox: Path) -> bool:
    """True if the request was still in the inbox (the app never took it) and is now gone."""
    try:
        os.remove(inbox)
    except FileNotFoundError:
        return False
    return True


def _refusal(code: str, message: str, **extra: Any) -> Dict[str, Any]:
    return dict(code=code, message=message, **extra)


def _app_error(tool: str, error: str, lines: List[str]) -> Dict[str, Any]:
    """The refusal for a failed start or stop line (LabChannel's messages)."""
    if error.startswith('owned by'):
        # The journal names the owner token: keep it out of the record.
        return _refusal('console_held_by_agent', "A lab script holds the app channel's owner lock; "
                        'the daemon does not claim it in Slice 5a.', evidence={'reason': 'channel_owned'})
    if 'not a program' in error:
        return _refusal('panel_not_program', 'The selected panel is not a program.')
    if 'controller not connected' in error:
        return _refusal('controller_not_ready', 'The app refused the start: {}'.format(error))
    match = PROGRAM_IS_RE.match(error)
    if match and tool == 'run_program':
        code = 'program_running' if match.group(1) in ('RUNNING', 'STOPPING') else 'program_not_stopped'
        return _refusal(code, 'The app refused the start: {}'.format(error))
    return _refusal('app_unavailable', 'The app refused the {}: {}'.format(
        'start' if tool == 'run_program' else 'stop', error), evidence={'journal': lines})


def _first_blocked(state: Dict[str, Any], skip=('console_held_by_user',)) -> None:
    """Raises the first refusal console://B/state lists for a run (the lease is checked apart)."""
    for entry in state['blocked']:
        if entry['code'] not in skip:
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
    changed = preflight.changed(issued['binding'], preflight.binding(state, save, args['panel'], args['options'],
                                                                     lease['token_hash']))
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
        command = (lambda args: 'preflight ' + preflight.dumps(args['options']))
        return self._act(client_id, request, check, command, expect_instance=described['instance_id']
                         if program is not None else None)

    def run_program(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        request = dict(request, tool='run_program')
        reader = self._reader(request.get('console'))
        save = save_now(reader)
        check = (lambda state: functools.partial(check_run, state, save, self.plane, reader.now()))
        command = (lambda args: '\n'.join(preflight.set_lines(args['options']) +
                                          ['start {}'.format(START_CONTROLLER_WAIT_S)]))
        return self._act(client_id, request, check, command)

    def stop_program(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        return self._act(client_id, dict(request, tool='stop_program'),
                         lambda state: functools.partial(check_stop, state), lambda args: 'stop')

    def cfr_batch(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        return self._act(client_id, dict(request, tool='cfr_batch'),
                         lambda state: functools.partial(check_cfr, state), lambda args: '\n'.join(args['lines']),
                         mark_save=True)

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
             command: Callable[[Dict[str, Any]], str], expect_instance: Optional[str] = None,
             mark_save: bool = False) -> Dict[str, Any]:
        reader = self._reader(request.get('console'))
        self.settle_pending(reader.console)
        state = reader.console_state()
        instance = state['app']['instance']['id'] if state['app'] is not None else ''
        check = make_check(state)
        if expect_instance is not None and state['app'] is not None and expect_instance != instance:
            check = functools.partial(_replaced, expect_instance, instance)
        record, act = self.plane.begin_action(client_id, request, instance, check)
        tool = request['tool']
        action = self.plane.action(client_id, tool, record['request_id'])
        if not act:
            if record['status'] == 'pending' and action is not None:
                return self._settle(reader, action, wait_s=0)
            return record
        if mark_save:
            self.plane.mark_save_stale(reader.console, tool, record['request_id'])
        shot.write_request(reader.lab_dir, action['inbox_name'], record['request_id'], command(record['arguments']))
        return self._settle(reader, action, wait_s=self._answer_wait(action))

    def _answer_wait(self, action: Dict[str, Any]) -> float:
        if action['tool'] == 'cfr_batch':
            record = self.plane.record(action['client_id'], action['tool'], action['request_id'])
            return cfr.check(record['arguments']['lines']) / 1000.0 + CFR_SLACK_S
        return ANSWER_WAIT_S[action['tool']]

    def _settle(self, reader: ConsoleReader, action: Dict[str, Any], wait_s: float) -> Dict[str, Any]:
        key = (action['client_id'], action['tool'], action['request_id'])
        deadline = reader.now() + wait_s
        while True:
            outcome = self._judge(reader, action)
            if outcome is not None:
                return self.plane.finish_action(*key, **outcome)
            if reader.now() >= deadline:
                break
            self.sleep(POLL_S)
        age = reader.now() - timestamps.parse(action['written_at'])
        wait = self._answer_wait(action)
        if age >= wait:
            if _withdraw(reader.lab_dir / 'inbox' / (action['inbox_name'] + '.txt')):
                return self.plane.finish_action(*key, refusal=_refusal(
                    'timeout', 'The app did not take the {} request within {:g} s (busy with another '
                    'request?); it was withdrawn and nothing was sent.'.format(
                        action['tool'], wait), evidence={'request': action['inbox_name']}))
            state = reader.console_state()
            if state['app'] is None and state['availability']['reason'] == 'app_not_running':
                return self.plane.finish_action(*key, refusal=_refusal(
                    'app_unavailable', 'The app exited before the {} settled.'.format(action['tool']),
                    evidence={'reason': 'app_not_running'}))
        return self.plane.record(*key)

    def _judge(self, reader: ConsoleReader, action: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """The outcome the app shows now, as finish_action keyword arguments, or None to wait."""
        lines = _journal(reader.lab_dir, action['inbox_name'])
        state = reader.console_state()
        app = state['app']
        if app is None:
            return None
        if action['tool'] in ('preflight_program', 'cfr_batch'):
            return self._judge_line(reader, state, action, lines)
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
        if after and transition['source'] == 'ui':
            return {'refusal': _refusal('console_held_by_user', 'The user clicked {} in the app during the {}; '
                                        'console {} is theirs.'.format(transition['action'].capitalize(), tool,
                                                                       reader.console),
                                        evidence={'transition': transition, 'journal_ok': lines is not None and
                                                  _line_ok(lines, 'start' if tool == 'run_program' else 'stop')})}
        ended = after and transition['action'] == 'stop' and transition['source'] in ENDED_SOURCES and \
            program['state'] == 'stopped'
        if tool == 'run_program':
            if ended and _line_ok(lines, 'start'):
                return {'result': dict(base, started_at=None, run_id=program['run_id'],
                                       ended={'source': transition['source'], 'at': transition['at']},
                                       correlated_by='time')}
        elif ended:
            return {'result': dict(base, sent=True, stopped_at=transition['at'], stopped_by=transition['source'],
                                   correlated_by='time')}
        error = _line_error(lines)
        if error is None:
            return None
        if tool == 'stop_program' and PROGRAM_IS_RE.match(error) and program['state'] == 'stopped':
            # The run ended between the check and the stop line, before anything this request can date.
            stopped_by = transition['source'] if transition is not None and transition['action'] == 'stop' else None
            return {'result': dict(base, sent=True, stopped_at=None, stopped_by=stopped_by, correlated_by='state')}
        return {'refusal': _app_error(tool, error, lines)}

    def _judge_line(self, reader: ConsoleReader, state: Dict[str, Any], action: Dict[str, Any],
                    lines: Optional[List[str]]) -> Optional[Dict[str, Any]]:
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
        if lines is None:
            return None
        base = {'console': reader.console, 'instance_id': instance}
        record = self.plane.record(action['client_id'], tool, action['request_id'])
        if tool == 'cfr_batch':
            return self._cfr_outcome(base, lines)
        error = _line_error(lines)
        if error is not None:
            if error.startswith('owned by'):
                return {'refusal': _app_error(tool, error, lines)}
            return {'refusal': _refusal('app_unavailable', "The app's preflight line failed: {}".format(error),
                                        evidence={'journal': lines})}
        args = record['arguments']
        try:
            reply = preflight.parse_reply(lines)
        except Refused as e:
            return {'refusal': e.refusal}
        refusal = preflight.outcome(reply, args['panel'], lines)
        if refusal is not None:
            return {'refusal': refusal}
        token_hash = self.plane.request_token_hash(action['client_id'], tool, action['request_id'])
        bound = preflight.binding(state, save_now(reader), args['panel'], args['options'], token_hash)
        token = preflight.new_token(bound)
        issued = self.plane.issue_preflight(reader.console, action['request_id'], token, bound)
        return {'result': dict(base, panel=args['panel'], checks=reply['checks'], options_hash=bound['options_hash'],
                               preflight_token=token, issued_at=issued,
                               expires_at=timestamps.format(timestamps.parse(issued) + preflight.TOKEN_TTL_S))}

    @staticmethod
    def _cfr_outcome(base: Dict[str, Any], lines: List[str]) -> Dict[str, Any]:
        """Every line ran (`done`), or the batch stopped at `failed_line` (1-based in the batch)."""
        error = _line_error(lines)
        if error is not None and error.startswith('owned by'):
            return {'refusal': _app_error('cfr_batch', error, lines)}
        output = [line for line in lines if not JOURNAL_LINE_RE.match(line) and line != 'end']
        if error is None:
            return {'result': dict(base, outcome='done', output=output)}
        failed = next(int(m.group(1)) for m in map(JOURNAL_LINE_RE.match, lines) if m and m.group(0).startswith('error'))
        # Line 1 of the request is `request <uuid>`; the batch starts on line 2.
        return {'result': dict(base, outcome='line_failed', failed_line=failed - 1, error=error, output=output)}
