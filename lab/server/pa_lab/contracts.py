"""Version-1 contracts between SerialPrograms (lab-B), the pa-lab console daemon and its clients.

Slice 0 of docs/design/Agent-Integration.md (fork #131): the JSON documents are frozen here
before anything writes or serves them. `parse(kind, text)` validates one document and returns a
copy rebuilt field by field; unknown keys in open objects are carried over verbatim and listed in
`Parsed.unknown`, so a reader never loses fields a newer writer added. Every failure names the
field: `program.last_transition.source: 'server' is not one of ui, lab, program, error`.

The field tables live in lab/server/README.md; `python3 -m pa_lab describe <kind>` prints the
same tree from the specs below, so the two cannot drift silently.
"""

from __future__ import annotations

import datetime
import json
import re
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple

SUPPORTED_VERSIONS = (1,)

TIMESTAMP_RE = re.compile(r'^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$')
UUID_RE = re.compile(r'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$')
PANEL_ID_RE = re.compile(r'^[A-Za-z0-9_]+:[A-Za-z0-9_]+$')
GAME_ID_RE = re.compile(r'^[A-Za-z][A-Za-z0-9_]*$')
UNKNOWN = 'unknown'


class FieldError:
    def __init__(self, path: str, message: str):
        self.path = path
        self.message = message

    def __str__(self):
        return '{}: {}'.format(self.path, self.message)

    def __repr__(self):
        return 'FieldError({!r}, {!r})'.format(self.path, self.message)


class ContractError(ValueError):
    """Raised with every field-level error found in one document."""

    def __init__(self, kind: str, errors: List[FieldError]):
        self.kind = kind
        self.errors = errors
        super().__init__('{}: {}'.format(kind, '; '.join(str(e) for e in errors)))

    def paths(self) -> List[str]:
        return [e.path for e in self.errors]


class Parsed:
    def __init__(self, kind: str, version: int, data: Dict[str, Any], unknown: List[str]):
        self.kind = kind
        self.version = version
        self.data = data
        self.unknown = unknown


class _Ctx:
    def __init__(self):
        self.errors: List[FieldError] = []
        self.unknown: List[str] = []

    def error(self, path: str, message: str):
        self.errors.append(FieldError(path or '$', message))


def _join(path: str, key: Any) -> str:
    if isinstance(key, int):
        return '{}[{}]'.format(path, key)
    return '{}.{}'.format(path, key) if path else key


def _type_name(value: Any) -> str:
    if value is None:
        return 'null'
    if isinstance(value, bool):
        return 'boolean'
    if isinstance(value, (int, float)):
        return 'number'
    if isinstance(value, str):
        return 'string'
    if isinstance(value, list):
        return 'array'
    return 'object'


# ---- value specs: check(value, path, ctx) returns the normalized copy --------------------------

class Spec:
    label = 'value'

    def check(self, value: Any, path: str, ctx: _Ctx) -> Any:
        raise NotImplementedError

    def describe(self, indent: str) -> List[str]:
        return []


class Str(Spec):
    def __init__(self, pattern: Optional['re.Pattern[str]'] = None, label: str = 'string'):
        self.pattern = pattern
        self.label = label

    def check(self, value, path, ctx):
        if not isinstance(value, str):
            ctx.error(path, 'expected a string, got {}'.format(_type_name(value)))
        elif value == '':
            ctx.error(path, 'must not be empty')
        elif self.pattern is not None and not self.pattern.match(value):
            ctx.error(path, '{!r} is not a valid {}'.format(value, self.label))
        return value


class Int(Spec):
    def __init__(self, minimum: Optional[int] = None, maximum: Optional[int] = None):
        self.minimum = minimum
        self.maximum = maximum
        bounds = [b for b in ('>= {}'.format(minimum) if minimum is not None else '',
                              '<= {}'.format(maximum) if maximum is not None else '') if b]
        self.label = 'integer' + (' ' + ', '.join(bounds) if bounds else '')

    def check(self, value, path, ctx):
        if isinstance(value, bool) or not isinstance(value, int):
            ctx.error(path, 'expected an integer, got {}'.format(_type_name(value)))
        elif self.minimum is not None and value < self.minimum:
            ctx.error(path, '{} is below the minimum {}'.format(value, self.minimum))
        elif self.maximum is not None and value > self.maximum:
            ctx.error(path, '{} is above the maximum {}'.format(value, self.maximum))
        return value


