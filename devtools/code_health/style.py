"""Lexical C++ style checks on the lines a change adds.

Upstream bans and formatting rules (AGENTS.md / CONTRIBUTING.md) and the Abseil Tips of
the Week adopted in the fork's C++ style notes. Only added or changed lines are checked,
so legacy code in a touched file is not reported. Comments, string and character
literals and preprocessor lines are blanked before matching. This is a lexer with a
brace-scope stack, not a parser: rules that need types stay in review.
"""
import difflib
import re
from dataclasses import dataclass

from .findings import Finding


HEADERS = ('.h', '.hpp', '.tpp')


@dataclass(frozen=True)
class Statement:
    text: str        # code with whitespace collapsed, attributes removed
    line: int        # line of its first character
    end: str         # ';', '{' or '}'
    scope: tuple     # enclosing brace kinds: 'namespace', 'class', 'enum', 'other'
    follows: str = ''  # kind of the brace closed just before it: `struct{...} name;`

    @property
    def namespace_scope(self):
        return all(kind == 'namespace' for kind in self.scope)

    @property
    def declaration_scope(self):
        """Namespace or class scope: declarations, not statements in a body."""
        return all(kind in ('namespace', 'class') for kind in self.scope)


# ---- Lexing ----------------------------------------------------------------------

RAW_STRING = re.compile(r'(?:u8|u|U|L)?R"([^()\\\s]{0,16})\(')


DIGIT_SEPARATOR = re.compile(r"(?<![\w'])[0-9][\w']*$")


def identifier_before(source, i):
    return i > 0 and (source[i - 1].isalnum() or source[i - 1] == '_')


def blank(text):
    return re.sub(r'[^\n]', ' ', text)


