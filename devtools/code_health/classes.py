"""Class/struct shape checks from a lexical scan (no compiler or include paths needed).

Members are recognized from lizard's C++ tokens: access sections, declarations ended
by ';' and inline bodies. Unbalanced braces from #if branches make a class unmeasured
rather than failing the run.
"""
import re
from dataclasses import dataclass

from lizard_languages import CLikeReader

from .config import CLASS_LIMITS
from .metrics import Metric


MACRO = re.compile(r'^[A-Z][A-Z0-9_]*$')
IDENTIFIER = re.compile(r'^[A-Za-z_]\w*$')
ACCESS = {'public', 'private', 'protected'}
SIGNALS = {'signals', 'Q_SIGNALS'}
SLOTS = {'slots', 'Q_SLOTS'}
TYPE_KEYWORDS = {'class', 'struct', 'union'}
# Words that may precede '(' in a declaration without naming a function.
NOT_NAMES = {'decltype', 'alignas', 'alignof', 'sizeof', 'noexcept', 'throw', 'requires',
             '__attribute__', '__declspec', 'void', 'int', 'bool', 'char', 'auto'}
SKIP_DECLARATIONS = {'friend', 'using', 'typedef', 'static_assert', 'enum'}


@dataclass(frozen=True)
class Member:
    access: str
    kind: str          # method, ctor, dtor, operator, data, type
    pure: bool = False
    deleted: bool = False
    virtual: bool = False
    static: bool = False
    const: bool = False    # const/static method, or const data: cannot mutate the object
    option: bool = False   # a config option or table cell field (type named *Option or *Cell)


@dataclass(frozen=True)
class ClassInfo:
    name: str
    line: int
    has_bases: bool
    members: tuple
    internal: bool = False  # nested in a class, or in a detail or anonymous namespace


def tokens(source):
    result, line = [], 1
    for text in CLikeReader.generate_tokens(source):
        if not (text.isspace() or text.startswith(('//', '/*', '#'))):
            result.append((text, line))
        line += text.count('\n')
    return result


def brace_pairs(toks):
    pairs, stack = {}, []
    for index, (text, _) in enumerate(toks):
        if text == '{':
            stack.append(index)
        elif text == '}' and stack:
            pairs[stack.pop()] = index
    return pairs


def skip_group(toks, index, open_text, close_text):
    """Index just past the group opened at index; len(toks) if unbalanced."""
    depth = 0
    for k in range(index, len(toks)):
        if toks[k][0] == open_text:
            depth += 1
        elif toks[k][0] == close_text:
            depth -= 1
            if depth == 0:
                return k + 1
    return len(toks)


def class_head(toks, index):
    """For a class/struct/union keyword, return (name, has_bases, '{' index) or None."""
    if index > 0 and toks[index - 1][0] in ('enum', '<', ',', 'friend', '(', 'typename'):
        return None
    angle, bases, names, k = 0, False, [], index + 1
    while k < len(toks):
        text = toks[k][0]
        if text == '{' and angle == 0:
            break
        if text == '(' and k > 0 and (toks[k - 1][0] in NOT_NAMES or MACRO.match(toks[k - 1][0])):
            k = skip_group(toks, k, '(', ')')
            continue
        if text == '[':
            k = skip_group(toks, k, '[', ']')
            continue
        if text in (';', '(', ')', '=', '}', '{') and angle == 0:
            return None
        if text == '<':
            angle += 1
        elif text == '>':
            angle -= 1
            if angle < 0:
                return None
        elif text == ',' and angle == 0 and not bases:
            return None
        elif text == ':' and angle == 0:
            bases = True
        elif not bases and angle == 0 and IDENTIFIER.match(text) and text != 'final':
            names.append(text)
        k += 1
    else:
        return None
    if len(names) > 1:
        names = [n for n in names if not MACRO.match(n)] or names
    return (names[-1] if names else None), bases, k


def strip_template(decl):
    if decl and decl[0] == 'template' and len(decl) > 1 and decl[1] == '<':
        depth = 0
        for k in range(1, len(decl)):
            depth += {'<': 1, '>': -1}.get(decl[k], 0)
            if depth == 0:
                return decl[k + 1:]
    return decl


def call_paren(decl):
    """Index of the '(' that opens a function's parameters, or None for data."""
    angle = 0
    for k, text in enumerate(decl):
        if text == 'operator':
            return decl.index('(', k + 2) if '(' in decl[k + 2:] else None
        if text == '=' and angle == 0:
            return None
        if text == '<' and k > 0 and IDENTIFIER.match(decl[k - 1]):
            angle += 1
        elif text == '>' and angle > 0:
            angle -= 1
        elif text == '(' and angle == 0 and k > 0:
            if decl[k - 1] in NOT_NAMES or not IDENTIFIER.match(decl[k - 1]):
                return None
            if k + 1 < len(decl) and decl[k + 1] in ('*', '&', '^'):
                return None
            return k
    return None


def is_option(decl):
    """Option and cell fields are the framework's reflected settings, public by convention."""
    angle, words = 0, []
    for text in decl:
        if text in ('=', '{}', ':', '['):
            break
        angle += {'<': 1, '>': -1}.get(text, 0)
        if angle == 0 and IDENTIFIER.match(text):
            words.append(text)
    return any(w.endswith(('Option', 'Options', 'Cell')) for w in words[:-1])


def _close(tail):
    depth = 0
    for k, text in enumerate(tail):
        depth += {'(': 1, ')': -1}.get(text, 0)
        if depth == 0:
            return k + 1
    return len(tail)