class Num(Spec):
    def __init__(self, minimum: float, maximum: float):
        self.minimum = minimum
        self.maximum = maximum
        self.label = 'number {}..{}'.format(minimum, maximum)

    def check(self, value, path, ctx):
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            ctx.error(path, 'expected a number, got {}'.format(_type_name(value)))
        elif not self.minimum <= value <= self.maximum:
            ctx.error(path, '{} is outside {}..{}'.format(value, self.minimum, self.maximum))
        return value


class Bool(Spec):
    label = 'boolean'

    def check(self, value, path, ctx):
        if not isinstance(value, bool):
            ctx.error(path, 'expected a boolean, got {}'.format(_type_name(value)))
        return value


class Enum(Spec):
    def __init__(self, *values: str):
        self.values = values
        self.label = '|'.join(values)

    def check(self, value, path, ctx):
        if value not in self.values:
            ctx.error(path, '{!r} is not one of {}'.format(value, ', '.join(self.values)))
        return value


class Timestamp(Spec):
    label = 'timestamp (UTC, YYYY-MM-DDTHH:MM:SS.sssZ)'

    def check(self, value, path, ctx):
        if not isinstance(value, str) or not TIMESTAMP_RE.match(value):
            ctx.error(path, '{!r} is not a UTC timestamp like 2026-11-14T06:10:12.124Z'.format(value))
            return value
        try:
            datetime.datetime.strptime(value, '%Y-%m-%dT%H:%M:%S.%fZ')
        except ValueError:
            ctx.error(path, '{!r} is not a real date and time'.format(value))
        return value


class Uuid(Spec):
    label = 'uuid (lower-case, 8-4-4-4-12)'

    def check(self, value, path, ctx):
        if not isinstance(value, str) or not UUID_RE.match(value):
            ctx.error(path, '{!r} is not a lower-case UUID'.format(value))
        return value


class Nullable(Spec):
    def __init__(self, inner: Spec):
        self.inner = inner
        self.label = inner.label + ' | null'

    def check(self, value, path, ctx):
        return None if value is None else self.inner.check(value, path, ctx)

    def describe(self, indent):
        return self.inner.describe(indent)


class ListOf(Spec):
    def __init__(self, item: Spec, min_items: int = 0, max_items: Optional[int] = None):
        self.item = item
        self.min_items = min_items
        self.max_items = max_items
        self.label = 'array of ' + item.label

    def check(self, value, path, ctx):
        if not isinstance(value, list):
            ctx.error(path, 'expected an array, got {}'.format(_type_name(value)))
            return value
        if len(value) < self.min_items:
            ctx.error(path, 'needs at least {} item(s)'.format(self.min_items))
        if self.max_items is not None and len(value) > self.max_items:
            ctx.error(path, 'has {} items, more than {}'.format(len(value), self.max_items))
        return [self.item.check(v, _join(path, i), ctx) for i, v in enumerate(value)]

    def describe(self, indent):
        return self.item.describe(indent)


class AnyObject(Spec):
    """An object this contract does not look inside (program options, results, evidence)."""

    label = 'object (opaque)'

    def check(self, value, path, ctx):
        if not isinstance(value, dict):
            ctx.error(path, 'expected an object, got {}'.format(_type_name(value)))
            return value
        return json.loads(json.dumps(value))


class F:
    """One field of an object. `required` means the key must be present; `nullable` is in the spec."""

    def __init__(self, name: str, spec: Spec, required: bool = True, doc: str = ''):
        self.name = name
        self.spec = spec
        self.required = required
        self.doc = doc


Rule = Callable[[Dict[str, Any], str, _Ctx], None]


