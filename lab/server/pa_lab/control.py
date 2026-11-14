"""The durable control plane: leases, request records, tickets and events (Slice 3 of fork #131).

Everything here is a row in the daemon's SQLite file (`pa_lab.db`), changed only inside one
`BEGIN IMMEDIATE` transaction, so a crash or a second daemon cannot split a decision:

- **Leases** (`free`, `agent_idle`, `agent_running`, `user`; docs/design/Agent-Integration.md
  "Leases, requests and events"). An agent lease is an opaque token; only its hash is stored in
  the lease row. `agent_idle` expires after its TTL (1200 s by default); `agent_running` never
  expires. A UI Start/Stop the app reports (`last_transition.source == 'ui'`) revokes any agent
  lease and installs a `user` lease with no timeout; only the operator releases it.
- **Requests.** Every mutating call is keyed by `(client_id, tool, request_id)` and stores its
  normalized arguments and terminal result. An exact retry gets the stored record back; the same
  key with other arguments is refused. A `pending` record is an unresolved ticket.
- **Events** get a strictly increasing `seq` that is never reused. `next_events` returns the
  events after a cursor and records that cursor; compaction never deletes an event after an
  active cursor or one that belongs to an unresolved ticket.
- **Reconciliation.** `observe(app)` is applied whenever the daemon reads the app's state and
  once at start: it applies each new transition exactly once (UI takeover, `stopped` events)
  and ends a running lease whose app instance is gone.
"""

from __future__ import annotations

import hashlib
import json
import secrets
import sqlite3
import threading
import time
from typing import Any, Dict, List, Optional, Sequence, Tuple

from pa_lab import contracts, timestamps
from pa_lab.db import Database

DEFAULT_TTL_S = 1200
MAX_WAIT_S = 60.0
DEFAULT_PAGE = 100
MAX_PAGE = 500
POLL_S = 0.2  # another process's writes are seen by polling; this process's wake the waiters


class Refused(Exception):
    """A refusal (contracts REFUSAL) the control plane could not or did not store as a record."""

    def __init__(self, code: str, message: str, **extra: Any):
        super().__init__(message)
        self.refusal = dict(code=code, message=message, **extra)


class Retention:
    """Compaction bounds. Events after an active cursor and events of unresolved tickets are kept
    whatever these say; a cursor not used for `cursor_ttl_s` stops being active."""

    def __init__(self, keep_events: int = 10000, cursor_ttl_s: float = 7 * 86400,
                 request_ttl_s: float = 7 * 86400, compact_every: int = 500):
        self.keep_events = keep_events
        self.cursor_ttl_s = cursor_ttl_s
        self.request_ttl_s = request_ttl_s
        self.compact_every = compact_every


def _hash(token: Optional[str]) -> Optional[str]:
    return None if token is None else hashlib.sha256(token.encode('utf-8')).hexdigest()


def _json(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False)


def _clock_time(at: str) -> str:
    return at[11:16]


def lease_doc(row: sqlite3.Row) -> Dict[str, Any]:
    return {'state': row['state'], 'owner': row['owner'], 'acquired_at': row['acquired_at'],
            'expires_at': row['expires_at']}


def normalized_arguments(tool: str, arguments: Dict[str, Any]) -> Dict[str, Any]:
    """The arguments a record stores and a retry is compared against: defaults filled in."""
    out = dict(arguments)
    if tool in ('acquire_console', 'renew_console'):
        out.setdefault('ttl_s', DEFAULT_TTL_S)
    return out


