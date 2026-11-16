"""`preflight_program`'s pure parts (Slice 5b of fork #131): candidate options against the
program's static contract, the app's `preflight` reply, and the token's binding.

**Options.** `check_options(contract, options)` checks a candidate against `describe_program`'s
option entries (Lab.md "Describe program") before anything is sent: every key must be one the
program saves, a value must have the entry's type and lie within its bounds, an enum/select value
must be an enabled slug, a group's value is an object checked against the group's own entries
(plus `Enabled` when the group is toggleable). A `type: other` option cannot be staged. Dynamic
visibility and `check_validity` depend on the other options' values; only the app can evaluate
them, on the staged candidate.

**The app command** (the app side is fork #601, Lab.md "Preflight"): `preflight <JSON>` runs on
the app's LabChannel worker. It stages the candidate options of the selected program on the UI
thread (through `run_on_ui`, as `set` does), rejects unknown keys and disabled values, evaluates
`check_validity`, runs the program's prechecks in order on the worker without starting it, and
restores the prior options on the UI thread again. When the options no longer equal the staged
candidate at restore time (the user edited the panel during the checks), it leaves them alone and
replies `restored: false`. It prints one line, `preflight <JSON>`:

    {"schema": 1, "id": "<Game>:<Program>", "verdict": "passed" | "failed" | "invalid_options" | "unsupported" | "cancelled",
     "checks": [{"name", "status": "passed" | "failed" | "skipped", "message"}],
     "failure": {"check", "message", "evidence"?} | null,      # verdict failed
     "invalid": {"key", "message"} | null,                     # verdict invalid_options
     "restored": true | false,
     "edited_keys": ["<key>", ...],                            # optional: keys a user edited during the checks
     "options_hash": "<sha256>" | null}                         # optional: the program's full options after the restore

**The token** is the hash of what the preflight was checked against (`binding`): the app instance,
the program id, the candidate's hash (`candidate_hash`), the app's hash of every option the program
holds (`options_hash`: the reply's, else `program.options_hash` in state.json; None from an app
before #601), the observation versions a run depends on (the last
start/stop transition, `console://B/save`'s status and the newest `cfr_batch`) and the lease (its token's hash), plus a
random salt so it cannot be guessed. The daemon stores the binding under the token's hash;
`run_program` recomputes it from what it reads now and refuses a token whose binding changed
(`preflight_stale`, `evidence.changed`) or that is older than `TOKEN_TTL_S`.
"""

from __future__ import annotations

import hashlib
import json
import re
import secrets
from typing import Any, Dict, List, Optional

from pa_lab.channel import SetOption
from pa_lab.control import Refused

TOKEN_TTL_S = 120
PREFLIGHT_RE = re.compile(r'^preflight (\{.*\})$')
VERDICTS = ('passed', 'failed', 'invalid_options', 'unsupported', 'cancelled')
CHECK_STATUSES = ('passed', 'failed', 'skipped')
BINDING_FIELDS = ('console', 'instance_id', 'panel', 'candidate_hash', 'options_hash', 'transition', 'save', 'lease')

__all__ = ['check_options', 'candidate_hash', 'held_options_hash', 'set_commands', 'parse_reply', 'binding', 'new_token', 'changed',
           'TOKEN_TTL_S']


def _json(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False)


def candidate_hash(options: Dict[str, Any]) -> str:
    """The hash of the caller's candidate (only the options it names)."""
    return hashlib.sha256(_json(options).encode('utf-8')).hexdigest()


def held_options_hash(app: Dict[str, Any]) -> Optional[str]:
    """`program.options_hash` in state.json: the app's hash of every option the selected program holds
    (#601); None from an app that does not report it."""
    value = app['program'].get('options_hash')
    return value if isinstance(value, str) else None


# ---- candidate options against the static contract

def _bad(path: str, message: str) -> Refused:
    return Refused('invalid_arguments', '{}: {}.'.format(path, message), field=path)


