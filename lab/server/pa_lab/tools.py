"""The read-only tools of Slice 2 (fork #131): argument checks and `ensure_game`.

`shot` lives in pa_lab/shot.py. No tool here presses a button, starts or stops a program, or
changes what the app shows; mutating tools arrive with leases (Slices 3 and 5).
"""

from __future__ import annotations

from typing import Any, Dict, Optional

from pa_lab import contracts, timestamps
from pa_lab.contracts import F, Int, ListOf, Obj, Str
from pa_lab.lab_reader import ConsoleReader

ENSURE_GAME_DEFAULT_MAX_AGE_MS = 5000

ARGUMENTS: Dict[str, Obj] = {
    'shot': Obj([
        F('console', Str()),
        F('max_age_ms', Int(minimum=100, maximum=60000), required=False, doc='default 2000'),
        F('timeout_ms', Int(minimum=100, maximum=60000), required=False, doc='default 5000'),
    ], open=False),
    'ensure_game': Obj([
        F('console', Str()),
        F('expected', Str(contracts.GAME_ID_RE, 'game id'), doc="e.g. 'PokemonFRLG'"),
        F('allowed_screens', ListOf(Str(contracts.GAME_ID_RE, 'screen id'), min_items=1, max_items=32),
          required=False),
        F('max_age_ms', Int(minimum=100, maximum=600000), required=False, doc='default 5000'),
    ], open=False),
}


class InvalidArguments(Exception):
    def __init__(self, field: str, message: str):
        super().__init__('{}: {}'.format(field, message))
        self.field = field
        self.message = message

    def refusal(self) -> Dict[str, Any]:
        return {'code': 'invalid_arguments', 'field': self.field, 'message': self.message}


def check_arguments(tool: str, arguments: Any) -> Dict[str, Any]:
    """Closed argument objects: an unknown key is refused, as for every request (README)."""
    ctx = contracts._Ctx()
    if not isinstance(arguments, dict):
        raise InvalidArguments('arguments', 'expected an object')
    out = ARGUMENTS[tool].check(arguments, '', ctx)
    if ctx.errors:
        first = ctx.errors[0]
        raise InvalidArguments(first.path, first.message)
    return out


def _age_ms(observation: Dict[str, Any], now: float) -> Optional[int]:
    if observation['observed_at'] is None:
        return None
    return int(round((now - timestamps.parse(observation['observed_at'])) * 1000))


def ensure_game(console: ConsoleReader, arguments: Dict[str, Any]) -> Dict[str, Any]:
    """Verifies, from the app's own observations, that the expected game (and one of the allowed
    screens) is in front. Never navigates HOME or launches a title. `unknown` never verifies.

    Raises InvalidArguments, or lab_reader.Unavailable when the app state cannot be trusted.
    """
    args = check_arguments('ensure_game', dict(arguments, console=arguments.get('console', console.console)))
    max_age_ms = args.get('max_age_ms', ENSURE_GAME_DEFAULT_MAX_AGE_MS)
    app = console.app_state()
    now = console.now()
    game = app['observation']['game']
    screen = app['observation']['screen']
    game_age = _age_ms(game, now)
    reason = None
    if game['value'] == contracts.UNKNOWN:
        reason = 'game_unknown'
    elif game_age > max_age_ms:
        reason = 'observation_stale'
    elif game['value'] != args['expected']:
        reason = 'game_mismatch'
    elif 'allowed_screens' in args:
        screen_age = _age_ms(screen, now)
        if screen['value'] == contracts.UNKNOWN:
            reason = 'screen_unknown'
        elif screen_age > max_age_ms:
            reason = 'observation_stale'
        elif screen['value'] not in args['allowed_screens']:
            reason = 'screen_not_allowed'
    return {
        'console': console.console,
        'verified': reason is None,
        'reason': reason,
        'expected': args['expected'],
        'instance_id': app['instance']['id'],
        'game': game,
        'game_age_ms': game_age,
        'screen': screen,
        'screen_age_ms': _age_ms(screen, now),
    }
