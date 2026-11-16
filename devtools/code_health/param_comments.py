"""Parameter comments on literal arguments: `f(x, /*count=*/3, /*cool=*/0ms)`.

In a call with three or more arguments, a bare literal says nothing about the parameter it
fills, so it needs a `/*param=*/` comment (Google "Function Arguments"; the user's review rule
on fork #590). Literals are numbers (with suffixes, so `0ms` and `1.5f`), `true`, `false`,
`nullptr` and brace lists of those (`{0, 0}`, `{}`). Only literals on added lines count.

Not reported, because the call itself already names the values:
- string and character literals: a message, key or label reads as its own name;
- macros (ALL_CAPS callees) and `std::` functions: no parameter names / well-known signatures;
- format and log calls (callee name contains format, print or log, or the first argument is a
  string literal): the format string's placeholders say what each argument is;
- a `nullptr` first argument: the repository's null logger or Qt parent slot
  (`InternalProgramError(nullptr, PA_CURRENT_FUNCTION, message)`, `critical(nullptr, ...)`);
- value-type constructors (`ImageFloatBox`, `ImagePixelBox`, `Color`, a `*Point` or `*Rect`):
  the type names the fields;
- a lone trailing duration in a call of at most four arguments
  (`save_button_shows(console, context, 5s)`): the call's timeout or length, and the function
  name says which. Longer calls (`ssf_press_button(context, buttons, delay, hold, 0ms)`) pass
  several durations, so the slot is ambiguous and is still reported;
- calls whose arguments are all plain numbers: a box, color or point tuple whose type names
  the fields (`ImageFloatBox(0.1, 0.2, 0.3, 0.4)`).
When the callee is declared in the same file with exactly one arity match, a comment that names
a different parameter is reported too. Lexical, like `style.py`: calls are only looked for inside
brace bodies, never in declarations.
"""
import bisect
import re

from .findings import Finding
from .style import NOT_FUNCTIONS, added_lines, brace_kind, default_split, matching, rule, signature, \
    statements, strip_code

MIN_ARGUMENTS = 3
SHORT_CALL = 4
CALL = re.compile(r'(?<![\w.])((?:[A-Za-z_]\w*\s*(?:::|\.|->)\s*)*[A-Za-z_]\w*)\s*\(')
NUMBER = r"[-+]?\s*(?:0[xXbB][\da-fA-F']+|\d[\d']*\.?[\d']*(?:[eE][-+]?\d+)?|\.\d+)[A-Za-z_]*"
PLAIN_NUMBER = re.compile(r"[-+]?\s*(?:0[xX][\da-fA-F']+|\d[\d']*\.?[\d']*(?:[eE][-+]?\d+)?|\.\d+)[fFuUlL]*")
DURATION = re.compile(r"\d[\d']*(?:ms|s|min|h|us|ns)")
SCALAR = NUMBER + r'|true|false|nullptr'
LITERAL = re.compile(r'(?:' + SCALAR + r')|\{\s*(?:(?:' + SCALAR + r')(?:\s*,\s*(?:' + SCALAR + r'))*)?\s*\}')
STRING = re.compile(r'(?:(?:u8|u|U|L)?"[^"]*"\s*)+|\'[^\']*\'')
COMMENT = re.compile(r'/\*\s*([A-Za-z_]\w*)\s*=\s*\*/')
MACRO = re.compile(r'[A-Z][A-Z0-9_]*')
VALUE_TYPE = re.compile(r'\w*(?:Box|Color|Point|Rect)')
FORMAT_LIKE = re.compile(r'format|print|log', re.I)
CITE = ('Google "Function Arguments"', 'user review on fork #590')


def body_mask(code_text, parsed):
    """Per offset of `code_text`, whether it lies inside a function body or a nested brace.

    `statements` emits one statement per `{`, in order, so the k-th `{` pairs with the k-th one."""
    opening = iter([brace_kind(s.text) for s in parsed if s.end == '{'])
    mask, stack = bytearray(len(code_text)), []
    for offset, c in enumerate(code_text):
        if c == '{':
            stack.append(next(opening, 'other'))
        elif c == '}' and stack:
            stack.pop()
        mask[offset] = 'other' in stack
    return mask


