#!/usr/bin/env python3
"""Prepare explicit model assumptions and empirical timing samples for native simulation."""
import argparse
import csv
import json
import re
import statistics
import subprocess
from datetime import datetime
from pathlib import Path


def prepare(history, output, logs):
    rows = list(csv.DictReader((history / 'attempts.csv').open()))
    rows = [r for r in rows if r['completed'] == 'True']
    paths = sorted(logs.glob('SerialPrograms-*.log')) + [logs / 'SerialPrograms.log']
    events = subprocess.check_output([
        'rg', '--no-filename', '-e', 'Win Ratio = ', '-e', r'\[Program\]: End of adventure',
        *map(str, paths),
    ], text=True)
    pending = None
    end_times = {'reset': [], 'continue': []}
    for line in events.splitlines():
        match = re.match(r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+)', line)
        if not match or match[1] > '2026-10-28 08:30:00':
            continue
        time = datetime.fromisoformat(match[1])
        if 'Win Ratio = ' in line:
            action = 'reset' if 'Resetting to keep path' in line else 'continue'
            pending = time, action
        elif pending:
            seconds = (time - pending[0]).total_seconds()
            if 0 <= seconds < 120:
                end_times[pending[1]].append(seconds)
            pending = None
    reset_extra = statistics.median(end_times['reset']) - statistics.median(end_times['continue'])
    timings = {}
    for boss in ('mewtwo', 'cresselia', 'xurkitree'):
        timings[boss] = {}
        for outcome in ('1', '0'):
            sample = [r for r in rows if r['boss'] == boss and r['mode'].endswith('-Standard') and r['outcome'] == outcome]
            values = [(datetime.fromisoformat(r['end']) - datetime.fromisoformat(r['start'])).total_seconds() for r in sample]
            timings[boss]['win' if outcome == '1' else 'loss'] = values
    measured = {
        'cutoff': '2026-10-28 08:30:00',
        'reset_decision_to_end_median_seconds': statistics.median(end_times['reset']),
        'continue_decision_to_end_median_seconds': statistics.median(end_times['continue']),
        'reset_samples': len(end_times['reset']), 'continue_samples': len(end_times['continue']),
        'incremental_reset_seconds': reset_extra,
        'timings': {b: {k: {'count': len(v), 'median_seconds': statistics.median(v),
                           'mean_seconds': statistics.mean(v)} for k, v in t.items()} for b, t in timings.items()},
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / 'timing_assumptions.json').write_text(json.dumps(measured, indent=2) + '\n')
    models = []
    for boss in ('mewtwo', 'cresselia', 'xurkitree'):
        sample = [r for r in rows if r['boss'] == boss and r['mode'].endswith('-Standard')]
        models.append((boss, sum(int(r['outcome']) for r in sample) / len(sample), boss, 'observed Standard mean'))
    models += [('xerneas-low', .40, 'mewtwo', 'hypothetical mean; Mewtwo timing proxy'),
               ('xerneas-mid', .55, 'mewtwo', 'hypothetical mean; Mewtwo timing proxy'),
               ('easy', .90, 'mewtwo', 'hypothetical mean; Mewtwo timing proxy')]
    with (output / 'models.tsv').open('w') as target:
        target.write('name\tmean\tlow\thigh\thigh_probability\treset_extra_seconds\twin_seconds\tloss_seconds\tassumption\n')
        for name, mean, timing_boss, assumption in models:
            for shape, high, q in [('homogeneous', mean, 1.0), ('rare95', .95, .05),
                                   ('common95', .95, .35), ('capped', min(.93, max(.75, mean + .03)), .35)]:
                low = mean if q == 1 else (mean - q * high) / (1 - q)
                assert 0 <= low <= 1 and abs(low * (1 - q) + high * q - mean) < 1e-12
                win = ','.join(f'{x:.6f}' for x in timings[timing_boss]['win'])
                loss = ','.join(f'{x:.6f}' for x in timings[timing_boss]['loss'])
                target.write(f'{name}-{shape}\t{mean:.12f}\t{low:.12f}\t{high:.12f}\t{q:.12f}\t{reset_extra:.6f}\t{win}\t{loss}\t{assumption}\n')
    print(json.dumps(measured, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--history', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--logs', type=Path, default=Path.home() / 'Library/Application Support/SerialPrograms')
    args = parser.parse_args()
    prepare(args.history, args.out, args.logs)
