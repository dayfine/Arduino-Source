"""Compare changed C++ with its base revision; no stored debt registry."""


def evaluate(metrics, previous):
    old = {}
    for metric in previous:
        old[metric.key] = max(old.get(metric.key, 0), metric.value)
    failures = []
    for metric in metrics:
        ceiling = max(metric.limit, old.get(metric.key, metric.limit))
        if metric.value > ceiling:
            failures.append(f'{metric.path}:{metric.line}: {metric.symbol or "<file>"}: '
                            f'{metric.check}={metric.value}, limit={metric.limit}, allowed {ceiling}')
    return failures
