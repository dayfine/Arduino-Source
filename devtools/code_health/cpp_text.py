"""Comments and string literals of C++ source, with their line numbers.

Shares `style.strip_code`'s lexing (comments, raw strings, digit separators), so a `//` inside
a string or a quote inside a comment is read the same way by every check."""
import re

from .style import strip_code

STRING = re.compile(r'"( *)"')


def normalize(source):
    return source.replace('\r\n', '\n').replace('\r', '\n')


def comment_lines(source):
    """{line: comment text on that line}, the `//`, `/*`, `*/` and leading `*` removed.

    A character is comment text when `strip_code` blanked it and it lies outside the quotes
    that `strip_code` keeps around literals."""
    raw_lines = normalize(source).split('\n')
    code_lines = strip_code(normalize(source))
    found = {}
    for number, (raw, code) in enumerate(zip(raw_lines, code_lines), 1):
        if not code.strip() and raw.lstrip().startswith('#'):
            continue  # strip_code blanks preprocessor lines whole; they hold no comments we read
        text, quote = [], None
        for raw_char, code_char in zip(raw, code):
            if code_char in '"\'' and quote in (None, code_char):
                quote = None if quote else code_char
            elif code_char == ' ' and raw_char != ' ' and quote is None:
                text.append(raw_char)
            elif text and text[-1] != ' ':
                text.append(' ')
        cleaned = re.sub(r'^\s*(?:/[/*]+|\*+(?!/))\s*|\*+/\s*$', '', ''.join(text).strip()).strip()
        if cleaned:
            found[number] = cleaned
    return found


def comment_blocks(source):
    """[(first line, {line: text})] for runs of consecutive comment lines."""
    blocks, current, last = [], {}, None
    for number, text in sorted(comment_lines(source).items()):
        if last is not None and number != last + 1:
            blocks.append(current)
            current = {}
        current[number] = text
        last = number
    if current:
        blocks.append(current)
    return blocks


def string_literals(source):
    """[(line, start column, end column, value)] for one-line string literals.

    Columns index the normalized line; `end` is one past the closing quote. Escapes stay as
    written, which is enough for the identifier-like values the checks compare."""
    raw_lines = normalize(source).split('\n')
    found = []
    for number, (raw, code) in enumerate(zip(raw_lines, strip_code(normalize(source))), 1):
        for match in STRING.finditer(code):
            found.append((number, match.start(), match.end(), raw[match.start() + 1:match.end() - 1]))
    return found
