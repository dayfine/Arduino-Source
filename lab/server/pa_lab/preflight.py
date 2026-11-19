"""The server preflight of `run_program` (the preflight split, lab/server/README.md): the checks
that need nothing from inside the game, so they press nothing. `pa_lab.actions.check_run` runs them
in one transaction with the request record, right before `set` lines and `start` go out; what was
checked and what is started are one request, so there is no token and nothing to restore.

**Options.** `check_options(contract, options)` checks a candidate against `describe_program`'s
option entries (Lab.md "Describe program") before anything is sent: every key must be one the
program saves, a value must have the entry's type and lie within its bounds, an enum/select value
must be an enabled slug, a group's value is an object checked against the group's own entries
(plus `Enabled` when the group is toggleable). A `type: other` option cannot be set. Dynamic
visibility and `check_validity` depend on the other options' values; only the app evaluates them,
at the `start` line.

**Start checks stay on.** `describe_program` marks a boolean option that turns a start check off
with `skips_check: "<check name>"` (FRLG RNG's free party slots setting): it is for the user's own
Start. `check_start_checks_on` refuses a candidate that leaves such an option out or sets it
true, so a server run never starts with a check off, whatever the panel holds.

**Save.** `check_save` is the hook for Slice 6d's save check (`save_mismatch`); it passes
everything until save facts exist.
"""

from __future__ import annotations

from typing import Any, Dict, List

from pa_lab.channel import SetOption
from pa_lab.control import Refused

__all__ = ['check_options', 'check_start_checks_on', 'check_save', 'set_commands']


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
        raise _bad(path, 'the program contract does not describe this option (type {}); it cannot be set'.format(
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


# ---- start checks stay on

def _check_skippers(entries: List[Dict[str, Any]], options: Dict[str, Any], path: str) -> None:
    for entry in entries:
        key = entry.get('key')
        sub = '{}.{}'.format(path, key)
        if entry.get('type') == 'group':
            value = options.get(key)
            _check_skippers(list(entry.get('options') or []), value if isinstance(value, dict) else {}, sub)
            continue
        check = entry.get('skips_check')
        if not isinstance(check, str):
            continue
        if options.get(key) is not False:
            raise _bad(sub, 'turns off the start check {}; a run_program must set it to false (the check runs on '
                            'every server start)'.format(check))


def check_start_checks_on(program: Dict[str, Any], options: Dict[str, Any]) -> None:
    """Raises Refused(invalid_arguments, field=options.<key path>) for an option marked `skips_check`
    that the candidate leaves out or does not set to false. Run after check_options."""
    _check_skippers(program['options'], options, 'options')


# ---- the save (Slice 6d)

def check_save(save: Dict[str, Any], program: Dict[str, Any], options: Dict[str, Any]) -> None:
    """The save check's hook: Slice 6d refuses `save_mismatch` (evidence: the save resource's status,
    reason, since, the fact's checkpoint and game) when `console://B/save` does not match the run's
    start. Until save facts exist it passes every run."""
    return None