def arguments(code_text, open_index, close_index):
    """(start, end) offsets of each top-level argument between the parentheses."""
    spans, depth, start = [], 0, open_index + 1
    for k in range(open_index + 1, close_index):
        c = code_text[k]
        depth += (c in '([{') - (c in ')]}')
        if c == ',' and depth == 0:
            spans.append((start, k))
            start = k + 1
    if code_text[start:close_index].strip() or spans:
        spans.append((start, close_index))
    return spans


def exempt(name, args):
    """Whether the call names its values another way (see the module docstring)."""
    short = re.split(r'::|\.|->', name.replace(' ', ''))[-1]
    if short in NOT_FUNCTIONS or MACRO.fullmatch(short) or name.replace(' ', '').startswith('std::'):
        return True
    if VALUE_TYPE.fullmatch(short) or FORMAT_LIKE.search(short) or STRING.fullmatch(args[0]):
        return True
    literals = [arg for arg in args if LITERAL.fullmatch(arg)]
    if len(args) <= SHORT_CALL and literals == [args[-1]] and DURATION.fullmatch(args[-1]):
        return True
    return all(PLAIN_NUMBER.fullmatch(arg) for arg in args)


def declared_parameters(parsed):
    """{function name: [parameter names]} for names declared once per arity in this file."""
    found = {}
    for statement in parsed:
        sig = signature(statement)
        if sig is None:
            continue
        names = []
        for param in sig.params:
            last = re.search(r'([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?$', default_split(param)[0])
            names.append(last.group(1) if last else '')
        key = (sig.name.split('::')[-1], len(names))
        found.setdefault(key, set()).add(tuple(names))
    return {key: list(next(iter(v))) for key, v in found.items() if len(v) == 1}


def check(path, source, previous_source):
    """Advisories for uncommented literal arguments on lines this change adds."""
    raw = source.replace('\r\n', '\n').replace('\r', '\n')
    previous = None if previous_source is None else \
        previous_source.replace('\r\n', '\n').replace('\r', '\n').split('\n')
    added = added_lines(raw.split('\n'), previous)
    if not added:
        return []
    code = strip_code(raw)
    code_text = '\n'.join(code)
    parsed = statements(code)
    mask = body_mask(code_text, parsed)
    params = declared_parameters(parsed)
    line_of = line_index(code_text)
    findings = []
    for match in CALL.finditer(code_text):
        if not mask[match.start()]:
            continue
        open_index = match.end() - 1
        close_index = matching(code_text, open_index)
        if close_index < 0:
            continue
        spans = arguments(code_text, open_index, close_index)
        args = [code_text[a:b].strip() for a, b in spans]
        if len(args) < MIN_ARGUMENTS or exempt(match.group(1), args):
            continue
        finding = call_finding(path, match.group(1), spans, (code_text, raw), line_of, added, params)
        if finding:
            findings.append(finding)
    return findings


def call_finding(path, name, spans, texts, line_of, added, params):
    """One finding per call: its uncommented literals, or comments naming the wrong parameter.

    `texts` is (blanked code, raw source); offsets match because blanking keeps columns."""
    code_text, raw = texts
    short = re.split(r'::|\.|->', name.replace(' ', ''))[-1]
    declared = params.get((short, len(spans)))
    missing, wrong, line = [], [], None
    for index, (start, end) in enumerate(spans):
        arg = code_text[start:end].strip()
        at = line_of(start + len(code_text[start:end]) - len(code_text[start:end].lstrip()))
        if at not in added or (index == 0 and arg == 'nullptr'):
            continue
        comment = COMMENT.search(raw[start:end])
        if comment and declared and declared[index] and comment.group(1) != declared[index]:
            wrong.append(f'/*{comment.group(1)}=*/ names parameter \'{declared[index]}\'')
            line = line or at
        elif not comment and LITERAL.fullmatch(arg):
            missing.append(arg)
            line = line or at
    if not missing and not wrong:
        return None
    problems = []
    if missing:
        plural = len(missing) > 1
        problems.append(f'literal argument{"s" if plural else ""} {", ".join(missing)} '
                        f'need{"" if plural else "s"} a /*param=*/ comment')
    problems += wrong
    return Finding(path, line, rule(f'{short}(): {"; ".join(problems)}', *CITE), check='param_comment')


def line_index(text):
    """A function from an offset in `text` to its 1-based line."""
    starts = [0] + [k + 1 for k, c in enumerate(text) if c == '\n']
    return lambda offset: bisect.bisect_right(starts, offset)
