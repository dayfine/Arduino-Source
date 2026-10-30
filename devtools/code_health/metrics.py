"""C++ measurements, independent of repository layout and baseline policy."""
from contextlib import redirect_stderr
from dataclasses import dataclass
from io import StringIO

import lizard


@dataclass(frozen=True)
class Metric:
    path: str
    symbol: str
    check: str
    value: int
    limit: int
    line: int = 1

    @property
    def key(self):
        return (self.path, self.symbol, self.check)


class BraceDepth:
    """Observe parsed function boundaries after each token; ignore enclosing scopes."""
    def __call__(self, tokens, reader):
        for token in tokens:
            before = reader.context.current_function
            yield token
            function = reader.context.current_function
            if token == '{':
                function.brace_depth = getattr(function, 'brace_depth', 0) + 1
                function.brace_peak = max(getattr(function, 'brace_peak', 0),
                                          function.brace_depth)
            elif token == '}':
                before.brace_depth = getattr(before, 'brace_depth', 0) - 1


def measure(path, source):
    source = source.replace('\r\n', '\n').replace('\r', '\n')
    lines = source.splitlines()
    result = [Metric(path, '', 'file_lines', len(lines), 1000)]
    analyzer = lizard.FileAnalyzer(lizard.get_extensions([BraceDepth()]))
    errors = StringIO()
    with redirect_stderr(errors):
        analysis = analyzer.analyze_source_code(path, source)
    if errors.getvalue():
        raise ValueError(f'Parser could not analyze {path}: {errors.getvalue().strip()}')
    for function in analysis.function_list:
        values = (
            ('function_lines', function.end_line - function.start_line + 1, 50),
            ('nesting', max(0, getattr(function, 'brace_peak', 1) - 1), 4),
            ('parameters', function.parameter_count, 7),
        )
        for check, value, limit in values:
            result.append(Metric(path, function.long_name, check, value, limit,
                                 function.start_line))
    return result