class Obj(Spec):
    """An object. Open objects keep unknown keys (forward-compatible readers); closed ones refuse them."""

    label = 'object'

    def __init__(self, fields: Sequence[F], rules: Sequence[Rule] = (), open: bool = True):
        self.fields = {f.name: f for f in fields}
        self.rules = list(rules)
        self.open = open

    def check(self, value, path, ctx):
        if not isinstance(value, dict):
            ctx.error(path, 'expected an object, got {}'.format(_type_name(value)))
            return value
        out: Dict[str, Any] = {}
        errors_before = len(ctx.errors)
        for key, item in value.items():
            field = self.fields.get(key)
            if field is not None:
                out[key] = field.spec.check(item, _join(path, key), ctx)
            elif self.open:
                ctx.unknown.append(_join(path, key))
                out[key] = json.loads(json.dumps(item))
            else:
                ctx.error(_join(path, key), 'unknown field (this object is closed)')
        for field in self.fields.values():
            if field.required and field.name not in value:
                ctx.error(_join(path, field.name), 'required field is missing')
        if len(ctx.errors) == errors_before:
            for rule in self.rules:
                rule(out, path, ctx)
        return out

    def describe(self, indent):
        lines = []
        for f in self.fields.values():
            presence = 'required' if f.required else 'optional'
            doc = '  -- ' + f.doc if f.doc else ''
            lines.append('{}{}: {} ({}){}'.format(indent, f.name, f.spec.label, presence, doc))
            lines.extend(f.spec.describe(indent + '  '))
        if not self.open:
            lines.append(indent + '(closed: unknown fields are refused)')
        return lines


class Document(Spec):
    """A versioned document; also used where one document embeds another (console_state.app)."""

    def __init__(self, kind: str):
        self.kind = kind
        self.label = kind + ' document'

    def check(self, value, path, ctx):
        if not isinstance(value, dict):
            ctx.error(path, 'expected an object, got {}'.format(_type_name(value)))
            return value
        version = _check_version(value, path, ctx)
        if version is None:
            return value
        return _SPECS[self.kind].check(value, path, ctx)

    def describe(self, indent):
        return [indent + '(see `describe {}`)'.format(self.kind)]


def _check_version(value: Dict[str, Any], path: str, ctx: _Ctx) -> Optional[int]:
    schema_path = _join(path, 'schema')
    if 'schema' not in value:
        ctx.error(schema_path, 'required field is missing: every document names its schema version')
        return None
    version = value['schema']
    if isinstance(version, bool) or not isinstance(version, int):
        ctx.error(schema_path, 'expected an integer version, got {}'.format(_type_name(version)))
        return None
    if version not in SUPPORTED_VERSIONS:
        ctx.error(schema_path, 'unsupported version {}; this reader supports {}'.format(
            version, ', '.join(str(v) for v in SUPPORTED_VERSIONS)))
        return None
    return version


# ---- cross-field rules ---------------------------------------------------------------------------

def _when(condition: Callable[[Dict[str, Any]], bool], field: str, must: str,
          test: Callable[[Any], bool]) -> Rule:
    def rule(obj, path, ctx):
        if condition(obj) and not test(obj.get(field)):
            ctx.error(_join(path, field), must)
    return rule


def _is_null(v):
    return v is None


def _not_null(v):
    return v is not None


def _transition_request_id(obj, path, ctx):
    lab = obj.get('source') == 'lab'
    if lab and obj.get('request_id') is None:
        ctx.error(_join(path, 'request_id'), 'required when source is lab (the daemon request it answers)')
    if not lab and obj.get('request_id') is not None:
        ctx.error(_join(path, 'request_id'), 'must be null unless source is lab')


def _observation_known(obj, path, ctx):
    if obj.get('value') == UNKNOWN:
        return
    for key in ('observed_at', 'detector'):
        if obj.get(key) is None:
            ctx.error(_join(path, key), 'required when value is not unknown')


def _no_program_matches_panel(obj, path, ctx):
    is_program = obj.get('panel', {}).get('kind') == 'program'
    no_program = obj.get('program', {}).get('state') == 'no_program'
    if is_program == no_program:
        ctx.error(_join(path, 'program.state'),
                  'no_program exactly when panel.kind is not program (panel.kind={!r})'.format(
                      obj.get('panel', {}).get('kind')))


def _lease_shape(obj, path, ctx):
    state = obj.get('state')
    if state == 'free':
        for key in ('owner', 'acquired_at', 'expires_at'):
            if obj.get(key) is not None:
                ctx.error(_join(path, key), 'must be null while the lease is free')
        return
    for key in ('owner', 'acquired_at'):
        if obj.get(key) is None:
            ctx.error(_join(path, key), 'required while the lease is held')
    if state == 'agent_idle' and obj.get('expires_at') is None:
        ctx.error(_join(path, 'expires_at'), 'required for agent_idle (it has a TTL)')
    if state in ('agent_running', 'user') and obj.get('expires_at') is not None:
        ctx.error(_join(path, 'expires_at'), 'must be null: {} never expires'.format(state))
    if state == 'user' and obj.get('owner') != 'user':
        ctx.error(_join(path, 'owner'), "must be 'user' for a user lease")


