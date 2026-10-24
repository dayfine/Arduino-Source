"""Dated debt ceilings and a per-symbol regression ratchet."""
import datetime


SCHEMA = 1
CHECKS = {'file_lines': 1000, 'function_lines': 50, 'nesting': 4, 'parameters': 7}


def make_baseline(metrics, review_at):
    datetime.date.fromisoformat(review_at)
    entries = {}
    for metric in metrics:
        if metric.value <= metric.limit:
            continue
        entry = dict(path=metric.path, symbol=metric.symbol, check=metric.check,
                     value=metric.value, review_at=review_at,
                     reason='Existing debt at initial code-health rollout')
        old = entries.get(metric.key)
        if old is None or old['value'] < metric.value:
            entries[metric.key] = entry
    return {'schema': SCHEMA, 'violations': [entries[k] for k in sorted(entries)]}


def read_entries(baseline):
    if baseline.get('schema') != SCHEMA:
        raise ValueError('Unsupported baseline schema')
    entries = {}
    for entry in baseline['violations']:
        try:
            key = (entry['path'], entry['symbol'], entry['check'])
            datetime.date.fromisoformat(entry['review_at'])
            if not entry['reason'] or type(entry['value']) is not int:
                raise ValueError('Baseline entries require an integer value and reason')
            if entry['value'] <= CHECKS[entry['check']] or key in entries:
                raise ValueError('Invalid or duplicate baseline violation: ' + str(key))
        except (KeyError, TypeError) as error:
            raise ValueError('Malformed baseline entry: ' + str(entry)) from error
        entries[key] = entry
    return entries


def evaluate(metrics, baseline, today, previous=None):
    entries = read_entries(baseline)
    old = {}
    for metric in previous or []:
        old[metric.key] = max(old.get(metric.key, 0), metric.value)
    failures = []
    for metric in metrics:
        if metric.value <= metric.limit:
            continue
        entry = entries.get(metric.key)
        ceiling = entry['value'] if entry else metric.limit
        if previous is not None:
            ceiling = min(ceiling, max(metric.limit, old.get(metric.key, metric.limit)))
        expired = entry and datetime.date.fromisoformat(entry['review_at']) <= today
        if metric.value > ceiling or expired:
            reason = 'expired review_at=' + entry['review_at'] if expired else f'allowed {ceiling}'
            failures.append(f'{metric.path}:{metric.line}: {metric.symbol or "<file>"}: '
                            f'{metric.check}={metric.value}, limit={metric.limit}, {reason}')
    return failures