def _redacted(result: Optional[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
    """A result as events and status reads show it: never the lease token."""
    if result is None or 'lease_token' not in result:
        return result
    return {k: v for k, v in result.items() if k != 'lease_token'}


class ControlPlane:
    def __init__(self, db: Database, consoles: Sequence[str], now=timestamps.wall_clock,
                 retention: Optional[Retention] = None):
        self.db = db
        self.consoles = tuple(consoles)
        self.now = now
        self.retention = retention or Retention()
        self._changed = threading.Condition()
        self._appended = 0
        with self.db.write() as conn:
            stamp = self._stamp()
            for console in self.consoles:
                conn.execute("INSERT OR IGNORE INTO leases (console, state, updated_at) VALUES (?, 'free', ?)",
                             (console, stamp))

    # ---- reads -------------------------------------------------------------------------------

    def lease(self, console: str) -> Dict[str, Any]:
        """The current lease, after applying a due expiry (which is a write and an event)."""
        self._check_console(console)
        with self._writing() as conn:
            self._expire_due(conn, console)
            return lease_doc(self._lease_row(conn, console))

    def request_status(self, client_id: str, request_id: str) -> Dict[str, Any]:
        """The caller's own request record (a ticket while pending); the lease token is redacted."""
        row = self.db.read().execute('SELECT * FROM requests WHERE client_id = ? AND request_id = ?',
                                     (client_id, request_id)).fetchone()
        if row is None:
            raise Refused('invalid_arguments', 'No request {} from this client.'.format(request_id),
                          field='request_id')
        record = self._record(row)
        record['result'] = _redacted(record['result'])
        return record

    def next_events(self, client_id: str, console: str, after_seq: Any, timeout_s: Any = 0,
                    limit: Any = DEFAULT_PAGE) -> Dict[str, Any]:
        """Events with seq > after_seq, oldest first, waiting up to timeout_s (<= 60) for one.

        `after_seq` is also the client's confirmed position: compaction keeps everything after it,
        so a client that lost a response asks again with the same cursor and loses nothing.
        """
        self._check_console(console)
        after_seq = self._int_arg('after_seq', after_seq, 0, None)
        limit = self._int_arg('limit', limit, 1, MAX_PAGE)
        if isinstance(timeout_s, bool) or not isinstance(timeout_s, (int, float)) or \
                not 0 <= timeout_s <= MAX_WAIT_S:
            raise Refused('invalid_arguments', 'timeout_s must be a number of seconds 0..{:g}.'.format(MAX_WAIT_S),
                          field='timeout_s')
        with self._writing() as conn:
            head = self._head(conn)
            if after_seq > head:
                raise Refused('invalid_arguments', 'after_seq {} is ahead of the newest event {}.'.format(
                    after_seq, head), field='after_seq', evidence={'head_seq': head})
            compacted = int(self._meta(conn, 'compacted_through', '0'))
            if after_seq < compacted:
                raise Refused('invalid_arguments',
                              'Events up to seq {} were compacted; this cursor ({}) is no longer active. '
                              'Read console://{}/state and resume from {}.'.format(
                                  compacted, after_seq, console, compacted),
                              field='after_seq', evidence={'compacted_through': compacted})
            conn.execute('INSERT OR REPLACE INTO cursors (client_id, console, after_seq, seen_at) VALUES (?, ?, ?, ?)',
                         (client_id, console, after_seq, self._stamp()))
        deadline = time.monotonic() + timeout_s
        while True:
            rows = self.db.read().execute(
                'SELECT * FROM events WHERE console = ? AND seq > ? ORDER BY seq LIMIT ?',
                (console, after_seq, limit)).fetchall()
            remaining = deadline - time.monotonic()
            if rows or remaining <= 0:
                break
            with self._changed:
                self._changed.wait(min(remaining, POLL_S))
        events = [self._event(row) for row in rows]
        page = {'schema': 1, 'events': events, 'next_after_seq': events[-1]['seq'] if events else after_seq}
        return contracts.validate('event_page', page).data

    # ---- requests (acquire / renew / release) -------------------------------------------------

    def execute(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        """Runs one lease request once. Returns its request_record: `completed` or `refused`, the
        stored one for an exact retry. Raises Refused for a request that cannot be keyed or whose
        key was used with other arguments; those are not stored."""
        req, args, token_hash = self._parse_request(client_id, request)
        handler = {'acquire_console': self._acquire, 'renew_console': self._renew,
                   'release_console': self._release}.get(req['tool'])
        if handler is None:
            raise Refused('invalid_arguments', '{} is not served before Slice 5.'.format(req['tool']), field='tool')
        with self._writing() as conn:
            existing = self._existing(conn, req, args, token_hash)
            if existing is not None:
                return existing
            self._expire_due(conn, req['console'])
            stamp = self._stamp()
            try:
                result, refusal = handler(conn, req, args, token_hash), None
            except Refused as e:
                result, refusal = None, e.refusal
            status = 'completed' if refusal is None else 'refused'
            conn.execute('INSERT INTO requests (client_id, tool, request_id, console, token_hash, status, created_at, '
                         'completed_at, arguments, result, refusal) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
                         (client_id, req['tool'], req['request_id'], req['console'], token_hash, status, stamp,
                          stamp, _json(args), None if result is None else _json(result),
                          None if refusal is None else _json(refusal)))
            self._request_event(conn, req['console'], req['request_id'], req['tool'], result, refusal)
            return self._record(self._request_row(conn, client_id, req['tool'], req['request_id']))

    # ---- tickets (long operations; Slice 5 starts and resolves them) --------------------------

    def open_ticket(self, client_id: str, request: Dict[str, Any]) -> Dict[str, Any]:
        """Stores a `pending` record for a long operation, once. An exact retry gets the same record."""
        req, args, token_hash = self._parse_request(client_id, request)
        with self._writing() as conn:
            existing = self._existing(conn, req, args, token_hash)
            if existing is not None:
                return existing
            conn.execute('INSERT INTO requests (client_id, tool, request_id, console, token_hash, status, created_at, '
                         "arguments) VALUES (?, ?, ?, ?, ?, 'pending', ?, ?)",
                         (client_id, req['tool'], req['request_id'], req['console'], token_hash, self._stamp(),
                          _json(args)))
            return self._record(self._request_row(conn, client_id, req['tool'], req['request_id']))

    def resolve_ticket(self, client_id: str, tool: str, request_id: str, result: Optional[Dict[str, Any]] = None,
                       refusal: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Completes (result) or fails (refusal) a pending ticket once; later calls return the record."""
        if (result is None) == (refusal is None):
            raise ValueError('resolve_ticket needs exactly one of result and refusal')
        with self._writing() as conn:
            row = self._request_row(conn, client_id, tool, request_id)
            if row is None:
                raise Refused('invalid_arguments', 'No ticket {}.'.format(request_id), field='request_id')
            if row['status'] != 'pending':
                return self._record(row)
            status = 'completed' if refusal is None else 'failed'
            conn.execute('UPDATE requests SET status = ?, completed_at = ?, result = ?, refusal = ? '
                         'WHERE client_id = ? AND tool = ? AND request_id = ?',
                         (status, self._stamp(), None if result is None else _json(result),
                          None if refusal is None else _json(refusal), client_id, tool, request_id))
            record = self._record(self._request_row(conn, client_id, tool, request_id))
            self._request_event(conn, row['console'], request_id, tool, result, refusal)
            return record

    # ---- runs and the operator ----------------------------------------------------------------

    def begin_run(self, console: str, lease_token: str, request_id: str, instance_id: str) -> Dict[str, Any]:
        """agent_idle -> agent_running for the lease holder (Slice 5's run_program calls this once
        the app reports the start). A running lease never expires."""
        self._check_console(console)
        with self._writing() as conn:
            self._expire_due(conn, console)
            row = self._held(conn, console, _hash(lease_token))
            if row['state'] == 'agent_running':
                return lease_doc(row)
            stamp = self._stamp()
            self._set_lease(conn, console, 'run_started', request_id=request_id, instance_id=instance_id,
                            state='agent_running', expires_at=None, run_request_id=request_id,
                            run_instance_id=instance_id, run_started_at=stamp)
            return lease_doc(self._lease_row(conn, console))

    def claim_user(self, console: str) -> Dict[str, Any]:
        """The operator takes the console by hand (CLI); same effect as a UI click."""
        self._check_console(console)
        with self._writing() as conn:
            self._take_over(conn, console, self._stamp(), 'operator_claim', None)
            return lease_doc(self._lease_row(conn, console))

    def release_user(self, console: str) -> Dict[str, Any]:
        """The operator says the user is done (CLI only; an agent token cannot end a user lease)."""
        self._check_console(console)
        with self._writing() as conn:
            row = self._lease_row(conn, console)
            if row['state'] != 'user':
                raise Refused('invalid_arguments', 'Console {} is {}, not held by the user.'.format(
                    console, row['state']), field='console')
            self._set_lease(conn, console, 'operator_release', **self._freed())
            return lease_doc(self._lease_row(conn, console))

    # ---- reconciliation -----------------------------------------------------------------------

    def observe(self, console: str, app: Dict[str, Any]) -> Dict[str, Any]:
        """Applies what a parsed `Lab/state.json` shows, each transition once; returns the lease.

        - a new UI transition revokes any agent lease and installs the user lease (no timeout);
        - a new stop transition is a `stopped` event with its source;
        - a running lease ends (back to agent_idle with a fresh TTL) when its run's stop is
          reported or its app instance is gone.
        """
        self._check_console(console)
        instance = app['instance']['id']
        transition = app['program']['last_transition']
        key = None if transition is None else _json([instance, transition['at'], transition['action'],
                                                     transition['source'], transition['request_id']])
        with self._writing() as conn:
            self._expire_due(conn, console)
            seen = conn.execute('SELECT transition_key FROM observations WHERE console = ?', (console,)).fetchone()
            new = transition is not None and (seen is None or seen['transition_key'] != key)
            row = self._lease_row(conn, console)
            if row['state'] == 'agent_running':
                if instance != row['run_instance_id']:
                    self._end_run(conn, console, row, 'app_restarted', instance)
                elif new and transition['action'] == 'stop' and transition['source'] != 'ui':
                    self._end_run(conn, console, row, 'run_ended', instance)
            if new:
                if transition['action'] == 'stop':
                    self._append(conn, 'stopped', console, {'source': transition['source'],
                                                            'run_id': app['program']['run_id']},
                                 instance_id=instance, request_id=transition['request_id'], at=transition['at'])
                if transition['source'] == 'ui':
                    self._take_over(conn, console, transition['at'], 'ui_takeover', instance)
            conn.execute('INSERT OR REPLACE INTO observations (console, instance_id, transition_key, program_state, '
                         'observed_at) VALUES (?, ?, ?, ?, ?)',
                         (console, instance, key if transition is not None else
                          (seen['transition_key'] if seen is not None else None),
                          app['program']['state'], self._stamp()))
            return lease_doc(self._lease_row(conn, console))

    # ---- retention ----------------------------------------------------------------------------

    def compact(self) -> Dict[str, int]:
        """Deletes old events and finished records within the Retention bounds. Never deletes an
        event after an active cursor, an event of a pending ticket, or a pending record."""
        r = self.retention
        with self.db.write() as conn:
            now = self.now()
            stale = [(row['client_id'], row['console']) for row in conn.execute('SELECT * FROM cursors')
                     if now - timestamps.parse(row['seen_at']) > r.cursor_ttl_s]
            conn.executemany('DELETE FROM cursors WHERE client_id = ? AND console = ?', stale)
            head = conn.execute('SELECT MAX(seq) FROM events').fetchone()[0] or 0
            floor = conn.execute('SELECT MIN(after_seq) FROM cursors').fetchone()[0]
            limit = min(head if floor is None else floor, head - r.keep_events)
            keep = "(request_id IS NULL OR request_id NOT IN (SELECT request_id FROM requests WHERE status = 'pending'))"
            through = conn.execute('SELECT MAX(seq) FROM events WHERE seq <= ? AND ' + keep, (limit,)).fetchone()[0]
            events = conn.execute('DELETE FROM events WHERE seq <= ? AND ' + keep, (limit,)).rowcount
            if through is not None:
                old = int(self._meta(conn, 'compacted_through', '0'))
                conn.execute("INSERT OR REPLACE INTO meta (key, value) VALUES ('compacted_through', ?)",
                             (str(max(old, through)),))
            done = [(row['client_id'], row['tool'], row['request_id']) for row in
                    conn.execute("SELECT client_id, tool, request_id, completed_at FROM requests WHERE status != 'pending'")
                    if now - timestamps.parse(row['completed_at']) > r.request_ttl_s]
            conn.executemany('DELETE FROM requests WHERE client_id = ? AND tool = ? AND request_id = ?', done)
        self._appended = 0
        return {'events': events, 'requests': len(done), 'cursors': len(stale)}

    # ---- internals ----------------------------------------------------------------------------

    def _writing(self):
        plane = self

        class _Write:
            def __enter__(self):
                self._cm = plane.db.write()
                return self._cm.__enter__()

            def __exit__(self, *exc):
                result = self._cm.__exit__(*exc)
                if exc[0] is None:
                    with plane._changed:
                        plane._changed.notify_all()
                    if plane._appended >= plane.retention.compact_every:
                        plane.compact()
                return result

        return _Write()

    def _stamp(self) -> str:
        return timestamps.format(self.now())

    def _check_console(self, console: Any) -> None:
        if console not in self.consoles:
            raise Refused('console_not_allowed', 'Console {!r} is not served; only {}.'.format(
                console, ', '.join(self.consoles)))

    @staticmethod
    def _int_arg(name: str, value: Any, minimum: int, maximum: Optional[int]) -> int:
        if isinstance(value, bool) or not isinstance(value, int) or value < minimum or \
                (maximum is not None and value > maximum):
            raise Refused('invalid_arguments', '{} must be an integer {}{}.'.format(
                name, minimum, '..{}'.format(maximum) if maximum is not None else ' or more'), field=name)
        return value

    def _parse_request(self, client_id: str, request: Dict[str, Any]) -> Tuple[Dict[str, Any], Dict[str, Any], Optional[str]]:
        self._check_console(request.get('console'))
        try:
            req = contracts.validate('request', dict(request, client_id=client_id)).data
        except contracts.ContractError as e:
            raise Refused('invalid_arguments', str(e), field=e.errors[0].path)
        return req, normalized_arguments(req['tool'], req['arguments']), _hash(req.get('lease_token'))

    def _request_row(self, conn, client_id: str, tool: str, request_id: str) -> Optional[sqlite3.Row]:
        return conn.execute('SELECT * FROM requests WHERE client_id = ? AND tool = ? AND request_id = ?',
                            (client_id, tool, request_id)).fetchone()

    def _existing(self, conn, req, args, token_hash) -> Optional[Dict[str, Any]]:
        row = self._request_row(conn, req['client_id'], req['tool'], req['request_id'])
        if row is None:
            return None
        if row['console'] != req['console'] or row['arguments'] != _json(args) or row['token_hash'] != token_hash:
            raise Refused('request_id_reused', 'Request {} was already used for {} with other arguments.'.format(
                req['request_id'], req['tool']), evidence={'status': row['status']})
        return self._record(row)

    @staticmethod
    def _record(row: sqlite3.Row) -> Dict[str, Any]:
        doc = {'schema': 1, 'request_id': row['request_id'], 'client_id': row['client_id'], 'tool': row['tool'],
               'status': row['status'], 'created_at': row['created_at'], 'completed_at': row['completed_at'],
               'arguments': json.loads(row['arguments']),
               'result': None if row['result'] is None else json.loads(row['result']),
               'refusal': None if row['refusal'] is None else json.loads(row['refusal'])}
        return contracts.validate('request_record', doc).data

    @staticmethod
    def _event(row: sqlite3.Row) -> Dict[str, Any]:
        return {'schema': 1, 'seq': row['seq'], 'type': row['type'], 'at': row['at'], 'console': row['console'],
                'instance_id': row['instance_id'], 'request_id': row['request_id'],
                'payload': json.loads(row['payload'])}

    @staticmethod
    def _meta(conn, key: str, default: str) -> str:
        row = conn.execute('SELECT value FROM meta WHERE key = ?', (key,)).fetchone()
        return default if row is None else row['value']

    @staticmethod
    def _head(conn) -> int:
        row = conn.execute("SELECT seq FROM sqlite_sequence WHERE name = 'events'").fetchone()
        return 0 if row is None else row['seq']

    def _append(self, conn, type: str, console: str, payload: Dict[str, Any], instance_id: Optional[str] = None,
                request_id: Optional[str] = None, at: Optional[str] = None) -> int:
        doc = {'schema': 1, 'seq': 1, 'type': type, 'at': at or self._stamp(), 'console': console,
               'instance_id': instance_id, 'request_id': request_id, 'payload': payload}
        doc = contracts.validate('event', doc).data
        cursor = conn.execute('INSERT INTO events (type, at, console, instance_id, request_id, payload) '
                              'VALUES (?, ?, ?, ?, ?, ?)',
                              (type, doc['at'], console, instance_id, request_id, _json(doc['payload'])))
        self._appended += 1
        return cursor.lastrowid

    def _request_event(self, conn, console, request_id, tool, result, refusal) -> None:
        if refusal is None:
            self._append(conn, 'request_completed', console,
                         {'request_id': request_id, 'tool': tool, 'result': _redacted(result)}, request_id=request_id)
        else:
            self._append(conn, 'request_failed', console,
                         {'request_id': request_id, 'tool': tool, 'refusal': refusal}, request_id=request_id)

    # ---- lease rows

    @staticmethod
    def _lease_row(conn, console: str) -> sqlite3.Row:
        return conn.execute('SELECT * FROM leases WHERE console = ?', (console,)).fetchone()

    @staticmethod
    def _freed(ended_reason: Optional[str] = None, ended_token_hash: Optional[str] = None) -> Dict[str, Any]:
        return dict(state='free', owner=None, client_id=None, token_hash=None, acquired_at=None, expires_at=None,
                    ttl_s=None, run_request_id=None, run_instance_id=None, run_started_at=None,
                    ended_token_hash=ended_token_hash, ended_reason=ended_reason)

    def _set_lease(self, conn, console: str, reason: str, request_id: Optional[str] = None,
                   instance_id: Optional[str] = None, at: Optional[str] = None, **fields: Any) -> None:
        old = lease_doc(self._lease_row(conn, console))
        fields['updated_at'] = self._stamp()
        assignments = ', '.join('{} = ?'.format(name) for name in fields)
        conn.execute('UPDATE leases SET {} WHERE console = ?'.format(assignments), (*fields.values(), console))
        new = lease_doc(self._lease_row(conn, console))
        # `reason` is an extra payload field: the v1 contract keeps it as an unknown field.
        self._append(conn, 'lease_changed', console, {'old': old, 'new': new, 'reason': reason},
                     instance_id=instance_id, request_id=request_id, at=at)

    def _expire_due(self, conn, console: str) -> None:
        row = self._lease_row(conn, console)
        if row['state'] == 'agent_idle' and timestamps.parse(row['expires_at']) <= self.now():
            self._set_lease(conn, console, 'expired', at=row['expires_at'],
                            **self._freed('expired', row['token_hash']))

    def _end_run(self, conn, console: str, row: sqlite3.Row, reason: str, instance_id: str) -> None:
        self._set_lease(conn, console, reason, request_id=row['run_request_id'], instance_id=instance_id,
                        state='agent_idle', expires_at=timestamps.format(self.now() + row['ttl_s']),
                        run_request_id=None, run_instance_id=None, run_started_at=None)

    def _take_over(self, conn, console: str, at: str, reason: str, instance_id: Optional[str]) -> None:
        row = self._lease_row(conn, console)
        if row['state'] == 'user':
            return
        fields = self._freed(reason, row['token_hash'] or row['ended_token_hash'])
        fields.update(state='user', owner='user', acquired_at=at)
        self._set_lease(conn, console, reason, instance_id=instance_id, **fields)

    def _held(self, conn, console: str, token_hash: Optional[str]) -> sqlite3.Row:
        """The lease row if `token_hash` holds it; else the refusal that says why not."""
        row = self._lease_row(conn, console)
        if token_hash is not None and row['token_hash'] == token_hash and row['state'] in ('agent_idle', 'agent_running'):
            return row
        if token_hash is not None and row['ended_token_hash'] == token_hash:
            if row['state'] == 'user' or row['ended_reason'] in ('ui_takeover', 'operator_claim'):
                raise self._held_by_user(console, row)
            if row['ended_reason'] == 'expired':
                raise Refused('lease_expired', 'The lease on console {} expired; acquire it again.'.format(console))
        raise Refused('lease_invalid', 'That token does not hold console {}.'.format(console))

    @staticmethod
    def _held_by_user(console: str, row: sqlite3.Row) -> Refused:
        return Refused('console_held_by_user', 'The user holds console {} since {}; ask them to release it.'.format(
            console, _clock_time(row['acquired_at'])), evidence={'since': row['acquired_at']})

    def _acquire(self, conn, req, args, token_hash) -> Dict[str, Any]:
        console = req['console']
        row = self._lease_row(conn, console)
        if row['state'] == 'user':
            raise self._held_by_user(console, row)
        if row['state'] != 'free':
            raise Refused('console_held_by_agent', 'Console {} is leased to {} ({}).'.format(
                console, row['owner'], row['state']), evidence=lease_doc(row))
        token = secrets.token_urlsafe(32)
        stamp = self._stamp()
        fields = self._freed()
        fields.update(state='agent_idle', owner=args['owner'], client_id=req['client_id'], token_hash=_hash(token),
                      acquired_at=stamp, expires_at=timestamps.format(timestamps.parse(stamp) + args['ttl_s']),
                      ttl_s=args['ttl_s'])
        self._set_lease(conn, console, 'acquired', request_id=req['request_id'], at=stamp, **fields)
        return dict(lease_doc(self._lease_row(conn, console)), lease_token=token, ttl_s=args['ttl_s'])

    def _renew(self, conn, req, args, token_hash) -> Dict[str, Any]:
        console = req['console']
        row = self._held(conn, console, token_hash)
        if row['state'] == 'agent_idle':
            conn.execute('UPDATE leases SET expires_at = ?, ttl_s = ?, updated_at = ? WHERE console = ?',
                         (timestamps.format(self.now() + args['ttl_s']), args['ttl_s'], self._stamp(), console))
        else:  # running: never expires; the new TTL applies when the run ends
            conn.execute('UPDATE leases SET ttl_s = ?, updated_at = ? WHERE console = ?',
                         (args['ttl_s'], self._stamp(), console))
        return dict(lease_doc(self._lease_row(conn, console)), ttl_s=args['ttl_s'])

    def _release(self, conn, req, args, token_hash) -> Dict[str, Any]:
        console = req['console']
        row = self._held(conn, console, token_hash)
        if row['state'] == 'agent_running':
            raise Refused('program_running', 'A run holds console {}; stop it before releasing.'.format(console))
        self._set_lease(conn, console, 'released', request_id=req['request_id'],
                        **self._freed('released', row['token_hash']))
        return lease_doc(self._lease_row(conn, console))