def _availability_matches_app(obj, path, ctx):
    available = obj.get('availability', {}).get('state') == 'available'
    if available and obj.get('app') is None:
        ctx.error(_join(path, 'app'), 'required when availability.state is available')
    if not available and obj.get('availability', {}).get('reason') is None:
        ctx.error(_join(path, 'availability.reason'), 'required when the app is unavailable')


SAVE_STALE_REASONS = ('user_lease', 'cfr_batch', 'unhooked_program')


def _save_status(obj, path, ctx):
    status, reason, fact = obj.get('status'), obj.get('reason'), obj.get('fact')
    if status == 'known':
        if fact is None:
            ctx.error(_join(path, 'fact'), 'required when status is known')
        if reason != 'hooked_save':
            ctx.error(_join(path, 'reason'), "must be 'hooked_save' when status is known")
    elif status == 'stale':
        if fact is None:
            ctx.error(_join(path, 'fact'), 'required when status is stale (the last known fact)')
        if reason not in SAVE_STALE_REASONS:
            ctx.error(_join(path, 'reason'), 'must be one of {} when stale'.format(', '.join(SAVE_STALE_REASONS)))
    elif status == 'unknown' and reason != 'no_fact':
        ctx.error(_join(path, 'reason'), "must be 'no_fact' when status is unknown")


def _refusal_detail(obj, path, ctx):
    if obj.get('code') == 'precheck_failed' and not obj.get('check'):
        ctx.error(_join(path, 'check'), 'required for precheck_failed (the failed check name)')
    if obj.get('code') == 'invalid_arguments' and not obj.get('field'):
        ctx.error(_join(path, 'field'), 'required for invalid_arguments (the offending argument)')


def _request_lease(obj, path, ctx):
    if obj.get('tool') == 'acquire_console':
        if 'lease_token' in obj:
            ctx.error(_join(path, 'lease_token'), 'must be absent for acquire_console')
    elif not obj.get('lease_token'):
        ctx.error(_join(path, 'lease_token'), 'required for {}'.format(obj.get('tool')))


def _request_arguments(obj, path, ctx):
    spec = TOOL_ARGUMENTS[obj['tool']]
    obj['arguments'] = spec.check(obj.get('arguments'), _join(path, 'arguments'), ctx)


def _record_status(obj, path, ctx):
    status = obj.get('status')
    want = {
        'pending': dict(completed_at=False, result=False, refusal=False),
        'completed': dict(completed_at=True, result=True, refusal=False),
        'refused': dict(completed_at=True, result=False, refusal=True),
        'failed': dict(completed_at=True, result=False, refusal=True),
    }[status]
    for key, present in want.items():
        if present and obj.get(key) is None:
            ctx.error(_join(path, key), 'required when status is {}'.format(status))
        if not present and obj.get(key) is not None:
            ctx.error(_join(path, key), 'must be null when status is {}'.format(status))


def _event_payload(obj, path, ctx):
    spec = EVENT_PAYLOADS[obj['type']]
    obj['payload'] = spec.check(obj.get('payload'), _join(path, 'payload'), ctx)


def _page_order(obj, path, ctx):
    seqs = [e.get('seq') for e in obj.get('events', [])]
    for i in range(1, len(seqs)):
        if seqs[i] <= seqs[i - 1]:
            ctx.error(_join(_join(path, 'events'), i) + '.seq', 'must be greater than the previous event seq')
    if seqs and obj.get('next_after_seq') != seqs[-1]:
        ctx.error(_join(path, 'next_after_seq'), 'must equal the last event seq ({})'.format(seqs[-1]))


# ---- version-1 specs ----------------------------------------------------------------------------

PROGRAM_STATES = ('stopped', 'running', 'stopping', 'not_ready', 'no_program')
TRANSITION_SOURCES = ('ui', 'lab', 'program', 'error')
CONSOLES = Enum('B')
SCHEMA = F('schema', Int(minimum=1), doc='document version; this reader supports 1')

OBSERVATION = Obj([
    F('value', Str(GAME_ID_RE, 'identifier'), doc="detector result, or 'unknown'"),
    F('observed_at', Nullable(Timestamp()), doc='frame time the detector judged; null while unknown'),
    F('detector', Nullable(Str()), required=False, doc='detector name; required unless unknown'),
    F('confidence', Nullable(Num(0, 1)), required=False),
], rules=[_observation_known])