def _number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _check_value(entry: Dict[str, Any], value: Any, path: str) -> None:
    kind = entry.get('type')
    if kind == 'boolean':
        if not isinstance(value, bool):
            raise _bad(path, 'expected true or false')
    elif kind == 'string':
        if not isinstance(value, str):
            raise _bad(path, 'expected a string')
    elif kind in ('integer', 'number'):
        if kind == 'integer' and not (isinstance(value, int) and not isinstance(value, bool)):
            raise _bad(path, 'expected an integer')
        if not _number(value):
            raise _bad(path, 'expected a number')
        low, high = entry.get('min'), entry.get('max')
        if (low is not None and value < low) or (high is not None and value > high):
            raise _bad(path, '{} is outside {}..{}'.format(value, '' if low is None else low,
                                                          '' if high is None else high))
    elif kind in ('enum', 'select'):
        slugs = {e.get('slug'): e.get('enabled') for e in entry.get('entries') or []}
        if not isinstance(value, str) or value not in slugs:
            raise _bad(path, 'expected one of {}'.format(', '.join(str(s) for s in slugs)))
        if slugs[value] is not True:
            raise _bad(path, "'{}' is disabled in this program".format(value))
    elif kind == 'group':
        if not isinstance(value, dict):
            raise _bad(path, 'expected an object (a group of options)')
        entries = list(entry.get('options') or [])
        if entry.get('toggleable') and not any(e.get('key') == 'Enabled' for e in entries):
            entries.append({'key': 'Enabled', 'type': 'boolean'})
        _check_object(entries, value, path)
    else:
        raise _bad(path, 'the program contract does not describe this option (type {}); it cannot be staged'.format(
            kind))


def _check_object(entries: List[Dict[str, Any]], options: Dict[str, Any], path: str) -> None:
    by_key = {e.get('key'): e for e in entries}
    for key, value in options.items():
        sub = '{}.{}'.format(path, key)
        if key not in by_key:
            raise _bad(sub, 'unknown option; the program saves {}'.format(', '.join(str(k) for k in by_key)))
        _check_value(by_key[key], value, sub)


def check_options(program: Dict[str, Any], options: Dict[str, Any]) -> None:
    """Raises Refused(invalid_arguments, field=options.<key path>) at the first bad option."""
    _check_object(program['options'], options, 'options')


def set_commands(options: Dict[str, Any], prefix: str = '') -> List[SetOption]:
    """The app commands that load `options`, in key order (a record keeps them as canonical JSON);
    an object value is a group (key `GROUP.KEY`). Only for options `check_options` passed (no
    `type: other`)."""
    out: List[SetOption] = []
    for key, value in options.items():
        if isinstance(value, dict):
            out.extend(set_commands(value, prefix + key + '.'))
        else:
            out.append(SetOption(key=prefix + key, value=value))
    return out


# ---- the app's reply

def _reply_error(message: str, lines: List[str]) -> Refused:
    return Refused('app_unavailable', 'The app\'s preflight reply is unusable: {}.'.format(message),
                   evidence={'journal': lines})


def parse_reply(printed: List[str], lines: List[str]) -> Dict[str, Any]:
    """The one `preflight <JSON>` line among a journal's printed lines, checked; Refused(app_unavailable)
    otherwise. `lines` is the whole journal, the evidence."""
    matches = [PREFLIGHT_RE.match(line) for line in printed]
    matches = [m for m in matches if m]
    if len(matches) != 1:
        raise _reply_error('expected one preflight line, found {}'.format(len(matches)), lines)
    try:
        doc = json.loads(matches[0].group(1))
    except ValueError:
        raise _reply_error('invalid JSON', lines) from None
    if not isinstance(doc, dict) or doc.get('schema') != 1:
        raise _reply_error('schema must be 1', lines)
    if not isinstance(doc.get('id'), str) or doc.get('verdict') not in VERDICTS or \
            not isinstance(doc.get('restored'), bool) or not isinstance(doc.get('checks'), list):
        raise _reply_error('id, verdict, checks and restored are required', lines)
    for check in doc['checks']:
        if not isinstance(check, dict) or not isinstance(check.get('name'), str) or \
                check.get('status') not in CHECK_STATUSES or not isinstance(check.get('message', ''), str):
            raise _reply_error('a check needs a name, a status and a message', lines)
    failure = doc.get('failure')
    if doc['verdict'] == 'failed' and not (isinstance(failure, dict) and isinstance(failure.get('check'), str)
                                           and isinstance(failure.get('message'), str)):
        raise _reply_error('a failed verdict needs failure.check and failure.message', lines)
    invalid = doc.get('invalid')
    if doc['verdict'] == 'invalid_options' and not (isinstance(invalid, dict) and isinstance(invalid.get('key'), str)
                                                    and isinstance(invalid.get('message'), str)):
        raise _reply_error('an invalid_options verdict needs invalid.key and invalid.message', lines)
    if doc.get('options_hash') is not None and not isinstance(doc['options_hash'], str):
        raise _reply_error('options_hash must be a string or null', lines)
    return doc


