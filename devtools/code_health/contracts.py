"""Structural C++ contracts on changed declarations, using the style lexer's scopes.

This is deliberately not a C++ type checker: included types and overload resolution
remain compiler responsibilities. Literals/comments/preprocessor text never declare
symbols. Existing declarations and namespace closes are grandfathered.
"""
import re

from .findings import Finding
from .style import HEADERS, added_lines, brace_kind, default_split, matching, signature, split_top_level, statements, strip_code


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


def check(path, source, previous_source):
    """Return structural findings; unchanged declarations are grandfathered."""
    lines = source.splitlines()
    changed = added_lines(lines, previous_source.splitlines() if previous_source is not None else None)
    parsed = statements(strip_code(source))
    errors = namespace_closes(path, lines, parsed, changed)
    if path.endswith("_Tests.cpp"):
        errors += native_cases(path, "\n".join(strip_code(source)), parsed, changed)
    if path.endswith(HEADERS):
        errors += template_api(path, parsed, changed)
        errors += header_comments(path, lines, parsed, changed)
    return errors