APP_STATE = Obj([
    SCHEMA,
    F('written_at', Timestamp(), doc='heartbeat: rewritten even when nothing else changed'),
    F('instance', Obj([
        F('id', Uuid(), doc='new on every app start'),
        F('pid', Int(minimum=1)),
        F('profile', Str(), required=False, doc="the --profile name, e.g. 'lab-B'"),
    ])),
    F('panel', Obj([
        F('id', Nullable(Str(PANEL_ID_RE, 'panel id <Game>:<Program>'))),
        F('kind', Enum('program', 'other'), doc='other = settings and every non-program panel'),
    ])),
    F('program', Obj([
        F('state', Enum(*PROGRAM_STATES), doc='legacy state= in lower case'),
        F('run_id', Nullable(Str()), doc='null until run records (#116) exist'),
        F('last_transition', Nullable(Obj([
            F('action', Enum('start', 'stop')),
            F('source', Enum(*TRANSITION_SOURCES), doc='set where the action originates'),
            F('request_id', Nullable(Uuid()), doc='the daemon request; non-null exactly when source is lab'),
            F('at', Timestamp()),
        ], rules=[_transition_request_id])), doc='null until the first start/stop of this instance'),
        F('paused', Nullable(Str()), required=False, doc='legacy paused=: breakpoint label while waiting'),
    ])),
    F('controller', Obj([
        F('state', Enum('ready', 'not_connected', 'no_console'),
          doc='legacy controller= ready | none | (empty)'),
    ])),
    F('video', Obj([
        F('state', Enum('live', 'stale', 'missing'), doc='transport freshness'),
        F('frame_seq', Nullable(Int(minimum=0)), doc='per instance; resets on restart'),
        F('captured_at', Nullable(Timestamp()), doc="that frame's capture time"),
        F('signal', Enum('ok', 'black', 'colour_bars', UNKNOWN), doc="'unknown' until a detector exists"),
    ], rules=[_when(lambda o: o.get('state') == 'live', 'frame_seq', 'required when state is live', _not_null),
              _when(lambda o: o.get('state') == 'live', 'captured_at', 'required when state is live', _not_null)])),
    F('observation', Obj([
        F('game', OBSERVATION),
        F('screen', OBSERVATION),
    ])),
    F('channel', Obj([
        F('last_request', Nullable(Str()), doc='legacy seq=: name of the last inbox request'),
        F('last_error', Nullable(Str()), doc='legacy error=; null when the last request succeeded'),
        F('owner_claimed', Bool(), doc='legacy owner= non-empty; the token itself is not copied'),
    ]), required=False, doc='the legacy file channel (Lab.md); absent in a build without it'),
], rules=[_no_program_matches_panel])

SAVE_FACT = Obj([
    SCHEMA,
    F('game', Str(GAME_ID_RE, 'game id'), doc="program-id prefix, e.g. 'PokemonFRLG'"),
    F('slot', Int(minimum=0), doc='save slot; 0 for single-slot games'),
    F('checkpoint', Nullable(Str()), doc="story checkpoint id like '13.01'; null outside a story"),
    F('position', Nullable(Obj([
        F('map', Str()),
        F('x', Int()),
        F('y', Int()),
        F('facing', Enum('up', 'down', 'left', 'right', UNKNOWN), required=False),
    ]))),
    F('saved_at', Timestamp()),
    F('run_id', Nullable(Str())),
    F('producer', Obj([
        F('program', Str(PANEL_ID_RE, 'program id')),
        F('build', Str(), doc='git describe or commit of the app build'),
    ])),
])

LEASE = Obj([
    F('state', Enum('free', 'agent_idle', 'agent_running', 'user')),
    F('owner', Nullable(Str()), doc="descriptive, e.g. 'agent:frlg-lane'; 'user' for a user lease"),
    F('acquired_at', Nullable(Timestamp())),
    F('expires_at', Nullable(Timestamp()), doc='agent_idle only'),
], rules=[_lease_shape])

