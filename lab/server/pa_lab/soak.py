"""The Slice 5 soak check: over a real run, every mutation has exactly one audit record.

`python3 -m pa_lab soak-check --config ... --since T0` (README, "Slice 5 acceptance") reads the
store and the app's `Lab/done/` after the run, with the daemon stopped, and lists every mismatch:

- a request since T0 without exactly one terminal event of its own tool;
- a request still pending;
- a `done/` request the daemon wrote without an `actions` row, and an `actions` row since T0 without
  a `done/` file (only one that settled with nothing sent, `never_sent`, may lack it).
"""

from __future__ import annotations

import json
import sqlite3
from typing import Any, Dict, Iterable, List

from pa_lab import timestamps
from pa_lab.channel_file import REQUEST_PREFIX

# The requests the daemon sends that change nothing, and so have no `actions` row:
# `app_query.describe_program` (preflight_program sends it first), `app_query.game_open`, `shot`.
READ_ONLY_REQUESTS = ('describe_program', 'game_open', 'shot')

TERMINAL_EVENTS = ('request_completed', 'request_failed')

CLEAN: Dict[str, List[Any]] = {'terminal_events': [], 'pending': [], 'done_without_action': [],
                               'action_without_done': []}


def is_read_only(name: str) -> bool:
    """Whether an inbox request name is one of READ_ONLY_REQUESTS."""
    return any(name.startswith(REQUEST_PREFIX + label + '_') for label in READ_ONLY_REQUESTS)


def check(conn: sqlite3.Connection, done_names: Iterable[str], since: str) -> Dict[str, List[Any]]:
    """The mismatches since `since` (a contract timestamp, with milliseconds); `CLEAN` when none.
    `done_names` are the names in `Lab/done/` without `.txt`."""
    timestamps.parse(since)  # a T0 without milliseconds would drop the rows of its own second
    events: Dict[tuple, int] = {}
    for row in conn.execute('SELECT request_id, payload FROM events WHERE type IN (?, ?) AND request_id IS NOT NULL',
                            TERMINAL_EVENTS):
        key = (json.loads(row['payload']).get('tool'), row['request_id'])
        events[key] = events.get(key, 0) + 1
    terminal = []
    for row in conn.execute('SELECT tool, request_id, status FROM requests WHERE created_at >= ? '
                            'ORDER BY tool, request_id', (since,)):
        count = events.get((row['tool'], row['request_id']), 0)
        if count != 1:
            terminal.append({'tool': row['tool'], 'request_id': row['request_id'], 'status': row['status'],
                             'events': count})
    pending = [{'tool': row['tool'], 'request_id': row['request_id']} for row in
               conn.execute("SELECT tool, request_id FROM requests WHERE status = 'pending' ORDER BY tool, request_id")]
    actions = {row['inbox_name'] for row in
               conn.execute('SELECT inbox_name FROM actions WHERE written_at >= ?', (since,))}
    done = {name for name in done_names if name.startswith(REQUEST_PREFIX) and not is_read_only(name)}
    return {'terminal_events': terminal, 'pending': pending,
            'done_without_action': sorted(done - actions), 'action_without_done': sorted(actions - done)}
