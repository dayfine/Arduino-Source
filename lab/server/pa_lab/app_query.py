"""`describe_program` and `game_open`: one-line reads through the app's channel (Slice 4, fork #131).

Both are app commands from #504 (Lab.md "Describe program" and "Screen") that need no owner
token, no program state and no lease:

- `describe_program` prints `describe_program <JSON>`, the selected program's id and the static
  contract of its options: `{"id", "options", "prechecks", "schema": 1}`. `prechecks` is the
  string `unsupported` when the program declares no precheck list; otherwise the list in run
  order, each `{"name", "needs", "skippable"}` with a unique non-empty name (#511).
- `game_open` prints `game_open yes`, or `game_open no REASON` with REASON `home`,
  `user_select`, `black` or `no_video`. It never fails the line and does not tell one game from
  another.

Each call reads the app's state (the app is up; its `instance.id`), sends a fixed command through
the app's channel (`pa_lab.channel`) the way `shot` does, reads the journal, and reads the state
again: a reply that spans an app restart is refused. No caller input reaches the app.
"""

from __future__ import annotations

import json
import re
import time
import uuid
from typing import Any, Callable, Dict, List, Type

from pa_lab import shot
from pa_lab.channel import Command, DescribeProgram, ErrorKind, GameOpen, Journal, await_answer
from pa_lab.lab_reader import ConsoleReader, Unavailable
from pa_lab.shot import Refused
from pa_lab.tools import check_arguments

QUERY_TIMEOUT_MS = 5000
GAME_OPEN_REASONS = ('home', 'user_select', 'black', 'no_video')
PROGRAM_SCHEMA = 1
PRECHECKS_UNSUPPORTED = 'unsupported'

DESCRIBE_RE = re.compile(r'^describe_program (\{.*\})$')
GAME_OPEN_RE = re.compile(r'^game_open (yes|no(?: (\S+))?)$')

__all__ = ['describe_program', 'game_open', 'Refused']


def _instance_id(console: ConsoleReader) -> str:
    try:
        return console.app_state()['instance']['id']
    except Unavailable as e:
        raise Refused(e.refusal()['code'], e.message, e.evidence)


def _ask(console: ConsoleReader, label: str, command: Command, sleep: Callable[[float], None],
         new_uuid: Callable[[], uuid.UUID]) -> Dict[str, Any]:
    """Sends `command` and returns its journal, the instance and the request."""
    started = console.now()
    instance_id = _instance_id(console)
    request_id = str(new_uuid())
    channel = console.channel
    name = channel.request_name(label, request_id)
    shot.send_or_refuse(channel, name, request_id, (command,), label)
    deadline = started + (QUERY_TIMEOUT_MS + shot.ANSWER_GRACE_MS) / 1000.0
    journal = await_answer(channel, name, console.now, sleep, deadline, shot.POLL_S)
    if journal is None:
        raise shot.refuse_timeout(label, name)
    after = _instance_id(console)
    if after != instance_id:
        raise Refused('app_unavailable', 'The app restarted during the call.', {
            'reason': 'instance_replaced', 'instance_id': instance_id, 'new_instance_id': after})
    return {'journal': journal, 'instance_id': instance_id, 'request': name}


def _one_reply(journal: Journal, pattern: 're.Pattern[str]', label: str, kind: Type[Command]):
    """The single printed reply line of a command that succeeded."""
    replies = [m for m in (pattern.match(line) for line in journal.printed) if m]
    if len(replies) != 1 or not journal.ok(kind):
        raise Refused('app_unavailable', 'The {} journal has no single reply line and ok line.'.format(label),
                      {'journal': list(journal.lines)})
    return replies[0]


def _good_precheck(entry: Any) -> bool:
    return (isinstance(entry, dict) and isinstance(entry.get('name'), str) and bool(entry['name'])
            and isinstance(entry.get('needs'), str) and isinstance(entry.get('skippable'), bool))


