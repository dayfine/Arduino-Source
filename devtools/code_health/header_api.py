"""Header API shape: prefix overloads and the number of public namespace-scope names.

Both read the statements `style.py` lexes. An overload pair whose parameter types are a
strict prefix of each other is what a default argument or a `std::optional` parameter
replaces. The header symbol count is a Metric, compared with the base like function length.
"""
import re

from .config import HEADER_LIMITS
from .findings import Finding
from .metrics import Metric
from .style import (HEADERS, added_lines, default_split, internal, parameter_type, rule, scopes, signature,
                    statements, strip_code, without_templates)

SYMBOL = '<header>'
TYPE_HEAD = re.compile(r'(?:template\s*<.*>\s*)?(?:enum(?:\s+(?:class|struct))?|class|struct|union)\b(.*)$')
DECLARATION_KEYWORDS = re.compile(r'(using|typedef|namespace|template|class|struct|union|enum|friend'
                                  r'|static_assert|return|extern\s*")\b')
VALUE = re.compile(r'(?:[\w:]+(?:<.*>)?[\s\*&]+)+?[\*&\s]*([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?')


def public(enclosing):
    """Namespace scope outside anonymous, `detail`, `internal` and `*Internal` namespaces."""
    for kind, name in enclosing:
        if kind == 'linkage':
            continue
        if kind != 'namespace' or not name or internal(name.split('::')[-1]):
            return False
    return True


def normalized(text):
    return ' '.join(text.replace('*', ' * ').replace('&', ' & ').split())


# ---- Prefix overloads ---------------------------------------------------------------

def overload_candidate(statement, enclosing):
    """(scope, name, parameter types) of a declaration that may pair with a prefix overload."""
    sig = signature(statement)
    if sig is None or '::' in sig.name or sig.name.startswith('~') or 'operator' in statement.text:
        return None
    if re.search(r'=\s*delete\b', sig.tail) or any('...' in p for p in sig.params):
        return None
    if enclosing and enclosing[-1][0] == 'class' and sig.name == enclosing[-1][1]:
        return None  # constructor
    if not all(kind in ('namespace', 'linkage', 'class') for kind, _ in enclosing):
        return None
    params = tuple(normalized(parameter_type(p)) for p in sig.params if p.strip() not in ('', 'void'))
    return enclosing, sig.name, params


def prefix_overloads(path, source, previous_source=None):
    """Errors for same-scope overloads whose parameter types are a strict prefix of another's.

    At least one of the pair must be on a line added since `previous_source`."""
    if not path.endswith(HEADERS):
        return []
    previous = normalize(previous_source).split('\n') if previous_source is not None else None
    added = added_lines(normalize(source).split('\n'), previous)
    found = []
    for statement, enclosing in scopes(statements(strip_code(normalize(source)))):
        candidate = overload_candidate(statement, enclosing)
        if candidate:
            found.append((candidate, statement.line))
    errors, reported = [], set()
    for (scope, name, short), line in found:
        for (other_scope, other_name, long), other_line in found:
            if (scope, name) != (other_scope, other_name) or len(short) >= len(long) \
                    or long[:len(short)] != short:
                continue
            if line not in added and other_line not in added:
                continue
            at = max(line, other_line)
            if (at, name) in reported:
                continue
            reported.add((at, name))
            errors.append(Finding(path, at, rule(
                f'{name}: overload ({", ".join(short)}) is a prefix of ({", ".join(long)}) at line '
                f'{other_line if at == line else line}; use one function with a default argument '
                'or a std::optional parameter', 'fork AGENTS.md "Functions"'),
                check='overload_prefix'))
    return sorted(errors, key=lambda f: f.line)


# ---- Public names per header --------------------------------------------------------

def type_name(text):
    head = TYPE_HEAD.match(text)
    if not head:
        return None
    rest = re.split(r'(?<!:):(?!:)', head.group(1))[0]
    if '<' in without_templates(rest) or '<' in rest:
        return None  # specialization of a template declared elsewhere
    words = [w for w in re.findall(r'[A-Za-z_]\w*', rest) if w != 'final']
    return words[-1] if words else None


def declared_name(statement):
    """The namespace-scope name a statement declares, or None."""
    text = statement.text
    if statement.end == '}' or not text or (statement.follows == 'class' and statement.end == ';'):
        return None  # the variable in `struct{...} name;`, rare at namespace scope
    alias = re.match(r'(?:template\s*<.*>\s*)?using\s+([A-Za-z_]\w*)\s*=', text)
    if alias:
        return alias.group(1)
    if text.startswith('typedef'):
        names = re.findall(r'([A-Za-z_]\w*)\s*\)?\s*(?:\(.*\))?\s*(?:\[[^\]]*\])?$', text)
        return names[-1] if names else None
    if statement.end == '{':
        named = type_name(text)
        if named:
            return named
    elif TYPE_HEAD.match(text):
        return None  # forward or opaque declaration
    sig = signature(statement)
    if sig is not None:
        if '::' in sig.name or 'operator' in text:
            return None
        return sig.name
    head = re.sub(r'^template\s*<.*?>\s*', '', text)
    head = default_split(head.rstrip('=').strip() if statement.end == '{' else head)[0]
    if DECLARATION_KEYWORDS.match(head) or '(' in without_templates(head):
        return None
    value = VALUE.fullmatch(head)
    return value.group(1) if value else None


def public_names(source):
    """{name: first line} of distinct public namespace-scope names in a header."""
    names = {}
    for statement, enclosing in scopes(statements(strip_code(normalize(source)))):
        if not statement.namespace_scope or not public(enclosing):
            continue
        name = declared_name(statement)
        if name and name not in names:
            names[name] = statement.line
    return names


def measure_header(path, source):
    if not path.endswith(HEADERS):
        return []
    count = len(public_names(source))
    return [Metric(path, SYMBOL, 'header_symbols', count, HEADER_LIMITS['header_symbols'], 1)]


def normalize(source):
    return source.replace('\r\n', '\n').replace('\r', '\n')
