"""The `cfr_batch` allowlist (Slice 5b of fork #131, from #547): which console lines a leased agent may send.

A batch is 1-200 lines in the app's console-command syntax (Lab.md "Console"), each checked here
before anything is stored or sent. Only controller and screen-reading commands pass; every
number is bounded, and the batch's worst-case duration must fit in `BATCH_MAX_MS`, so the call
that waits for its journal is bounded too. Left out on purpose:

- channel commands (`select`, `set`, `start`, `stop`, `owner`, `request`, `keepawake`, `popups`,
  `snapshot`, `describe_program`, `state`, `help`): the server has its own tools for these;
- `close_game` (loses unsaved progress) and `history` (writes a recording);
- the `CAPTURE` button (writes to the console's album);
- the `image` argument of `detect` / `game_open`, which names a file;
- game-registered commands, comments (`#`) and blank lines;
- any line not in canonical form (single spaces, no leading or trailing space).

`check(lines)` returns the batch's worst-case duration in ms or raises `control.Refused`
(`invalid_arguments`, `field` = `arguments.lines[i]`, or `arguments.lines` for the budget).
"""

from __future__ import annotations

import re
from typing import Callable, Dict, List, Optional

from pa_lab import control

BATCH_MAX_MS = 60000
MAX_HOLD_MS = 10000
MAX_WAIT_MS = 30000
MAX_TAPS = 20
MAX_SHOTS = 10
LINE_OVERHEAD_MS = 200  # per line: the app's own bookkeeping
SCREEN_READ_MS = 1000  # shot, detect, game_open: one frame read and maybe a PNG write

BUTTONS = ('A', 'B', 'X', 'Y', 'L', 'R', 'ZL', 'ZR', 'PLUS', 'MINUS', 'LCLICK', 'RCLICK', 'HOME')
DIRECTIONS = ('UP', 'DOWN', 'LEFT', 'RIGHT')
INT_RE = re.compile(r'^(0|[1-9][0-9]{0,5})$')
AXIS_RE = re.compile(r'^-?(0|1)(\.[0-9]{1,3})?$')
STILL_RE = re.compile(r'^(0|[1-9][0-9]{0,2})(\.[0-9]{1,2})?$')
LABEL_RE = re.compile(r'^[A-Za-z0-9_-]{1,64}$')
DETECTOR_RE = re.compile(r'^[a-z][a-z0-9_]{0,39}$')

__all__ = ['check', 'BATCH_MAX_MS', 'COMMANDS']


class _Bad(ValueError):
    pass


def _int(word: str, name: str, minimum: int, maximum: int) -> int:
    if not INT_RE.match(word) or not minimum <= int(word) <= maximum:
        raise _Bad('{} must be an integer {}..{}, not {!r}'.format(name, minimum, maximum, word))
    return int(word)


def _axis(word: str, name: str) -> None:
    if not AXIS_RE.match(word) or abs(float(word)) > 1:
        raise _Bad('{} must be a number -1..1 with at most 3 decimals, not {!r}'.format(name, word))


def _still(word: str) -> None:
    if not STILL_RE.match(word):
        raise _Bad('still must be a number 0..999 with at most 2 decimals, not {!r}'.format(word))


def _buttons(word: str) -> None:
    parts = word.split('+')
    if any(part not in BUTTONS for part in parts) or len(set(parts)) != len(parts):
        raise _Bad('buttons must be one or more of {} joined with +, not {!r}'.format(' '.join(BUTTONS), word))


def _arity(args: List[str], least: int, most: int, usage: str) -> None:
    if not least <= len(args) <= most:
        raise _Bad('usage: ' + usage)


def _hold_release(args: List[str], hold: int, release: int) -> int:
    if len(args) > 0:
        hold = _int(args[0], 'hold', 0, MAX_HOLD_MS)
    if len(args) > 1:
        release = _int(args[1], 'release', 0, MAX_HOLD_MS)
    return hold + release


def _press(args: List[str]) -> int:
    _arity(args, 1, 3, 'press BTN[+BTN] [hold] [release]')
    _buttons(args[0])
    return _hold_release(args[1:], 160, 160)


def _dpad(args: List[str]) -> int:
    _arity(args, 1, 4, 'dpad UP|DOWN|LEFT|RIGHT [hold] [release] [BTN[+BTN]]')
    if args[0] not in DIRECTIONS:
        raise _Bad('direction must be one of {}'.format(' '.join(DIRECTIONS)))
    if len(args) == 4:
        _buttons(args[3])
    return _hold_release(args[1:3], 100, 200)


def _stick(args: List[str]) -> int:
    _arity(args, 3, 6, 'stick L|R x y [hold] [release] [BTN[+BTN]]')
    if args[0] not in ('L', 'R'):
        raise _Bad('stick must be L or R')
    _axis(args[1], 'x')
    _axis(args[2], 'y')
    if len(args) == 6:
        _buttons(args[5])
    return _hold_release(args[3:5], 160, 160)


