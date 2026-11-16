"""Test-only header exposure: a public name a header adds whose only users are tests.

A helper moved into a header so a `*_Tests.cpp` can call it widens the API for the tests'
sake (user review on fork #590). Test through the public function, or keep the helper in
the .cpp's anonymous namespace. Names are the header's public namespace-scope names
(`header_api.public_names`); only names new since the base revision, on added lines, count.
Users are the tracked C++ files that mention the name as a word (`git grep -w`), other than
the header itself and its own .cpp. Reported: a new name that only test files use, or that
only its own .cpp uses (tests in the same .cpp need no header declaration). A name with no
users at all is new API for a caller still to come, and a name that another public declaration
of the header mentions (a parameter, return or member type) is part of that API: neither is
reported.

Not reported: test headers (`*_Tests.h`, `Tests/`) and test registration hooks
(`add_tests*`), which exist to be called by the test runner.
"""
import re
import subprocess
from pathlib import PurePosixPath

from .findings import Finding
from .git_files import EXTENSIONS
from .header_api import declared_name, normalize, public, public_names
from .style import HEADERS, added_lines, rule, scopes, statements, strip_code

REGISTRATION = re.compile(r'add_tests\w*')
CITE = 'user review on fork #590'


def test_file(path):
    """`*_Tests.cpp`, `*_Test.cpp` and `*_Tests.h` files and anything under a `Tests/` directory."""
    pure = PurePosixPath(path)
    return 'Tests' in pure.parts[:-1] or re.search(r'_Tests?$', pure.stem) is not None


def git_users(root):
    """A function from names to {name: tracked first-party C++ files that mention it as a word}.

    One `git grep -o` for all names: a name per call would take minutes on a large change."""
    pathspecs = [f'{top}/*{ext}' for top in ('Common', 'SerialPrograms') for ext in sorted(EXTENSIONS)]

    def users(names):
        found = {name: set() for name in names}
        if not names:
            return found
        patterns = [arg for name in sorted(names) for arg in ('-e', name)]
        result = subprocess.run(['git', '-C', str(root), 'grep', '-o', '-w', '-F', '-I', '--null',
                                 *patterns, '--', *pathspecs], capture_output=True, text=True)
        if result.returncode not in (0, 1):
            raise ValueError(f'git grep for header names failed: {result.stderr.strip()}')
        for line in result.stdout.splitlines():
            path, _, match = line.partition('\0')
            if match in found and '3rdParty' not in path:
                found[match].add(path)
        return found
    return users


def own_files(path):
    """The header and the sources that implement it: same directory and stem."""
    stem = str(PurePosixPath(path).with_suffix(''))
    return {stem + ext for ext in EXTENSIONS}


def mentions_by_owner(source):
    """{public name: identifiers its declaration mentions}, members of a public class included."""
    owners = {}
    for statement, enclosing in scopes(statements(strip_code(normalize(source)))):
        if statement.end == '}':
            continue
        if statement.namespace_scope and public(enclosing):
            owner = declared_name(statement)
        else:
            outer = [name for kind, name in enclosing if kind == 'class']
            owner = outer[0] if outer and public([f for f in enclosing if f[0] != 'class']) else None
        if owner:
            owners.setdefault(owner, set()).update(re.findall(r'[A-Za-z_]\w*', statement.text))
    return owners


def exposure(path, mentions):
    """Why a name that `mentions` (files) use is exposed only for tests or its own .cpp, or None."""
    outside = sorted(mentions - own_files(path))
    if any(not test_file(user) for user in outside):
        return None
    if outside:
        return f"public only for {', '.join(PurePosixPath(u).name for u in outside)}"
    if mentions & (own_files(path) - {path}):
        return 'used only in its own .cpp'
    return None


def check(path, source, previous_source, users):
    """Advisories for public names this header adds that only tests or its own .cpp use.

    `users(names)` returns {name: repository files that mention it} (see `git_users`)."""
    if not path.endswith(HEADERS) or test_file(path):
        return []
    previous = set(public_names(previous_source)) if previous_source is not None else set()
    added = added_lines(normalize(source).split('\n'),
                        None if previous_source is None else normalize(previous_source).split('\n'))
    names = public_names(source)
    new = [name for name, line in names.items()
           if name not in previous and line in added and not REGISTRATION.fullmatch(name)]
    mentions = users(new) if new else {}
    exposed = {name: why for name in new for why in [exposure(path, mentions.get(name, set()))] if why}
    owners = mentions_by_owner(source)
    changed = True
    while changed:  # a name that a kept public declaration mentions is part of its API
        needed = {name for name in exposed for owner, words in owners.items()
                  if owner != name and owner not in exposed and name in words}
        changed = bool(needed)
        for name in needed:
            del exposed[name]
    return [Finding(path, names[name], rule(
        f"'{name}' is {why}; test through the public function, or keep it in the .cpp's "
        'anonymous namespace (ignore if a later PR in the stack calls it)', CITE), check='test_only_api')
        for name, why in sorted(exposed.items(), key=lambda item: names[item[0]])]