def outcome(doc: Dict[str, Any], panel: str, lines: List[str]) -> Optional[Dict[str, Any]]:
    """The refusal a reply means, or None when every check passed and the options were restored."""
    if not doc['restored']:
        evidence = {'reason': 'options_not_restored', 'reply': doc}
        if isinstance(doc.get('edited_keys'), list) and doc['edited_keys']:
            # #601: someone changed these in the panel during the checks; the app kept their edit.
            evidence['edited_keys'] = doc['edited_keys']
        return {'code': 'app_unavailable', 'message': 'The app did not restore the prior options after the preflight; '
                'check the panel before any run.', 'evidence': evidence}
    if doc['verdict'] == 'cancelled':
        # #616: a panel change (or the app closing) cancelled the checks; the options were put back.
        return {'code': 'app_unavailable', 'message': 'The app cancelled the checks of {} before they finished (the '
                'panel changed or the app is closing); run preflight_program again.'.format(doc['id']),
                'evidence': {'reason': 'cancelled', 'reply': doc}}
    if doc['id'] != panel:
        return {'code': 'app_unavailable', 'message': 'The app preflighted {}, not {}.'.format(doc['id'], panel),
                'evidence': {'reason': 'panel_changed', 'reply': doc}}
    if doc['verdict'] == 'unsupported':
        return {'code': 'prechecks_unsupported', 'message': '{} declares no start checks; the server does not run a '
                'program whose checks it cannot run.'.format(panel)}
    if doc['verdict'] == 'invalid_options':
        key = doc['invalid']['key']
        return {'code': 'invalid_arguments', 'field': 'options.' + key,
                'message': 'The app refused option {}: {}'.format(key, doc['invalid']['message'])}
    if doc['verdict'] == 'failed':
        failure = doc['failure']
        refusal = {'code': 'precheck_failed', 'check': failure['check'], 'message': failure['message']}
        if isinstance(failure.get('evidence'), dict):
            refusal['evidence'] = failure['evidence']
        return refusal
    if any(check['status'] == 'failed' for check in doc['checks']):
        return {'code': 'app_unavailable', 'message': 'The app said passed but lists a failed check.',
                'evidence': {'journal': lines}}
    return None


# ---- the token

def transition_key(app: Dict[str, Any]) -> Optional[str]:
    t = app['program']['last_transition']
    return None if t is None else '{}|{}|{}|{}'.format(t['action'], t['source'], t['request_id'], t['at'])


def binding(state: Dict[str, Any], save: Dict[str, Any], panel: str, options: Dict[str, Any],
            lease_token_hash: Optional[str], held_hash: Optional[str]) -> Dict[str, Any]:
    """What a preflight was checked against and a run must still find (field names: BINDING_FIELDS).
    `held_hash` is the app's hash of the program's full options (None from an app before #601): an
    option the candidate does not name, edited in the panel, changes it without any transition."""
    app = state['app']
    return {'console': state['console'], 'instance_id': app['instance']['id'], 'panel': panel,
            'candidate_hash': candidate_hash(options), 'options_hash': held_hash, 'transition': transition_key(app),
            'save': [save.get('status'), save.get('reason'), save.get('since'), save.get('cfr_at')],
            'lease': lease_token_hash}


def new_token(bound: Dict[str, Any]) -> str:
    salt = secrets.token_hex(16)
    return 'pf1.' + hashlib.sha256((salt + _json(bound)).encode('utf-8')).hexdigest()


def changed(issued: Dict[str, Any], now: Dict[str, Any]) -> List[str]:
    """The binding fields that differ; 'lease' rather than the hashes."""
    return [field for field in BINDING_FIELDS if issued.get(field) != now.get(field)]
