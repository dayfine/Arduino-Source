"""Shared code (Common, CommonFramework, CommonTools) may not include game modules."""
import posixpath
import re

from .config import GAME_MODULES, SHARED_DIRS, SOURCE_ROOT


INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]+)[>"]', re.M)


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
    """Fail includes of game modules that this change adds to shared code."""
    before = game_includes(path, previous_source)
    return [f'{path}:{line}: layering: shared code includes game module {module} ("{include}"); '
            'move the dependency into the game or invert it through an interface/callback'
            for include, (line, module) in sorted(game_includes(path, source).items())
            if include not in before]