def _good_prechecks(prechecks: Any) -> bool:
    """`unsupported`, or a list of well-formed entries with unique names."""
    if prechecks == PRECHECKS_UNSUPPORTED:
        return True
    if not isinstance(prechecks, list) or not all(_good_precheck(entry) for entry in prechecks):
        return False
    names = [entry['name'] for entry in prechecks]
    return len(names) == len(set(names))


def _check_contract(doc: Any, lines: List[str]) -> Dict[str, Any]:
    if not isinstance(doc, dict):
        raise Refused('app_unavailable', 'describe_program did not print a JSON object.', {'journal': lines})
    if doc.get('schema') != PROGRAM_SCHEMA:
        raise Refused('unsupported_schema', 'describe_program schema {!r}; this daemon reads {}.'.format(
            doc.get('schema'), PROGRAM_SCHEMA), {'schema': doc.get('schema')})
    problems = []
    if not isinstance(doc.get('id'), str) or not doc['id']:
        problems.append('id')
    if not isinstance(doc.get('options'), list) or not all(isinstance(o, dict) for o in doc['options']):
        problems.append('options')
    if not _good_prechecks(doc.get('prechecks')):
        problems.append('prechecks')
    if problems:
        raise Refused('app_unavailable', 'describe_program printed a bad {}.'.format(', '.join(problems)),
                      {'journal': lines})
    return doc


def describe_program(console: ConsoleReader, arguments: Dict[str, Any],
                     sleep: Callable[[float], None] = time.sleep,
                     new_uuid: Callable[[], uuid.UUID] = uuid.uuid4) -> Dict[str, Any]:
    """The selected program's id and option contract. Raises InvalidArguments or Refused."""
    check_arguments('describe_program', arguments)
    reply = _ask(console, 'describe_program', DescribeProgram(), sleep, new_uuid)
    journal = reply['journal']
    lines = list(journal.lines)
    if journal.error is not None:
        if journal.error.kind == ErrorKind.PANEL_NOT_PROGRAM:
            raise Refused('panel_not_program', 'The selected panel is not a program.', {'journal': lines})
        raise Refused('app_unavailable', "The app's describe_program line failed: {}".format(journal.error.detail),
                      {'journal': lines})
    match = _one_reply(journal, DESCRIBE_RE, 'describe_program', DescribeProgram)
    try:
        doc = json.loads(match.group(1))
    except ValueError:
        raise Refused('app_unavailable', 'describe_program printed invalid JSON.', {'journal': lines}) from None
    program = _check_contract(doc, lines)
    return {
        'console': console.console,
        'instance_id': reply['instance_id'],
        'program': program,
        'prechecks_supported': program['prechecks'] != PRECHECKS_UNSUPPORTED,
        'request': reply['request'],
    }


def game_open(console: ConsoleReader, arguments: Dict[str, Any],
              sleep: Callable[[float], None] = time.sleep,
              new_uuid: Callable[[], uuid.UUID] = uuid.uuid4) -> Dict[str, Any]:
    """Whether a game is in front (`open`), and why not (`reason`). Raises InvalidArguments or Refused."""
    check_arguments('game_open', arguments)
    reply = _ask(console, 'game_open', GameOpen(), sleep, new_uuid)
    journal = reply['journal']
    lines = list(journal.lines)
    if journal.error is not None:
        raise Refused('app_unavailable', "The app's game_open line failed: {}".format(journal.error.detail),
                      {'journal': lines})
    match = _one_reply(journal, GAME_OPEN_RE, 'game_open', GameOpen)
    is_open = match.group(1) == 'yes'
    reason = match.group(2)
    if not is_open and reason not in GAME_OPEN_REASONS:
        raise Refused('app_unavailable', 'The app gave an unknown game_open reason {!r}.'.format(reason),
                      {'journal': lines})
    return {
        'console': console.console,
        'instance_id': reply['instance_id'],
        'open': is_open,
        'reason': reason,
        'request': reply['request'],
    }
