"""Read-only view of one console's `Lab/` folder: `console://B/state` and `console://B/save`.

Slice 2 of docs/design/Agent-Integration.md (fork #131). The app writes `Lab/state.json` (and a
program that saved writes `Lab/save.json`); this module parses them strictly and builds the
daemon's resources around them, never editing the app's document. Anything it cannot trust
(no file, a torn or malformed file, an unsupported schema, another profile's app, a dead pid, a
stale heartbeat) is an explicit `unavailable` state with the reason, never a guess.

With a control plane (Slice 3, `pa_lab.control`) the lease comes from its SQLite store, and every
read of a live app is applied to it first (`observe`: UI takeover, run end, `stopped` events).
Without one (Slice 2) the lease is only reported: the app's last transition from a UI Start/Stop
means the user holds the console, with no timeout.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from pa_lab import contracts, timestamps

MAX_FILE_BYTES = 1 << 20

# The app rewrites state.json every 500 ms, but not while it runs a channel request (a `walk`
# can take 5 s), so the heartbeat is judged against a generous limit.
HEARTBEAT_TIMEOUT_S = 10.0

BAD_SIGNALS = ('black', 'colour_bars')


class Unavailable(Exception):
    """A file the daemon cannot use. `reason` is a console_state availability.reason."""

    def __init__(self, reason: str, message: str, evidence: Optional[Dict[str, Any]] = None):
        super().__init__(message)
        self.reason = reason
        self.message = message
        self.evidence = evidence

    def refusal(self) -> Dict[str, Any]:
        out = {'code': 'unsupported_schema' if self.reason == 'unsupported_schema' else 'app_unavailable',
               'message': self.message}
        if self.evidence is not None:
            out['evidence'] = self.evidence
        return out


def _no_duplicate_keys(pairs):
    out = {}
    for key, value in pairs:
        if key in out:
            raise ValueError('duplicate key {!r}'.format(key))
        out[key] = value
    return out


def _no_constants(name):
    raise ValueError('{} is not JSON'.format(name))


def load_strict(path: Path, kind: str) -> Dict[str, Any]:
    """One read of a whole file, parsed strictly and checked against the version-1 contract.

    Raises Unavailable with reason no_state_file, malformed or unsupported_schema.
    """
    name = 'Lab/' + path.name
    try:
        with open(path, 'rb') as f:
            data = f.read(MAX_FILE_BYTES + 1)
    except FileNotFoundError:
        raise Unavailable('no_state_file', '{} does not exist.'.format(name))
    except OSError as e:
        raise Unavailable('malformed', '{} cannot be read: {}.'.format(name, e.strerror))
    if len(data) > MAX_FILE_BYTES:
        raise Unavailable('malformed', '{} is larger than {} bytes.'.format(name, MAX_FILE_BYTES))
    try:
        value = json.loads(data.decode('utf-8'), object_pairs_hook=_no_duplicate_keys,
                           parse_constant=_no_constants)
    except (UnicodeDecodeError, ValueError) as e:
        raise Unavailable('malformed', '{} is not complete JSON (a partial write?): {}.'.format(name, e))
    if isinstance(value, dict) and 'schema' in value and value['schema'] not in contracts.SUPPORTED_VERSIONS \
            and isinstance(value['schema'], int) and not isinstance(value['schema'], bool):
        raise Unavailable('unsupported_schema', '{} has schema {}; this daemon reads {}.'.format(
            name, value['schema'], ', '.join(str(v) for v in contracts.SUPPORTED_VERSIONS)),
            {'schema': value['schema']})
    try:
        return contracts.validate(kind, value).data
    except contracts.ContractError as e:
        raise Unavailable('malformed', '{} breaks the {} contract: {}.'.format(
            name, kind, '; '.join(str(err) for err in e.errors)), {'fields': e.paths()})


def pid_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def _clock_time(at: str) -> str:
    return at[11:16]


class ConsoleReader:
    """Reads one allowlisted console's `Lab/` folder. Safe to share between threads: it keeps no
    state between calls except the last app instance it saw."""

    def __init__(self, console: str, lab_dir: Path, profile: Optional[str] = None,
                 now: Callable[[], float] = timestamps.wall_clock,
                 pid_alive: Callable[[int], bool] = pid_alive,
                 heartbeat_timeout_s: float = HEARTBEAT_TIMEOUT_S,
                 control: Optional[Any] = None):
        self.console = console
        self.control = control
        self.lab_dir = Path(lab_dir)
        self.profile = profile
        self.now = now
        self.pid_alive = pid_alive
        self.heartbeat_timeout_s = heartbeat_timeout_s

    @property
    def state_path(self) -> Path:
        return self.lab_dir / 'state.json'

    @property
    def save_path(self) -> Path:
        return self.lab_dir / 'save.json'

    def app_state(self) -> Dict[str, Any]:
        """The parsed `Lab/state.json` of a live app, or Unavailable."""
        app = load_strict(self.state_path, 'app_state')
        profile = app['instance'].get('profile')
        if self.profile is not None and profile is not None and profile != self.profile:
            raise Unavailable('malformed', 'Lab/state.json is from profile {!r}, not {!r}.'.format(
                profile, self.profile), {'profile': profile})
        pid = app['instance']['pid']
        if not self.pid_alive(pid):
            raise Unavailable('app_not_running', 'The app that wrote Lab/state.json (pid {}) is not running.'.format(
                pid), {'pid': pid, 'instance_id': app['instance']['id']})
        age = self.now() - timestamps.parse(app['written_at'])
        if age > self.heartbeat_timeout_s:
            raise Unavailable('stale_heartbeat', 'Lab/state.json was last written {:.1f} s ago (limit {:g} s).'.format(
                age, self.heartbeat_timeout_s), {'written_at': app['written_at'],
                                                 'instance_id': app['instance']['id']})
        return app

    def console_state(self) -> Dict[str, Any]:
        observed_at = timestamps.format(self.now())
        try:
            app = self.app_state()
        except Unavailable as e:
            lease = self.control.lease(self.console) if self.control is not None else free_lease()
            doc = {'schema': 1, 'console': self.console, 'observed_at': observed_at,
                   'availability': {'state': 'unavailable', 'reason': e.reason},
                   'app': None, 'lease': lease, 'blocked': [_app_unavailable(e)]}
            return contracts.validate('console_state', doc).data
        lease = self.control.observe(self.console, app) if self.control is not None else reported_lease(app)
        doc = {'schema': 1, 'console': self.console, 'observed_at': observed_at,
               'availability': {'state': 'available', 'reason': None},
               'app': app, 'lease': lease, 'blocked': blocked(self.console, app, lease)}
        return contracts.validate('console_state', doc).data

    def save_resource(self) -> Dict[str, Any]:
        """`console://B/save`: the last program-authored save fact and whether it still holds.

        A save.json that exists but cannot be read raises Unavailable: it is not "no fact".
        """
        if not self.save_path.exists():
            doc = {'schema': 1, 'console': self.console, 'status': 'unknown', 'reason': 'no_fact',
                   'since': None, 'fact': None}
            return contracts.validate('save_resource', doc).data
        fact = load_strict(self.save_path, 'save_fact')
        doc = {'schema': 1, 'console': self.console, 'status': 'known', 'reason': 'hooked_save',
               'since': fact['saved_at'], 'fact': fact}
        try:
            app = self.app_state()
        except Unavailable:
            app = None
        transition = app['program']['last_transition'] if app is not None else None
        if transition is not None and transition['source'] == 'ui' and \
                timestamps.parse(transition['at']) > timestamps.parse(fact['saved_at']):
            doc.update(status='stale', reason='user_lease', since=transition['at'])
        return contracts.validate('save_resource', doc).data


def _app_unavailable(e: Unavailable) -> Dict[str, Any]:
    out = {'code': 'app_unavailable', 'message': e.message}
    if e.evidence is not None:
        out['evidence'] = e.evidence
    return out


def free_lease() -> Dict[str, Any]:
    return {'state': 'free', 'owner': None, 'acquired_at': None, 'expires_at': None}


def reported_lease(app: Dict[str, Any]) -> Dict[str, Any]:
    """The lease as far as this instance's state shows it: a UI Start/Stop hands the console to
    the user, with no timeout. Without the Slice 3 store a user lease from before an app restart
    is not remembered."""
    transition = app['program']['last_transition']
    if transition is not None and transition['source'] == 'ui':
        return {'state': 'user', 'owner': 'user', 'acquired_at': transition['at'], 'expires_at': None}
    return free_lease()


def blocked(console: str, app: Dict[str, Any], lease: Dict[str, Any]) -> List[Dict[str, Any]]:
    """The refusals a leased run_program would get right now, in the order it would check them."""
    out = []
    if lease['state'] == 'user':
        transition = app['program']['last_transition']
        if transition is not None and transition['source'] == 'ui' and transition['at'] == lease['acquired_at']:
            message = 'The user clicked {} in the app at {}; console {} is theirs until they release it.'.format(
                transition['action'].capitalize(), _clock_time(transition['at']), console)
        else:
            message = 'The user holds console {} since {}; it is theirs until they release it.'.format(
                console, _clock_time(lease['acquired_at']))
        out.append({'code': 'console_held_by_user', 'message': message})
    state = app['program']['state']
    panel = app['panel']['id']
    if state == 'no_program':
        out.append({'code': 'panel_not_program', 'message': 'The selected panel is not a program.'})
    elif state in ('running', 'stopping'):
        out.append({'code': 'program_running', 'message': '{} is {}.'.format(panel, state)})
    elif state == 'not_ready':
        out.append({'code': 'program_not_stopped', 'message': '{} is still loading.'.format(panel)})
    if app['controller']['state'] != 'ready' and state != 'no_program':
        out.append({'code': 'controller_not_ready', 'message': 'The controller is {}.'.format(
            app['controller']['state'].replace('_', ' '))})
    video = app['video']
    if video['state'] != 'live':
        out.append({'code': 'video_stale', 'message': 'The video is {}.'.format(video['state'])})
    elif video['signal'] in BAD_SIGNALS:
        out.append({'code': 'video_bad_signal', 'message': 'The video shows {}.'.format(
            video['signal'].replace('_', ' '))})
    return out