def _wait(args: List[str]) -> int:
    _arity(args, 1, 1, 'wait ms')
    return _int(args[0], 'ms', 0, MAX_WAIT_MS)


def _taps(args: List[str]) -> int:
    _arity(args, 2, 6, 'taps UP|DOWN|LEFT|RIGHT N [all] [hold] [release] [still]')
    if args[0] not in DIRECTIONS:
        raise _Bad('direction must be one of {}'.format(' '.join(DIRECTIONS)))
    count = _int(args[1], 'N', 1, MAX_TAPS)
    rest = args[3:] if len(args) > 2 and args[2] == 'all' else args[2:]
    if len(rest) > 3:
        raise _Bad('usage: taps UP|DOWN|LEFT|RIGHT N [all] [hold] [release] [still]')
    if len(rest) == 3:
        _still(rest[2])
    return count * (_hold_release(rest[:2], 160, 640) + LINE_OVERHEAD_MS)


def _walk(args: List[str]) -> int:
    _arity(args, 2, 5, 'walk x y [max] [still] [BTN[+BTN]]')
    _axis(args[0], 'x')
    _axis(args[1], 'y')
    longest = _int(args[2], 'max', 200, MAX_HOLD_MS) if len(args) > 2 else 5000
    if len(args) > 3:
        _still(args[3])
    if len(args) > 4:
        _buttons(args[4])
    return longest + 2 * LINE_OVERHEAD_MS


def _shot(args: List[str]) -> int:
    _arity(args, 1, 1, 'shot LABEL')
    _label(args[0])
    return SCREEN_READ_MS


def _shots(args: List[str]) -> int:
    _arity(args, 2, 3, 'shots LABEL count [interval]')
    _label(args[0])
    count = _int(args[1], 'count', 1, MAX_SHOTS)
    interval = _int(args[2], 'interval', 100, 5000) if len(args) > 2 else 500
    return count * (interval + SCREEN_READ_MS)


def _label(word: str) -> None:
    if not LABEL_RE.match(word):
        raise _Bad('label must be 1-64 of A-Z a-z 0-9 _ -')


def _detector(word: str) -> None:
    if not DETECTOR_RE.match(word):
        raise _Bad('detector must be a name of a-z 0-9 _ (no image path)')


def _detect(args: List[str]) -> int:
    _arity(args, 1, 1, 'detect DETECTOR')
    _detector(args[0])
    return SCREEN_READ_MS


def _wait_for(args: List[str]) -> int:
    _arity(args, 1, 2, 'wait_for|expect DETECTOR [timeout]')
    _detector(args[0])
    return (_int(args[1], 'timeout', 0, MAX_WAIT_MS) if len(args) > 1 else 5000) + SCREEN_READ_MS


def _game_open(args: List[str]) -> int:
    _arity(args, 0, 0, 'game_open')
    return SCREEN_READ_MS


COMMANDS: Dict[str, Callable[[List[str]], int]] = {
    'press': _press, 'dpad': _dpad, 'stick': _stick, 'wait': _wait, 'taps': _taps, 'walk': _walk,
    'shot': _shot, 'shots': _shots, 'detect': _detect, 'wait_for': _wait_for, 'expect': _wait_for,
    'game_open': _game_open,
}


def check_line(line: str) -> int:
    """One line's worst-case duration in ms; raises ValueError with the reason it is not allowed."""
    if not isinstance(line, str) or line != ' '.join(line.split()) or not line:
        raise _Bad('not in canonical form: one command, words separated by single spaces')
    words = line.split(' ')
    handler: Optional[Callable[[List[str]], int]] = COMMANDS.get(words[0])
    if handler is None:
        raise _Bad('{!r} is not an allowed cfr_batch command; allowed: {}'.format(
            words[0], ', '.join(sorted(COMMANDS))))
    return handler(words[1:]) + LINE_OVERHEAD_MS


def check(lines: List[str]) -> int:
    """The batch's worst-case duration in ms, or control.Refused naming the first bad line."""
    total = 0
    for index, line in enumerate(lines):
        try:
            total += check_line(line)
        except _Bad as e:
            raise control.Refused('invalid_arguments', 'Line {} ({!r}): {}.'.format(index + 1, line, e),
                                  field='arguments.lines[{}]'.format(index))
    if total > BATCH_MAX_MS:
        raise control.Refused('invalid_arguments', 'The batch may take up to {} ms; the limit is {} ms. '
                              'Split it.'.format(total, BATCH_MAX_MS), field='arguments.lines',
                              evidence={'worst_case_ms': total, 'limit_ms': BATCH_MAX_MS})
    return total