REFUSAL_CODES = (
    'invalid_arguments', 'request_id_reused', 'console_not_allowed',
    'lease_required', 'lease_invalid', 'lease_expired', 'console_held_by_user', 'console_held_by_agent',
    'app_unavailable', 'unsupported_schema',
    'unknown_panel', 'panel_not_program', 'panel_hidden',
    'program_running', 'program_not_stopped', 'controller_not_ready',
    'video_stale', 'video_bad_signal', 'timeout',
    'prechecks_unsupported', 'precheck_failed', 'preflight_stale',
)

REFUSAL = Obj([
    F('code', Enum(*REFUSAL_CODES)),
    F('message', Str(), doc='one line for the user'),
    F('check', Str(), required=False, doc='precheck name, for precheck_failed'),
    F('field', Str(), required=False, doc='argument path, for invalid_arguments'),
    F('evidence', AnyObject(), required=False),
], rules=[_refusal_detail])

CONSOLE_STATE = Obj([
    SCHEMA,
    F('console', CONSOLES),
    F('observed_at', Timestamp(), doc='when the daemon read Lab/state.json'),
    F('availability', Obj([
        F('state', Enum('available', 'unavailable')),
        F('reason', Nullable(Enum('app_not_running', 'no_state_file', 'malformed', 'unsupported_schema',
                                  'stale_heartbeat'))),
    ])),
    F('app', Nullable(Document('app_state')), doc='Lab/state.json exactly as parsed; never edited'),
    F('lease', LEASE),
    F('blocked', ListOf(REFUSAL), doc='refusals a leased run_program would get right now'),
], rules=[_availability_matches_app])

SAVE_RESOURCE = Obj([
    SCHEMA,
    F('console', CONSOLES),
    F('status', Enum('known', 'stale', UNKNOWN)),
    F('reason', Enum('hooked_save', 'no_fact', *SAVE_STALE_REASONS)),
    F('since', Nullable(Timestamp()), doc='when status last changed'),
    F('fact', Nullable(Document('save_fact')), doc='Lab/save.json exactly as parsed'),
], rules=[_save_status])

PANEL_ARG = F('panel', Str(PANEL_ID_RE, 'panel id <Game>:<Program>'))
TTL = F('ttl_s', Int(minimum=60, maximum=3600), required=False, doc='default 1200 (open question 2)')

TOOL_ARGUMENTS: Dict[str, Spec] = {
    'acquire_console': Obj([F('owner', Str()), TTL], open=False),
    'renew_console': Obj([TTL], open=False),
    'release_console': Obj([], open=False),
    'preflight_program': Obj([PANEL_ARG, F('options', AnyObject())], open=False),
    'run_program': Obj([PANEL_ARG, F('options', AnyObject()), F('preflight_token', Str())], open=False),
    'stop_program': Obj([], open=False),
    'cfr_batch': Obj([F('lines', ListOf(Str(), min_items=1, max_items=200))], open=False),
}

REQUEST = Obj([
    SCHEMA,
    F('request_id', Uuid(), doc='client-generated; (client_id, tool, request_id) is the idempotency key'),
    F('client_id', Str()),
    F('tool', Enum(*TOOL_ARGUMENTS)),
    F('console', CONSOLES),
    F('lease_token', Str(), required=False, doc='required for every tool except acquire_console'),
    F('arguments', AnyObject(), doc='checked against the tool (closed objects)'),
], rules=[_request_lease, _request_arguments], open=False)

REQUEST_RECORD = Obj([
    SCHEMA,
    F('request_id', Uuid()),
    F('client_id', Str()),
    F('tool', Enum(*TOOL_ARGUMENTS)),
    F('status', Enum('pending', 'completed', 'refused', 'failed'),
      doc='refused = before acting; failed = after acting'),
    F('created_at', Timestamp()),
    F('completed_at', Nullable(Timestamp())),
    F('arguments', AnyObject(), doc='normalized; never holds the lease token'),
    F('result', Nullable(AnyObject())),
    F('refusal', Nullable(REFUSAL)),
], rules=[_record_status])

REFUSAL_DOC = Obj([
    SCHEMA,
    F('request_id', Nullable(Uuid()), doc='null for reads, which carry no request id'),
    F('tool', Str()),
    F('refusal', REFUSAL),
])

