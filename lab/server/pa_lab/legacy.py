"""The legacy key=value `Lab/state` (Lab.md on lab/tooling) and how it maps to `Lab/state.json`.

Slice 1 keeps `Lab/state` byte-compatible and writes `Lab/state.json` from the same app state.
`legacy_view(app_state)` is the mapping that writer must honour: the legacy fields it implies.
Every legacy field maps except the owner token, which state.json reduces to `owner_claimed`.
"""

from __future__ import annotations

from typing import Any, Dict

LEGACY_KEYS = ('seq', 'state', 'program', 'controller', 'owner', 'error', 'paused')

_CONTROLLER_TO_LEGACY = {'ready': 'ready', 'not_connected': 'none', 'no_console': ''}


def parse_legacy_state(text: str) -> Dict[str, str]:
    """Reads `key=value` lines; later keys win, unknown keys are kept, values may contain '='."""
    fields: Dict[str, str] = {}
    for line in text.splitlines():
        if not line.strip():
            continue
        key, sep, value = line.partition('=')
        if not sep:
            raise ValueError('line {!r} has no "="'.format(line))
        fields[key] = value
    return fields


def legacy_view(app_state: Dict[str, Any]) -> Dict[str, str]:
    """The legacy `Lab/state` fields a valid version-1 app state implies (owner reduced to yes/no)."""
    program = app_state['program']
    panel = app_state['panel']
    channel = app_state.get('channel') or {}
    return {
        'seq': channel.get('last_request') or '',
        'state': program['state'].upper(),
        'program': (panel['id'] or '') if panel['kind'] == 'program' else '',
        'controller': _CONTROLLER_TO_LEGACY[app_state['controller']['state']],
        'owner': 'claimed' if channel.get('owner_claimed') else '',
        'error': channel.get('last_error') or '',
        'paused': program.get('paused') or '',
    }


def comparable_legacy(fields: Dict[str, str]) -> Dict[str, str]:
    """A parsed legacy file in `legacy_view` terms: missing keys empty, the owner token reduced."""
    out = {key: fields.get(key, '') for key in LEGACY_KEYS}
    out['owner'] = 'claimed' if out['owner'] else ''
    return out