def classify(decl, access, class_name):
    decl = strip_template(decl)
    if not decl or decl[0] in SKIP_DECLARATIONS:
        return None
    if decl[0] in TYPE_KEYWORDS:
        declarator = '{}' in decl and IDENTIFIER.match(decl[-1] if decl[-1] != '{}' else '')
        return Member(access, 'data') if declarator else Member(access, 'type')
    paren = call_paren(decl)
    if paren is None:
        # A const value (not a pointer to const) cannot be modified through the public field.
        read_only = 'const' in decl and '*' not in decl and '&' not in decl
        return Member(access, 'data', static='static' in decl, option=is_option(decl),
                      const=read_only)
    name = decl[paren - 1]
    if 'operator' in decl[:paren]:
        kind = 'operator'
    elif paren >= 2 and decl[paren - 2] == '~':
        kind = 'dtor'
    elif name == class_name:
        kind = 'ctor'
    else:
        kind = 'method'
    tail = decl[paren:]
    return Member(access, kind,
                  pure=len(tail) >= 2 and tail[-2:] == ['=', '0'],
                  deleted='delete' in tail and '=' in tail,
                  virtual='virtual' in decl or 'override' in tail,
                  static='static' in decl[:paren],
                  const='static' in decl[:paren] or 'const' in tail[_close(tail):])


def members(toks, start, end, pairs, keyword, class_name):
    access = 'private' if keyword == 'class' else 'public'
    found, decl, paren_depth, k = [], [], 0, start
    while k < end:
        text = toks[k][0]
        following = toks[k + 1][0] if k + 1 < end else ''
        if not decl:
            if text in ACCESS and following == ':':
                access, k = text, k + 2
                continue
            if text in ACCESS and following in SLOTS:
                access, k = text, k + 3
                continue
            if text in SIGNALS and following == ':':
                access, k = 'signals', k + 2
                continue
            if text == ';' or (MACRO.match(text) and following != '::' and text != class_name):
                k = skip_group(toks, k + 1, '(', ')') if following == '(' else k + 1
                continue
        if text == '(':
            paren_depth += 1
        elif text == ')':
            paren_depth -= 1
        if text == '{':
            close = pairs.get(k)
            if close is None or close > end:
                return None
            is_function = paren_depth == 0 and call_paren(strip_template(decl)) is not None
            in_initializers = is_function and ':' in decl and decl[-1] not in (')', '{}')
            if is_function and not in_initializers and decl[-1] != '=':
                member = classify(decl, access, class_name)
                if member:
                    found.append(member)
                decl, k = [], close + 1
                if k < end and toks[k][0] == ';':
                    k += 1
                continue
            decl.append('{}')
            k = close + 1
            continue
        if text == ';' and paren_depth == 0:
            member = classify(decl, access, class_name)
            if member:
                found.append(member)
            decl, k = [], k + 1
            continue
        decl.append(text)
        k += 1
    return found


def scope_label(toks, index, heads):
    """Name a '{' scope for qualified class names: classes and namespaces only."""
    if index in heads:
        return heads[index][0]
    names = []
    for k in range(index - 1, -1, -1):
        text = toks[k][0]
        if text == 'namespace':
            return '::'.join(reversed(names)) or None
        if not (IDENTIFIER.match(text) or text == '::'):
            return None
        if text != '::':
            names.append(text)
    return None


def is_internal_scope(toks, index, heads):
    """A '{' that opens a class, an anonymous namespace or a detail namespace."""
    if index in heads:
        return True
    if index > 0 and toks[index - 1][0] == 'namespace':
        return True
    label = scope_label(toks, index, heads) or ''
    return label == 'detail' or label.endswith('::detail')


def find_classes(source):
    toks = tokens(source)
    pairs = brace_pairs(toks)
    heads = {}
    for index, (text, line) in enumerate(toks):
        if text in TYPE_KEYWORDS:
            head = class_head(toks, index)
            if head and head[0] and head[2] in pairs:
                heads[head[2]] = (head[0], head[1], line, text)
    result, stack, internal = [], [], []
    for index, (text, _) in enumerate(toks):
        if text == '{':
            label = scope_label(toks, index, heads)
            if index in heads:
                name, bases, line, keyword = heads[index]
                body = members(toks, index + 1, pairs[index], pairs, keyword, name)
                if body is not None:
                    qualified = '::'.join([s for s in stack if s] + [name])
                    result.append(ClassInfo(qualified, line, bases, tuple(body), any(internal)))
            stack.append(label)
            internal.append(is_internal_scope(toks, index, heads))
        elif text == '}' and stack:
            stack.pop()
            internal.pop()
    return result


def is_single_method_interface(info):
    """Nested and detail-namespace classes are implementation details (e.g. type erasure)."""
    if info.internal:
        return False
    methods = [m for m in info.members if m.kind == 'method']
    others = [m for m in info.members
              if m.kind != 'method' and not (m.kind == 'dtor' and m.virtual)]
    return not info.has_bases and len(methods) == 1 and methods[0].pure and not others


def is_test_file(path):
    return '/Tests/' in path or path.endswith(('_Tests.cpp', '_Test.cpp', 'Tests.cpp'))


def measure_classes(path, source):
    """Public data is allowed in value types (only const methods) and in test fakes."""
    result = []
    for info in find_classes(source.replace('\r\n', '\n').replace('\r', '\n')):
        methods = [m for m in info.members if m.kind == 'method' and not m.deleted]
        public_methods = sum(m.access == 'public' for m in methods)
        mutators = [m for m in methods if not m.const]
        public_data = sum(m.access == 'public' and m.kind == 'data' and not (m.static or m.option or m.const)
                          for m in info.members) if mutators and not is_test_file(path) else 0
        values = (
            ('public_methods', public_methods),
            ('public_data', public_data),
            ('single_method_interface', int(is_single_method_interface(info))),
        )
        for check, value in values:
            result.append(Metric(path, info.name, check, value, CLASS_LIMITS[check], info.line))
    return result
