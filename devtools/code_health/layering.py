"""Layering rules: shared code vs game modules, and pure logic vs hardware/vision."""
import posixpath
import re

from .config import GAME_MODULES, SHARED_DIRS, SOURCE_ROOT
from .findings import Finding


INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]+)[>"]', re.M)

_PURE_SUFFIXES = (
    'Core.h', 'Core.cpp',
    'Policy.h', 'Policy.cpp',
    'Plan.h', 'Plan.cpp',
    'Sim.h', 'Sim.cpp',
    'SimField.h', 'SimField.cpp',
    'SimTaps.h', 'SimTaps.cpp',
    'Data.h', 'Data.cpp',
    'Table.h', 'Table.cpp',
    'Types.h', 'Types.cpp',
    'Supplies.h', 'Supplies.cpp',
    'StoryFailureClasses.h', 'StoryFailureClasses.cpp',
)

_HEAVY_BASENAMES = {
    'NintendoSwitch_ConsoleHandle.h',
    'NintendoSwitch_ProController.h',
    'NintendoSwitch_JoyconController.h',
    'ImageViewRGB32.h',
    'ImageRGB32.h',
    'VideoFeed.h',
}


def is_pure_module(path):
    """True when `path` is a pure-logic/sim/data/table/types file."""
    return path.endswith(_PURE_SUFFIXES)


def is_heavy_include(include):
    """True when `include` pulls in hardware, vision, or Qt dependencies."""
    name = posixpath.basename(include)
    if name in _HEAVY_BASENAMES:
        return True
    if name.endswith(('Detector.h', 'Reader.h')):
        return True
    if include.startswith(('Common/Qt/', 'Qt')) or (name.startswith('Q') and '.' not in name):
        return True
    return False


def heavy_includes(path, source):
    if not is_pure_module(path) or source is None:
        return {}
    source = source.replace('\r\n', '\n')
    found = {}
    for match in INCLUDE.finditer(source):
        include = match.group(1)
        if is_heavy_include(include):
            found.setdefault(include, source.count('\n', 0, match.start()) + 1)
    return found


def game_module(path, include):
    """The game module an include resolves to, trying the include root then the file's dir."""
    candidates = (include, posixpath.normpath(posixpath.join(posixpath.dirname(path), include)))
    for candidate in candidates:
        candidate = candidate.lstrip('./') if candidate.startswith('../') else candidate
        if candidate.startswith(SOURCE_ROOT):
            candidate = candidate[len(SOURCE_ROOT):]
        top = candidate.split('/', 1)
        if len(top) == 2 and top[0] in GAME_MODULES:
            return top[0]
    return None


def game_includes(path, source):
    if not path.startswith(SHARED_DIRS) or source is None:
        return {}
    source = source.replace('\r\n', '\n')
    found = {}
    for match in INCLUDE.finditer(source):
        module = game_module(path, match.group(1))
        if module:
            found.setdefault(match.group(1), (source.count('\n', 0, match.start()) + 1, module))
    return found


def check(path, source, previous_source):
    """Fail added game includes in shared code and added heavy includes in pure modules."""
    before_game = game_includes(path, previous_source)
    findings = [
        Finding(path, line, f'layering: shared code includes game module {module} ("{include}"); '
                'move the dependency into the game or invert it through an interface/callback')
        for include, (line, module) in sorted(game_includes(path, source).items())
        if include not in before_game
    ]
    before_heavy = heavy_includes(path, previous_source)
    findings.extend(
        Finding(path, line, f'layering: pure module includes hardware/vision/Qt header "{include}"; '
                'keep pure logic free of ConsoleHandle, ProController, ImageViewRGB32, Detector, Reader, and Qt')
        for include, line in sorted(heavy_includes(path, source).items())
        if include not in before_heavy
    )
    return findings
