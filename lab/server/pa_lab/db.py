"""The daemon's SQLite file: migrations, connections and write transactions (Slice 3 of fork #131).

SQLite is the source of truth for leases, request records (idempotency and tickets), events and
event cursors; JSONL is only ever an export. Every write runs in `BEGIN IMMEDIATE`, so two
daemons, two threads or a daemon and the operator CLI that share the file serialize on SQLite's
lock: there is no read-then-write window in which both could win a lease. A process that dies
mid-transaction leaves nothing behind (the journal rolls it back on the next open).

The schema version is `PRAGMA user_version`. `MIGRATIONS[i]` takes the file from version `i` to
`i + 1`; a file newer than this code is refused rather than guessed at.
"""

from __future__ import annotations

from contextlib import contextmanager
import os
from pathlib import Path
import sqlite3
import threading
from typing import Iterator, List

BUSY_TIMEOUT_S = 30.0

MIGRATIONS: List[List[str]] = [
    # 1: leases, requests (idempotency + tickets), events, cursors, observations, meta.
    [
        """CREATE TABLE leases (
            console TEXT PRIMARY KEY,
            state TEXT NOT NULL CHECK (state IN ('free', 'agent_idle', 'agent_running', 'user')),
            owner TEXT,
            client_id TEXT,
            token_hash TEXT,
            acquired_at TEXT,
            expires_at TEXT,
            ttl_s INTEGER,
            run_request_id TEXT,
            run_instance_id TEXT,
            run_started_at TEXT,
            ended_token_hash TEXT,
            ended_reason TEXT,
            updated_at TEXT NOT NULL
        )""",
        # One row per mutating call; status 'pending' is an unresolved ticket.
        """CREATE TABLE requests (
            client_id TEXT NOT NULL,
            tool TEXT NOT NULL,
            request_id TEXT NOT NULL,
            console TEXT NOT NULL,
            token_hash TEXT,
            status TEXT NOT NULL CHECK (status IN ('pending', 'completed', 'refused', 'failed')),
            created_at TEXT NOT NULL,
            completed_at TEXT,
            arguments TEXT NOT NULL,
            result TEXT,
            refusal TEXT,
            PRIMARY KEY (client_id, tool, request_id)
        )""",
        "CREATE INDEX requests_by_id ON requests (client_id, request_id)",
        "CREATE INDEX requests_unresolved ON requests (status) WHERE status = 'pending'",
        # AUTOINCREMENT: a seq is never reused, even after compaction deletes the newest rows.
        """CREATE TABLE events (
            seq INTEGER PRIMARY KEY AUTOINCREMENT,
            type TEXT NOT NULL,
            at TEXT NOT NULL,
            console TEXT NOT NULL,
            instance_id TEXT,
            request_id TEXT,
            payload TEXT NOT NULL
        )""",
        "CREATE INDEX events_by_request ON events (request_id) WHERE request_id IS NOT NULL",
        # The last after_seq each client confirmed; compaction never deletes past an active one.
        """CREATE TABLE cursors (
            client_id TEXT NOT NULL,
            console TEXT NOT NULL,
            after_seq INTEGER NOT NULL,
            seen_at TEXT NOT NULL,
            PRIMARY KEY (client_id, console)
        )""",
        # The last app transition applied per console, so one UI click is applied exactly once.
        """CREATE TABLE observations (
            console TEXT PRIMARY KEY,
            instance_id TEXT,
            transition_key TEXT,
            program_state TEXT,
            observed_at TEXT NOT NULL
        )""",
        "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)",
    ],
]

SCHEMA_VERSION = len(MIGRATIONS)


class DatabaseError(RuntimeError):
    pass


def _connect(path: Path) -> sqlite3.Connection:
    conn = sqlite3.connect(str(path), timeout=BUSY_TIMEOUT_S, isolation_level=None,
                           check_same_thread=False)
    conn.row_factory = sqlite3.Row
    conn.execute('PRAGMA journal_mode=WAL')
    conn.execute('PRAGMA synchronous=FULL')
    return conn


def _create_owner_only(path: Path) -> None:
    """The file holds lease tokens of in-flight acquires: owner-only from its first byte."""
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        os.close(os.open(str(path), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600))
    except FileExistsError:
        pass


def migrate(conn: sqlite3.Connection) -> int:
    """Brings the file to SCHEMA_VERSION in one transaction per step; returns the version found."""
    conn.execute('BEGIN IMMEDIATE')
    try:
        found = conn.execute('PRAGMA user_version').fetchone()[0]
        if found > SCHEMA_VERSION:
            raise DatabaseError('the control database is at schema {}; this daemon knows up to {}'.format(
                found, SCHEMA_VERSION))
        for version in range(found, SCHEMA_VERSION):
            for statement in MIGRATIONS[version]:
                conn.execute(statement)
            conn.execute('PRAGMA user_version = {:d}'.format(version + 1))
        conn.execute('COMMIT')
    except BaseException:
        conn.execute('ROLLBACK')
        raise
    return found


class Database:
    """One SQLite file shared by every thread of a daemon (one connection per thread)."""

    def __init__(self, path: Path):
        self.path = Path(path)
        _create_owner_only(self.path)
        self._local = threading.local()
        self._all: List[sqlite3.Connection] = []
        self._all_lock = threading.Lock()
        self.found_version = migrate(self.conn())

    def conn(self) -> sqlite3.Connection:
        conn = getattr(self._local, 'conn', None)
        if conn is None:
            conn = _connect(self.path)
            self._local.conn = conn
            with self._all_lock:
                self._all.append(conn)
        return conn

    @contextmanager
    def write(self) -> Iterator[sqlite3.Connection]:
        """`BEGIN IMMEDIATE` ... `COMMIT`: the whole block is one atomic, serialized write."""
        conn = self.conn()
        conn.execute('BEGIN IMMEDIATE')
        try:
            yield conn
        except BaseException:
            conn.execute('ROLLBACK')
            raise
        conn.execute('COMMIT')

    def read(self) -> sqlite3.Connection:
        return self.conn()

    def close(self) -> None:
        with self._all_lock:
            for conn in self._all:
                conn.close()
            self._all = []
        self._local = threading.local()