def strip_code(source):
    """Lines of `source` with comments, literal contents and preprocessor lines blanked.

    Columns and line numbers are kept; quotes stay so `extern "C"` remains recognizable."""
    out, i, n = [], 0, len(source)
    while i < n:
        c = source[i]
        if source.startswith('//', i):
            end = source.find('\n', i)
            end = n if end < 0 else end
            out.append(blank(source[i:end]))
            i = end
        elif source.startswith('/*', i):
            end = source.find('*/', i + 2)
            end = n if end < 0 else end + 2
            out.append(blank(source[i:end]))
            i = end
        elif c in 'uULR' and RAW_STRING.match(source, i) and not identifier_before(source, i):
            raw = RAW_STRING.match(source, i)
            close = ')' + raw.group(1) + '"'
            end = source.find(close, raw.end())
            end = n if end < 0 else end + len(close)
            out.append('"' + blank(source[i + 1:end - 1]) + '"')
            i = end
        elif c == '\'' and DIGIT_SEPARATOR.search(source, max(0, i - 40), i):
            out.append(c)  # 1'000
            i += 1
        elif c in '"\'':
            j = i + 1
            while j < n and source[j] != c and source[j] != '\n':
                j += 2 if source[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(c + blank(source[i + 1:j - 1]) + source[j - 1:j] if j - 1 > i else c)
            i = j
        else:
            out.append(c)
            i += 1
    lines = ''.join(out).split('\n')
    continued = False
    for index, line in enumerate(lines):
        if continued or line.lstrip().startswith('#'):
            continued = line.rstrip().endswith('\\')
            lines[index] = blank(line)
    return lines


def brace_kind(text):
    if re.search(r'\bnamespace\b[\w:\s]*$', text) or re.fullmatch(r'extern\s*"\s*"', text):
        return 'namespace'
    if re.match(r'(template\s*<.*>\s*)?(class|struct|union)\b', text):
        return 'class'
    if re.match(r'enum\b', text):
        return 'enum'
    return 'other'


def statements(code_lines):
    """Split blanked code at ';' (outside parentheses), '{' and '}' with their scopes."""
    result, stack = [], []
    text, start, parens, closed = [], None, 0, ''

    def emit(end):
        joined = re.sub(r'\[\[.*?\]\]', ' ', ' '.join(''.join(text).split())).strip()
        if joined or end != ';':
            result.append(Statement(joined, start or line_number, end,
                                    tuple(k for k, _ in stack), closed))
        return joined

    for line_number, line in enumerate(code_lines, 1):
        for c in line:
            if c == '{':
                kind = brace_kind(emit('{'))
                stack.append((kind, parens))
                text, start, parens, closed = [], None, 0, ''
            elif c == '}':
                emit('}')
                closed, parens = stack.pop() if stack else ('', 0)
                text, start = [], None
            elif c == ';' and parens <= 0:
                emit(';')
                text, start, closed = [], None, ''
            else:
                parens += (c == '(') - (c == ')')
                if start is None and not c.isspace():
                    start = line_number
                text.append(c)
        text.append(' ')
    return result


def added_lines(source_lines, previous_lines):
    """1-based numbers of lines in `source_lines` that are new relative to `previous_lines`."""
    if previous_lines is None:
        return set(range(1, len(source_lines) + 1))
    added = set()
    matcher = difflib.SequenceMatcher(None, previous_lines, source_lines, autojunk=False)
    for tag, _, _, j1, j2 in matcher.get_opcodes():
        if tag in ('replace', 'insert'):
            added.update(range(j1 + 1, j2 + 1))
    return added


# ---- Signatures --------------------------------------------------------------------

NOT_FUNCTIONS = {'if', 'for', 'while', 'switch', 'catch', 'return', 'sizeof', 'alignof',
                 'decltype', 'static_assert', 'noexcept', 'alignas', 'requires', 'throw',
                 'operator', 'new', 'delete', 'typeid', 'co_return', 'co_await'}


def matching(text, open_index, opening='(', closing=')'):
    depth = 0
    for k in range(open_index, len(text)):
        depth += (text[k] == opening) - (text[k] == closing)
        if depth == 0:
            return k
    return -1


def split_top_level(text):
    parts, depth, current = [], 0, []
    for c in text:
        if c in '(<[{':
            depth += 1
        elif c in ')>]}':
            depth -= 1
        if c == ',' and depth == 0:
            parts.append(''.join(current).strip())
            current = []
        else:
            current.append(c)
    tail = ''.join(current).strip()
    return parts + [tail] if tail or parts else parts


@dataclass(frozen=True)
class Signature:
    name: str
    params: tuple
    tail: str


def signature(statement):
    """The function a declaration or definition header names, or None."""
    text = statement.text
    if statement.end == '}' or not statement.declaration_scope:
        return None
    if re.match(r'(using|typedef|friend|return|namespace|class|struct|enum|union)\b', text):
        return None
    for match in re.finditer(r'(~?[A-Za-z_]\w*(?:\s*::\s*~?[A-Za-z_]\w*)*)\s*\(', text):
        name = match.group(1).replace(' ', '')
        if name.split('::')[-1] in NOT_FUNCTIONS or '=' in text[:match.start()]:
            continue
        close = matching(text, match.end() - 1)
        if close < 0:
            return None
        tail = text[close + 1:].strip()
        if re.search(r'[(=](?!\s*(0|default|delete)\s*$)', tail) and not tail.startswith('->'):
            return None
        return Signature(name, tuple(split_top_level(text[match.end():close])), tail)
    return None


def without_templates(text):
    while True:
        reduced = re.sub(r'<[^<>]*>', '', text)
        if reduced == text:
            return text
        text = reduced


def default_split(param):
    depth = 0
    for k, c in enumerate(param):
        depth += (c in '(<[{') - (c in ')>]}')
        if c == '=' and depth == 0 and param[k - 1:k] not in ('=', '!', '<', '>') \
                and param[k + 1:k + 2] != '=':
            return param[:k].strip(), param[k + 1:].strip()
    return param.strip(), None


BOOL_PARAM = re.compile(r'(const\s+)?bool(\s+[A-Za-z_]\w*)?')


def bool_params(sig):
    return [(p, default) for p, default in map(default_split, sig.params)
            if BOOL_PARAM.fullmatch(p)]


def top_level_const(param):
    declared, _ = default_split(param)
    plain = without_templates(declared)
    if '&' in plain or '*' in plain or '(' in plain or '...' in plain:
        return False
    return bool(re.fullmatch(r'const\s+[\w:]+(\s+[\w:]+)*', plain)
                or re.fullmatch(r'[\w:]+\s+const(\s+\w+)?', plain))


# ---- Checks ------------------------------------------------------------------------

def rule(message, *cites):
    return f'style: {message} ({", ".join(cites)})' if cites else f'style: {message}'


BANS = (
    (re.compile(r'\bstd::filesystem\b'), ('Common/Cpp/Filesystem/',),
     'std::filesystem is banned; use the Common/Cpp/Filesystem wrappers (Path, Filesystem::, FileIO)',
     'upstream AGENTS.md "Filesystem"'),
    (re.compile(r'\bstd::j?thread\b(?!\s*::)'), ('Common/Cpp/Concurrency/',),
     'std::thread is banned; run background work on the pools in GlobalThreadPools.h',
     'upstream AGENTS.md "Concurrency"'),
    (re.compile(r'\bstd::(?:(?:recursive_|timed_|recursive_timed_|shared_|shared_timed_)?mutex'
                r'|condition_variable(?:_any)?)\b'), ('Common/Cpp/Concurrency/',),
     'std::mutex / std::condition_variable are banned; use PokemonAutomation::Mutex / ConditionVariable',
     'upstream AGENTS.md "Concurrency"', 'TotW #197'),
    (re.compile(r'\bstd::async\b'), (),
     'std::async is banned (it may spawn a thread); use the thread pools',
     'upstream AGENTS.md "Concurrency"'),
)
BIND = re.compile(r'\bstd::bind\s*\(')
NEW = re.compile(r'(?<!operator)(?<!operator )\bnew\b(?!\s*\()')
DELETE = re.compile(r'(?<!=)(?<!= )(?<!operator)(?<!operator )\bdelete\b')
RETURN_MOVE = re.compile(r'\breturn\s+std::move\s*\(\s*([A-Za-z_]\w*)\s*\)\s*;')
QT_INCLUDE = re.compile(r'^\s*#\s*include\s*<Q', re.M)
CONTROL = re.compile(r'\b(if(?:\s+constexpr)?|for|while|switch)\s*\(')


def function_start(bodies, line):
    """First line of the function definition that contains `line` (1 if none)."""
    return max((start for start in bodies if start <= line), default=1)


def lvalue_reference(name, text):
    """Whether `text` declares `name` as an lvalue reference (moving from it is not redundant)."""
    return bool(re.search(r'[\w>]\s*&\s*' + re.escape(name) + r'\b(?!\s*[(.\[])', text))


def line_checks(path, raw, code, added, statements_):
    """Bans, expressions and formatting that one line shows."""
    errors = []
    allocator = 'alloc' in path.lower() or bool(QT_INCLUDE.search(raw))
    bodies = [s.line for s in statements_ if s.end == '{' and signature(s)]
    for number in sorted(added):
        line = code[number - 1]
        for pattern, exempt, message, *cites in BANS:
            if pattern.search(line) and not path.startswith(exempt or ('\0',)):
                errors.append(Finding(path, number, rule(message, *cites)))
        if BIND.search(line):
            errors.append(Finding(path, number, rule('use a lambda, not std::bind', 'TotW #108')))
        if not allocator and (NEW.search(line) or DELETE.search(line)):
            errors.append(Finding(path, number, rule(
                'no new/delete outside allocators; use std::make_unique / std::make_shared',
                'TotW #126')))
        moved = RETURN_MOVE.search(line)
        if moved and not moved.group(1).startswith('m_') and not lvalue_reference(
                moved.group(1), '\n'.join(code[function_start(bodies, number) - 1:number])):
            errors.append(Finding(path, number, rule(
                f"don't std::move a returned local ('{moved.group(1)}'); return it by name",
                'TotW #120')))
        errors.extend(Finding(path, number, rule(message, 'upstream AGENTS.md "Control flow formatting"'))
                      for message in brace_problems(line, code[number] if number < len(code) else ''))
    return errors


def brace_problems(line, next_line):
    problems = []
    if re.search(r'\}[ \t]+else\b|\belse[ \t]+\{', line):
        problems.append('write }else{ and }else if (...){ with no spaces around else')
    if re.search(r'\bnamespace\b[\w:\s]*?[ \t]+\{', line):
        problems.append('write namespace X{ with no space before {')
    control = CONTROL.search(line)
    if control:
        close = matching(line, control.end() - 1)
        rest = line[close + 1:].strip() if close >= 0 else None
        if rest is None or (control.group(1) == 'while' and rest == ';'):
            pass
        elif re.match(r'\s+\{', line[close + 1:]):
            problems.append(f'no space between ) and {{: {control.group(1)} (...){{')
        elif rest == '' and next_line.strip().startswith('{'):
            problems.append(f'put the opening brace on the {control.group(1)} line')
        elif rest and (not rest.startswith('{') or rest.rstrip('{').strip() or rest.count('}')):
            if not (rest.startswith('{') and rest[1:].strip() == ''):
                problems.append(f'{control.group(1)} body goes on its own lines, braced')
    other_else = re.search(r'\belse\b(?!\s*(if\b|\{|$))', line)
    if other_else:
        problems.append('else body goes on its own lines, braced')
    return problems


def is_forward_declaration(statement):
    return statement.end == ';' and re.fullmatch(
        r'(template\s*<.*>\s*)?(class|struct|union)\s+[A-Za-z_][\w:]*', statement.text)


def namespace_variable(statement):
    """The name of a non-const variable this namespace-scope statement declares, or None."""
    text = statement.text
    if not statement.namespace_scope or statement.end == '}' or statement.follows:
        return None
    if statement.end == '{' and not text.endswith('='):
        if '(' in text or not re.fullmatch(r'[\w:<>,\s\*&]+\s[\*&]*[A-Za-z_]\w*', text):
            return None
    head = default_split(text.rstrip('=').strip() if statement.end == '{' else text)[0]
    if re.match(r'(using|typedef|namespace|template|class|struct|union|enum|friend|static_assert'
                r'|return|extern\s*")\b', head) or '(' in without_templates(head):
        return None
    match = re.fullmatch(r'((?:[\w:]+(?:<.*>)?[\s\*&]+)+?)[\*&\s]*([A-Za-z_]\w*)\s*(\[[^\]]*\])?', head)
    if not match:
        return None
    words = set(re.findall(r'\w+', without_templates(match.group(1))))
    if words & {'const', 'constexpr'} or match.group(2) in ('const', 'constexpr'):
        return None
    if words <= {'static', 'inline', 'extern', 'thread_local', 'constinit', 'volatile'}:
        return None
    return match.group(2)


def statement_checks(path, statements_, added, previous_signatures, new_file):
    errors, warnings = [], []
    header = path.endswith(HEADERS)
    # A primary template declared before its specializations is not a forward declaration.
    defined = {m.group(1) for s in statements_ if s.end == '{'
               for m in [re.search(r'\b(?:class|struct|union)\s+(\w+)', s.text)] if m}
    for statement in statements_:
        if statement.line not in added:
            continue
        text = statement.text
        if header and statement.namespace_scope and re.match(r'using\s+namespace\b', text):
            errors.append(Finding(path, statement.line, rule(
                'no using-directive at namespace scope in a header', 'TotW #153')))
        variable = namespace_variable(statement)
        if variable:
            what = 'non-const extern' if re.match(r'(static\s+|inline\s+)*extern\b', text) \
                else 'non-const namespace-scope variable'
            errors.append(Finding(path, statement.line, rule(
                f"{what} '{variable}'; make it const/constexpr or pass options down in a struct",
                'TotW #103')))
        if header and statement.namespace_scope and is_forward_declaration(statement) \
                and text.split()[-1] not in defined:
            finding = Finding(path, statement.line, rule(
                f"forward declaration '{text}'; include the header that defines it",
                'fork AGENTS.md "Forward declarations"'))
            (errors if new_file else warnings).append(finding)
        sig = signature(statement)
        if sig is None:
            continue
        if header and statement.end == ';':
            for param in sig.params:
                if top_level_const(param):
                    errors.append(Finding(path, statement.line, rule(
                        f"{sig.name}: drop top-level const from by-value parameter '{param}' "
                        'in the declaration', 'TotW #109')))
        bools = bool_params(sig)
        defaulted = [p for p, default in bools if default is not None]
        if (len(bools) >= 2 or defaulted) and not re.search(r'\b(override|final)\b', sig.tail) \
                and (sig.name, len(bools), len(defaulted)) not in previous_signatures:
            problem = 'a defaulted bool parameter' if defaulted else f'{len(bools)} bool parameters'
            errors.append(Finding(path, statement.line, rule(
                f'{sig.name}: {problem}; use an enum class or an options struct', 'TotW #94')))
    return errors, warnings


def bool_signatures(statements_):
    result = set()
    for statement in statements_:
        sig = signature(statement)
        if sig:
            bools = bool_params(sig)
            result.add((sig.name, len(bools), sum(d is not None for _, d in bools)))
    return result


# ---- Function comments (advisory) ----------------------------------------------------

STOP_WORDS = {'the', 'and', 'for', 'from', 'with', 'get', 'set', 'run', 'try', 'all'}


def name_tokens(name):
    return [t for t in name.split('::')[-1].lower().split('_') if len(t) >= 3 and t not in STOP_WORDS]


def prose_words(text):
    """(first sentence, rest) of a comment as sets of lowercase words, without code
    references (`x`, a_b, f(), A::B)."""
    text = re.sub(r'^\s*//+', ' ', text, flags=re.M)
    text = re.sub(r'`[^`]*`|\b\w+\(\)|\b\w*_\w*\b|\b\w+::\w+', ' ', text)
    parts = re.split(r'(?<=[a-z0-9)])\.(?:\s|$)', text.strip(), maxsplit=1) + ['']
    return tuple(set(re.findall(r'[a-z0-9]+', part.lower())) for part in parts[:2])


def mentioned(token, words):
    """Whether a word shares the token's stem: berry/berries, add/adds, save/saving."""
    stem = token[:-1] if len(token) >= 4 else token
    return any(w.startswith(stem) or (len(w) >= 4 and token.startswith(w)) for w in words)


def comment_above(raw_lines, line):
    """(first line, text) of the // comment block directly above `line`, or None."""
    k = line - 1
    while k >= 1 and raw_lines[k - 1].strip().startswith('//'):
        k -= 1
    if k == line - 1:
        return None
    return k + 1, '\n'.join(raw_lines[k:line - 1])


def describes(name, words, share=1.0):
    tokens = name_tokens(name)
    return bool(tokens) and sum(mentioned(t, words) for t in tokens) >= share * len(tokens)


def comment_checks(path, raw_lines, statements_, added):
    """A comment whose first sentence describes a neighbouring function and none of its own,
    when its own function is described later in the block or the neighbour has no comment:
    a stale comment left above another, or a comment that slid one function down."""
    functions = [(s.line, signature(s)) for s in statements_]
    functions = [(line, sig.name) for line, sig in functions if sig]
    warnings = []
    for index, (line, name) in enumerate(functions):
        block = comment_above(raw_lines, line)
        if block is None or not added & set(range(block[0], line + 1)):
            continue
        first, rest = prose_words(block[1])
        if any(mentioned(t, first) for t in name_tokens(name)):
            continue
        for other_line, other in functions[max(0, index - 1):index] + functions[index + 1:index + 2]:
            if other == name or len(name_tokens(other)) < 2 or not describes(other, first):
                continue
            if describes(name, rest, 0.5) or comment_above(raw_lines, other_line) is None:
                warnings.append(Finding(path, block[0], rule(
                    f'this comment sits above {name} but its first sentence describes {other}; '
                    'function comments go directly above their function',
                    'CONTRIBUTING', 'Google "Function Comments"')))
                break
    return warnings


# ---- Header and native test structure -----------------------------------------------

def scopes(parsed):
    """Yield each statement and its enclosing (kind, name) brace frames."""
    stack = []
    for statement in parsed:
        yield statement, tuple(stack)
        if statement.end == '{':
            kind = brace_kind(statement.text)
            if re.fullmatch(r'extern\s*"\s*"', statement.text):
                kind = 'linkage'
            match = re.fullmatch(r'(?:inline\s+)?namespace\s*([\w:]*)', statement.text)
            if kind == 'class':
                match = re.search(r'\b(?:class|struct|union)\s+(\w+)', statement.text)
            stack.append((kind, match.group(1) if match else ''))
        elif statement.end == '}' and stack:
            stack.pop()


def namespace_closes(path, lines, parsed, changed):
    errors = []
    for statement, enclosing in scopes(parsed):
        if statement.end != '}' or statement.line not in changed or not enclosing:
            continue
        kind, name = enclosing[-1]
        if kind != 'namespace':
            continue
        expected = '// namespace' + (' ' + name if name else '')
        if not re.search(r'}\s*' + re.escape(expected) + r'\s*$', lines[statement.line - 1]):
            errors.append(Finding(path, statement.line,
                                  f'namespace-close: close this namespace with {expected}'))
    return errors


def parameter_type(param):
    """Ignore ordinary parameter names/defaults when matching declarations."""
    declared = default_split(param)[0]
    last = re.search(r'\b([A-Za-z_]\w*)$', declared)
    if not last:
        return declared
    prefix = declared[:last.start()].rstrip()
    qualifiers = {'const', 'volatile', 'unsigned', 'signed', 'long', 'short'}
    if not prefix or prefix.endswith('::') or set(prefix.split()) <= qualifiers:
        return declared
    return prefix


def function_key(statement, enclosing):
    sig = signature(statement)
    if sig is None or not statement.namespace_scope:
        return None
    return (enclosing, sig.name, tuple(parameter_type(param) for param in sig.params))


def template_api(path, parsed, changed):
    """Template free functions declare their interface before implementation."""
    errors, declared = [], set()
    for statement, enclosing in scopes(parsed):
        key = function_key(statement, enclosing)
        if key is None or not statement.text.startswith('template'):
            continue
        helper = any(internal(name) for _, name in enclosing)
        if helper:
            if statement.end == ';' and statement.line in changed:
                errors.append(Finding(path, statement.line,
                    'template-api-internal: define internal function templates in place'))
            continue
        if statement.end == ';':
            declared.add(key)
        elif statement.end == '{' and key not in declared and statement.line in changed:
            errors.append(Finding(path, statement.line,
                                  'template-api: declare the function template before defining it'))
    errors += template_order(path, parsed, changed)
    return errors


def internal(name):
    return name.endswith('Internal') or name in ('detail', 'internal')


def template_order(path, parsed, changed):
    declarations = [(enclosing, statement.line) for statement, enclosing in scopes(parsed)
                    if statement.end == ';' and statement.text.startswith('template')
                    and function_key(statement, enclosing)
                    and not any(internal(name) for _, name in enclosing)]
    errors = []
    for statement, enclosing in scopes(parsed):
        match = re.fullmatch(r'namespace\s+(\w+)', statement.text)
        if statement.end != '{' or not match or not internal(match.group(1)):
            continue
        if any(parent == enclosing and line > statement.line
               and (statement.line in changed or line in changed)
               for parent, line in declarations):
            errors.append(Finding(path, statement.line,
                                  'template-api-order: declare public templates before internal helpers'))
    return errors


def documented(lines, line):
    """Accept adjacent // prose or a closed block comment, including Doxygen."""
    if line < 2:
        return False
    above = lines[line - 2].strip()
    if above.startswith('//'):
        return bool(above.lstrip('/').strip())
    if not above.endswith('*/'):
        return False
    for earlier in reversed(lines[:line - 1]):
        if '/*' in earlier:
            return True
        if not earlier.strip().startswith(('*', '//')):
            return False
    return False


def header_comments(path, lines, parsed, changed):
    errors, documented_functions = [], set()
    for statement, enclosing in scopes(parsed):
        text = statement.text
        if not statement.namespace_scope or statement.end == '}':
            continue
        if not text or re.match(r'(?:inline\s+)?namespace\b|(?:static_assert|extern\s*")', text):
            continue
        key = function_key(statement, enclosing)
        has_comment = documented(lines, statement.line)
        if has_comment and key:
            documented_functions.add(key)
        if statement.line in changed and not has_comment and key not in documented_functions:
            errors.append(Finding(path, statement.line,
                                  'header-comment: document each namespace-scope header symbol'))
    return errors


def structures(parsed):
    """Local aggregate definitions and their directly declared fields."""
    found = {}
    for statement, enclosing in scopes(parsed):
        match = re.match(r'struct\s+(\w+)\b', statement.text)
        if match and statement.end == '{':
            found[match.group(1)] = {}
        elif enclosing and enclosing[-1][0] == 'class' and statement.end == ';':
            fields = found.get(enclosing[-1][1])
            field = re.fullmatch(r'([\w:<> ,]+?)\s+(\w+)\s*(?:=.*)?', statement.text)
            if fields is not None and field:
                fields[field.group(2)] = field.group(1)
    return found


def case_type(test_name, parsed):
    """The single aggregate parameter of this UnitTest constructor, if present."""
    for statement, enclosing in scopes(parsed):
        if not enclosing or enclosing[-1] != ('class', test_name):
            continue
        constructor = re.match(r'(?:(?:public|protected|private)\s*:\s*)?(?:explicit\s+)?' + re.escape(test_name) + r'\s*\(', statement.text)
        if constructor:
            end = matching(statement.text, constructor.end() - 1)
            params = split_top_level(statement.text[constructor.end():end])
            if len(params) == 1:
                param = re.fullmatch(r'(?:const\s+)?(\w+)\s*(?:&&|&)?\s+\w+', params[0])
                return param.group(1) if param else None
    return None


def designated_case(row):
    row = row.strip()
    if not row.startswith('{') or not row.endswith('}'):
        return False
    fields = [field for field in split_top_level(row[1:-1]) if field]
    names = [re.match(r'\.(input|expectation)\s*=', field) for field in fields]
    return len(names) == 2 and all(names) and {name.group(1) for name in names} == {'input', 'expectation'}


def case_table(case, code):
    """A named aggregate table consumed by a range-for, not ad hoc registrations."""
    element = re.escape(case)
    declaration = re.compile(
        r'(?:const\s+|constexpr\s+|static\s+|inline\s+)*'
        r'(?:' + element + r'\s+(\w+)\s*\[\s*\]|'
        r'std::(?:vector|array)\s*<\s*' + element + r'(?:\s*,[^>]+)?\s*>\s+(\w+))\s*(?:=|{)')
    for table in declaration.finditer(code):
        name = table.group(1) or table.group(2)
        opening = code.find('{', table.end() - 1)
        closing = matching(code, opening, '{', '}') if opening >= 0 else -1
        if closing < 0:
            continue
        rows = [row for row in split_top_level(code[opening + 1:closing]) if row]
        designated = all(designated_case(row) for row in rows)
        if rows and designated and re.search(r'\bfor\s*\([^;]*?:\s*' + re.escape(name) + r'\s*\)', code):
            return True
    return False


def native_cases(path, code, parsed, changed):
    """New or changed native UnitTest classes use one structured, table-driven scenario."""
    definitions, errors = structures(parsed), []
    for statement, _ in scopes(parsed):
        test = re.match(r'(?:class|struct)\s+(\w+)\s*:\s*public\s+(?:[\w:]+::)?UnitTest\b', statement.text)
        if not test or statement.end != '{':
            continue
        edited = statement.line in changed or any(
            nested.line in changed and ('class', test.group(1)) in enclosing
            for nested, enclosing in scopes(parsed))
        if not edited:
            continue
        case = case_type(test.group(1), parsed)
        fields = definitions.get(case, {})
        valid_fields = set(fields) == {'input', 'expectation'} and all(
            field in definitions for field in fields.values())
        if not case or not valid_fields or not case_table(case, code):
            errors.append(Finding(path, statement.line,
                'test-case: construct each UnitTest from one case struct with input and expectation '
                'struct fields; register a named table of designated cases in a range-for'))
    return errors



# ---- Entry point -------------------------------------------------------------------

def check(path, source, previous_source):
    """(errors, warnings) for the style rules on lines this change adds."""
    raw = source.replace('\r\n', '\n').replace('\r', '\n')
    raw_lines = raw.split('\n')
    previous_lines = None
    previous_statements = []
    if previous_source is not None:
        previous = previous_source.replace('\r\n', '\n').replace('\r', '\n')
        previous_lines = previous.split('\n')
        previous_statements = statements(strip_code(previous))
    added = added_lines(raw_lines, previous_lines)
    if not added:
        return [], []
    code = strip_code(raw)
    statements_ = statements(code)
    errors = line_checks(path, raw, code, added, statements_)
    more_errors, warnings = statement_checks(path, statements_, added,
                                             bool_signatures(previous_statements),
                                             previous_source is None)
    errors += namespace_closes(path, raw_lines, statements_, added)
    if path.endswith('_Tests.cpp'):
        errors += native_cases(path, '\n'.join(code), statements_, added)
    if path.endswith(HEADERS):
        errors += template_api(path, statements_, added)
        errors += header_comments(path, raw_lines, statements_, added)
    warnings += comment_checks(path, raw_lines, statements_, added)
    return sorted(errors + more_errors, key=lambda f: f.line), sorted(warnings, key=lambda f: f.line)