EVENT_PAYLOADS: Dict[str, Spec] = {
    'program_finished': Obj([F('run_id', Nullable(Str())), F('verdict', Enum('passed', 'failed', 'stopped')),
                             F('checkpoint', Nullable(Str()))]),
    'stopped': Obj([F('source', Enum(*TRANSITION_SOURCES)), F('run_id', Nullable(Str()), required=False)]),
    'video_lost': Obj([F('last_frame_seq', Nullable(Int(minimum=0)))]),
    'save': Obj([F('save', Document('save_resource'))]),
    'lease_changed': Obj([F('old', LEASE), F('new', LEASE)]),
    'request_completed': Obj([F('request_id', Uuid()), F('tool', Enum(*TOOL_ARGUMENTS)),
                              F('result', AnyObject())]),
    'request_failed': Obj([F('request_id', Uuid()), F('tool', Enum(*TOOL_ARGUMENTS)), F('refusal', REFUSAL)]),
}
EVENT_PAYLOADS['program_failed'] = EVENT_PAYLOADS['program_finished']
EVENT_PAYLOADS['video_back'] = EVENT_PAYLOADS['video_lost']
EVENT_PAYLOADS['autosave'] = EVENT_PAYLOADS['save']

EVENT = Obj([
    SCHEMA,
    F('seq', Int(minimum=1), doc='64-bit, strictly increasing, never reused'),
    F('type', Enum(*EVENT_PAYLOADS)),
    F('at', Timestamp()),
    F('console', CONSOLES),
    F('instance_id', Nullable(Uuid()), doc='app instance the event came from'),
    F('request_id', Nullable(Uuid())),
    F('payload', AnyObject(), doc='checked against the type'),
], rules=[_event_payload])

EVENT_PAGE = Obj([
    SCHEMA,
    F('events', ListOf(Document('event'))),
    F('next_after_seq', Int(minimum=0), doc='cursor for the next next_events call'),
], rules=[_page_order])

_SPECS: Dict[str, Obj] = {
    'app_state': APP_STATE,
    'save_fact': SAVE_FACT,
    'console_state': CONSOLE_STATE,
    'save_resource': SAVE_RESOURCE,
    'request': REQUEST,
    'request_record': REQUEST_RECORD,
    'refusal': REFUSAL_DOC,
    'event': EVENT,
    'event_page': EVENT_PAGE,
}

KINDS: Tuple[str, ...] = tuple(_SPECS)

# Who writes each kind. The daemon never edits an app-authored document; its resources embed one
# whole (console_state.app, save_resource.fact) and add their own fields beside it.
AUTHORS: Dict[str, str] = {
    'app_state': 'app (Lab/state.json)',
    'save_fact': 'app (Lab/save.json, written by a program that saved)',
    'console_state': 'daemon (console://B/state)',
    'save_resource': 'daemon (console://B/save)',
    'request': 'client (mutating tool call)',
    'request_record': 'daemon (request status / idempotent retry result)',
    'refusal': 'daemon (tool error result)',
    'event': 'daemon (SQLite events row, JSONL export)',
    'event_page': 'daemon (next_events result)',
}


# ---- entry points --------------------------------------------------------------------------------

def validate(kind: str, value: Any) -> Parsed:
    if kind not in _SPECS:
        raise ValueError('unknown kind {!r}; kinds: {}'.format(kind, ', '.join(KINDS)))
    ctx = _Ctx()
    if not isinstance(value, dict):
        ctx.error('$', 'expected a JSON object, got {}'.format(_type_name(value)))
        raise ContractError(kind, ctx.errors)
    version = _check_version(value, '', ctx)
    if version is None:
        raise ContractError(kind, ctx.errors)
    data = _SPECS[kind].check(value, '', ctx)
    if ctx.errors:
        raise ContractError(kind, ctx.errors)
    return Parsed(kind, version, data, ctx.unknown)


def parse(kind: str, text: Any) -> Parsed:
    if isinstance(text, bytes):
        try:
            text = text.decode('utf-8')
        except UnicodeDecodeError as e:
            raise ContractError(kind, [FieldError('$', 'not UTF-8: {}'.format(e))])
    try:
        value = json.loads(text)
    except ValueError as e:
        raise ContractError(kind, [FieldError('$', 'malformed JSON: {}'.format(e))])
    return validate(kind, value)


def dump(parsed: Parsed) -> str:
    return json.dumps(parsed.data, indent=2, ensure_ascii=False) + '\n'


def describe(kind: str) -> List[str]:
    return ['{}  [{}]'.format(kind, AUTHORS[kind])] + _SPECS[kind].describe('  ')
