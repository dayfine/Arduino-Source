"""Advisories from the user's review of fork #648 (fork AGENTS.md "Types, names and comments").

On new classes, and on public members and aliases a header adds:
- `free_function`: a class with 1-2 public methods (const getters aside) whose data members are
  private and mostly references or pointers to what the constructor was given. Value members are
  counted as built from the constructor's arguments: a lexer cannot see the constructor, so a class
  fires when at least half its data members are references, pointers or const.
- `uncalled_method`: a public method of a header class without bases (an override may lack the
  keyword) that no file outside the class's own .h/.cpp and tests calls (`.name`, `->name`,
  `::name`) in a file that, or whose own header, names the class or includes its header.
- `one_user_alias`: a namespace-scope `using X = ...;` in a header that one other header (or, with
  no header, one other module) uses, counting files that include the header: it lives with that user.
- `bool_with_failure`: a struct with a `bool` completed/passed/ok/... next to an `Outcome` or a
  `*failure*`/`*error*` member: one `std::optional<Failure>` instead.
"""
import re
from pathlib import PurePosixPath

from .classes import find_classes, is_test_file
from .exposure import own_files, test_file
from .findings import Finding
from .header_api import public, public_names
from .style import HEADERS, added_lines, rule, scopes, statements, strip_code

CITE = 'fork AGENTS.md "Types, names and comments"'
FLAGS = {'completed', 'passed', 'ok', 'succeeded', 'success', 'failed'}
ITERATION = {'begin', 'end', 'cbegin', 'cend', 'rbegin', 'rend'}  # called by range-for
ALIAS = re.compile(r'(?:template\s*<.*>\s*)?using\s+([A-Za-z_]\w*)\s*=')


def normalize(source):
    return source.replace('\r\n', '\n').replace('\r', '\n')


def data(info):
    return [m for m in info.members if m.kind == 'data' and not m.static]


def handle(member):
    """A reference, pointer or const data member: set once from what the constructor was given."""
    declarator = member.text.split('=')[0].split('{}')[0]
    return member.const or bool(re.search(r'[&*]', declarator))


def is_getter(member):
    return member.const and '( )' in member.text


def free_function(info):
    methods = [m for m in info.members if m.kind == 'method' and m.access == 'public'
               and not m.deleted and not m.static and not is_getter(m)]
    fields = data(info)
    handles = [m for m in fields if handle(m)]
    return (not info.has_bases and 1 <= len(methods) <= 2
            and not any(m.virtual for m in info.members)
            and any(m.kind == 'ctor' and not m.deleted for m in info.members)
            and fields and all(m.access != 'public' for m in fields)
            and any('&' in m.text or '*' in m.text for m in handles) and 2 * len(handles) >= len(fields))


def bool_with_failure(info):
    """(bool flag, failure member) of a struct that holds both, or None."""
    fields = data(info)
    flags = [m for m in fields if m.name in FLAGS and 'bool' in m.text.split(m.name)[0].split()]
    failures = [m for m in fields if re.search(r'failure|error', m.name, re.I)
                or re.search(r'\b(?:Outcome|\w+Report|\w+Result)\b', m.text.split(m.name)[0])]
    return (flags[0], failures[0]) if flags and failures else None


def knows(file, pattern, read):
    """`file`, or the header of its own module, matches `pattern` (names the class, includes its header)."""
    own_header = str(PurePosixPath(file).with_suffix('.h'))
    return bool(pattern.search(read(file)) or (own_header != file and pattern.search(read(own_header))))


def called_outside(name, owner, path, files, read):
    """A same-named method of an unrelated class (`->run_checkpoint(`) does not count."""
    call = re.compile(r'(?:\.|->|::)\s*(?:template\s+)?' + re.escape(name) + r'\b')
    owner_or_header = re.compile(r'\b' + re.escape(owner) + r'\b|' + re.escape(PurePosixPath(path).name))
    return any(call.search(read(f)) and knows(f, owner_or_header, read)
               for f in files - own_files(path) if not test_file(f))


def class_findings(path, source, previous_source, added, users, read):
    old = {c.name: c for c in find_classes(normalize(previous_source))} if previous_source is not None else {}
    found, candidates = [], {}
    for info in find_classes(normalize(source)):
        if info.internal:
            continue
        new, short = info.name not in old, info.name.split('::')[-1]
        if new and not is_test_file(path) and free_function(info):
            found.append(Finding(path, info.line, rule(
                f"'{short}' holds only what its constructor was given; make it a free function "
                'taking those as parameters', CITE), check='free_function'))
        pair = bool_with_failure(info)
        if pair and (new or pair[0].line in added or pair[1].line in added):
            found.append(Finding(path, pair[0].line, rule(
                f"'{short}' has bool '{pair[0].name}' next to '{pair[1].name}'; hold one "
                'std::optional<Failure> instead', CITE), check='bool_with_failure'))
        if path.endswith(HEADERS) and not test_file(path) and not info.has_bases:
            before = {m.name for m in old[info.name].members if m.access == 'public'} if not new else set()
            for m in info.members:
                if m.kind == 'method' and m.access == 'public' and not (m.virtual or m.deleted) \
                        and m.name not in before | ITERATION and m.line in added:
                    candidates.setdefault(m.name, []).append((info.name.split('::')[-1], m.line))
    mentions = users(list(candidates)) if candidates else {}
    for name, sites in candidates.items():
        found += [Finding(path, line, rule(
            f"public '{owner}::{name}' has no caller outside its own files and tests; make it "
            'private or drop it (ignore if a later PR in the stack calls it)', CITE), check='uncalled_method')
            for owner, line in sites if not called_outside(name, owner, path, mentions.get(name, set()), read)]
    return found


def alias_findings(path, source, previous_source, added, users, read):
    if not path.endswith(HEADERS) or test_file(path):
        return []
    previous = set(public_names(previous_source)) if previous_source is not None else set()
    aliases = {}
    for statement, enclosing in scopes(statements(strip_code(normalize(source)))):
        match = ALIAS.match(statement.text) if statement.end == ';' else None
        if match and statement.namespace_scope and public(enclosing) and statement.line in added \
                and match.group(1) not in previous:
            aliases[match.group(1)] = statement.line
    mentions = users(list(aliases)) if aliases else {}
    found = []
    for name, line in aliases.items():
        header = re.compile(re.escape(PurePosixPath(path).name))
        outside = {f for f in mentions.get(name, set()) - own_files(path)
                   if not test_file(f) and knows(f, header, read)}
        headers = [f for f in outside if f.endswith(HEADERS)]
        modules = {str(PurePosixPath(f).with_suffix('')) for f in outside}
        user = headers[0] if len(headers) == 1 else (outside.pop() if not headers and len(modules) == 1 else None)
        if user:
            found.append(Finding(path, line, rule(
                f"alias '{name}' has one user, {PurePosixPath(user).name}; define it there", CITE),
                check='one_user_alias'))
    return found


def check(path, source, previous_source, users, read):
    """Advisories on what this change adds. `users(names)` -> {name: files}; `read(file)` -> text."""
    added = added_lines(normalize(source).split('\n'),
                        None if previous_source is None else normalize(previous_source).split('\n'))
    if not added:
        return []
    found = class_findings(path, source, previous_source, added, users, read)
    found += alias_findings(path, source, previous_source, added, users, read)
    return sorted(found, key=lambda f: f.line)
