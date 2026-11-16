"""The app channel as the daemon's core sees it: typed commands in, typed answers out.

The daemon talks to one SerialPrograms instance per console through a channel: it sends a request
(one or more commands), the app takes it, runs it and answers with a journal, and separately keeps
a state document (`app_state`) and, after a hooked save, a save fact (`save_fact`) current. The
version-1 wire format is `contracts/v1/channel.md`; `pa_lab.channel_file` implements it over the
`Lab/` folder and is the only module that knows file names, command text or journal line text.

Everything here is transport-free: actions, `shot` and `app_query` use only these types, so a test
can drive them with an in-memory channel (`tests/fake_channel.py`).
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable, Dict, FrozenSet, Optional, Tuple, Type

__all__ = ['AppChannel', 'AppError', 'Command', 'ConsoleLine', 'DescribeProgram', 'ErrorKind', 'GameOpen', 'Journal',
           'Preflight', 'SetOption', 'Snapshot', 'Start', 'Stop', 'await_answer']


# ---- commands: what the daemon asks for (never caller text)

class Command:
    """A command the daemon may send. Each kind is a frozen dataclass below."""


@dataclass(frozen=True)
class Start(Command):
    """Start the selected program; the app waits up to `controller_wait_s` for the controller."""
    controller_wait_s: int


@dataclass(frozen=True)
class Stop(Command):
    """Stop the running program."""


@dataclass(frozen=True)
class Snapshot(Command):
    """Save the first frame after the command starts, waiting up to `timeout_ms`."""
    label: str
    timeout_ms: int


@dataclass(frozen=True)
class DescribeProgram(Command):
    """Print the selected program's id and option contract."""


@dataclass(frozen=True)
class GameOpen(Command):
    """Print whether a game is in front."""


@dataclass(frozen=True)
class SetOption(Command):
    """Set one option of the selected program (`key` walks into groups with dots) to a JSON value."""
    key: str
    value: Any


@dataclass(frozen=True)
class Preflight(Command):
    """Stage `options`, run the program's start checks without starting it, restore the options."""
    options: Dict[str, Any]


@dataclass(frozen=True)
class ConsoleLine(Command):
    """One console command, already checked by `pa_lab.cfr`'s allowlist (the cfr_batch contract)."""
    line: str


# ---- answers

class ErrorKind:
    """Why the app refused a command, as far as the daemon tells the cases apart."""
    OWNER_CLAIMED = 'owner_claimed'            # a lab script holds the channel's owner lock
    PANEL_NOT_PROGRAM = 'panel_not_program'    # the selected panel is not a program
    CONTROLLER_NOT_READY = 'controller_not_ready'
    PROGRAM_STATE = 'program_state'            # the program is not in the state the command needs
    OTHER = 'other'


@dataclass(frozen=True)
class AppError:
    """The first failed command of a request. `detail` is the app's message (it may name an owner
    token: never put it in a record for OWNER_CLAIMED); `program_state` is set for PROGRAM_STATE
    (upper case, as the app prints it: RUNNING, STOPPING, ...); `command_index` is the failed
    command's 0-based place among the commands sent, when the app names it."""
    kind: str
    detail: str
    program_state: Optional[str] = None
    command_index: Optional[int] = None


@dataclass(frozen=True)
class Journal:
    """A finished answer. `lines` is the raw journal, kept only as evidence for refusals;
    `succeeded` holds the kinds of command that ran; `printed` the reply lines commands printed."""
    lines: Tuple[str, ...]
    succeeded: FrozenSet[Type[Command]] = frozenset()
    error: Optional[AppError] = None
    printed: Tuple[str, ...] = field(default=())

    def ok(self, kind: Type[Command]) -> bool:
        return kind in self.succeeded


class AppChannel:
    """One app's channel. A request is named by the daemon (`request_name`) so a retry or a later
    read can find it; names are opaque to the core."""

    def request_name(self, label: str, request_id: str, unique: bool = False) -> str:
        """A name for one request; `unique` adds a random part so an old answer of a reused
        request id never answers a new attempt."""
        raise NotImplementedError

    def send(self, name: str, request_id: str, commands: Tuple[Command, ...]) -> None:
        """Hands the request to the app at once (atomically). Raises OSError when it cannot."""
        raise NotImplementedError

    def answer(self, name: str) -> Optional[Journal]:
        """The request's finished journal, or None while there is none."""
        raise NotImplementedError

    def taken(self, name: str) -> bool:
        """True while the channel shows the app took the request (it may still act on it)."""
        raise NotImplementedError

    def withdraw(self, name: str) -> bool:
        """True if the app had not taken the request and it is now withdrawn (nothing ran)."""
        raise NotImplementedError

    def read_state(self, limit: int) -> Optional[bytes]:
        """The app_state document's bytes (at most `limit` + 1), or None when there is none.
        Raises OSError when it cannot be read."""
        raise NotImplementedError

    def read_save(self, limit: int) -> Optional[bytes]:
        """The save_fact document's bytes, like read_state."""
        raise NotImplementedError

    def snapshot_path(self, name: str, label: str) -> str:
        """Where a Snapshot of request `name` saves its PNG."""
        raise NotImplementedError

    state_label = 'app state'
    save_label = 'save fact'


def await_answer(channel: AppChannel, name: str, now: Callable[[], float], sleep: Callable[[float], None],
                 deadline: float, poll_s: float) -> Optional[Journal]:
    """Polls for the answer until `deadline`; on timeout withdraws the request and returns None."""
    while True:
        journal = channel.answer(name)
        if journal is not None:
            return journal
        if now() >= deadline:
            break
        sleep(poll_s)
    channel.withdraw(name)
    return None
