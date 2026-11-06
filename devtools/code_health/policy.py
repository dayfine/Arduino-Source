"""Compare changed C++ with its base revision; no stored debt registry."""
from .allow import ceiling as bypass_ceiling
from .config import ADVISORY, HINTS
from .findings import Finding


def base_values(previous, current=()):
    old = {}
    current_keys = {metric.key for metric in current}
    for metric in previous:
        old[metric.key] = max(old.get(metric.key, 0), metric.value)
        if metric.source_identity is not None and metric.key not in current_keys:
            key = move_key(metric)
            old[key] = max(old.get(key, 0), metric.value)
    return old


def move_key(metric):
    """An exact same-file function move; never borrow debt from a retained copy."""
    return ('namespace_move', metric.path, metric.source_identity, metric.check)


def base_value(metric, old, default):
    return old.get(metric.key, old.get(move_key(metric), default))


def evaluate(metrics, previous, allowed=None):
    """Failures for values above both the limit and the base; `allowed` maps metrics to tags."""
    metrics = list(metrics)
    old = base_values(previous, metrics)
    allowed = allowed or {}
    failures = []
    for metric in metrics:
        ceiling = max(metric.limit, base_value(metric, old, metric.limit))
        if metric.value <= ceiling:
            continue
        message = (f'{metric.symbol or "<file>"}: {metric.check}={metric.value}, '
                   f'limit={metric.limit}, allowed {ceiling}')
        if metric in allowed:
            hard = bypass_ceiling(metric)
            if hard is None or metric.value <= hard:
                continue
            message += f'; a code-health allow tag permits at most {hard}'
        elif metric.check in HINTS:
            message += f'; {HINTS[metric.check]}'
        failures.append(Finding(metric.path, metric.line, message))
    return failures


def bypasses(metrics, allowed):
    """(notices for every value above its limit under a tag, advisories for unneeded tags)."""
    notices, unneeded = [], []
    for metric in metrics:
        allowance = allowed.get(metric)
        if allowance is None:
            continue
        name = metric.symbol or '<file>'
        if metric.value > metric.limit:
            notices.append(Finding(metric.path, metric.line, f'{name}: {metric.check}={metric.value} '
                                   f'over limit {metric.limit}, bypassed by the tag at line '
                                   f'{allowance.line}: "{allowance.justification}"'))
        else:
            unneeded.append(Finding(metric.path, allowance.line, f'{name}: {metric.check}='
                                    f'{metric.value} is within limit {metric.limit}; remove '
                                    'its code-health allow tag'))
    return notices, unneeded


def advisories(metrics, previous):
    """New or changed values between an advisory floor and the limit; legacy excess is not repeated.

    Every changed file above its floor is reported: the file changed even if its length did not."""
    metrics = list(metrics)
    old = base_values(previous, metrics)
    notes = []
    for metric in metrics:
        floor = ADVISORY.get(metric.check)
        if floor is None or (metric.check != 'file_lines' and base_value(metric, old, None) == metric.value):
            continue
        if floor < metric.value <= metric.limit:
            notes.append(Finding(metric.path, metric.line, f'{metric.symbol or "<file>"}: '
                                 f'{metric.check}={metric.value} (advisory above {floor}, limit {metric.limit})'))
    return notes
