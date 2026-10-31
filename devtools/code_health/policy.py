"""Compare changed C++ with its base revision; no stored debt registry."""
from .config import ADVISORY, HINTS
from .findings import Finding


def base_values(previous):
    old = {}
    for metric in previous:
        old[metric.key] = max(old.get(metric.key, 0), metric.value)
    return old


def evaluate(metrics, previous):
    old = base_values(previous)
    failures = []
    for metric in metrics:
        ceiling = max(metric.limit, old.get(metric.key, metric.limit))
        if metric.value > ceiling:
            failures.append(Finding(metric.path, metric.line, f'{metric.symbol or "<file>"}: '
                            f'{metric.check}={metric.value}, limit={metric.limit}, allowed {ceiling}'
                            + (f'; {HINTS[metric.check]}' if metric.check in HINTS else '')))
    return failures


def advisories(metrics, previous):
    """New or changed values between an advisory floor and the limit; legacy excess is not repeated.

    Every changed file above its floor is reported: the file changed even if its length did not."""
    old = base_values(previous)
    notes = []
    for metric in metrics:
        floor = ADVISORY.get(metric.check)
        if floor is None or (metric.check != 'file_lines' and old.get(metric.key) == metric.value):
            continue
        if floor < metric.value <= metric.limit:
            notes.append(Finding(metric.path, metric.line, f'{metric.symbol or "<file>"}: '
                                 f'{metric.check}={metric.value} (advisory above {floor}, limit {metric.limit})'))
    return notes
