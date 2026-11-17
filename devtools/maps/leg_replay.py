"""A console failure as a fault file for the simulation (Pokemon_LegReplay.h).

    python3 -m devtools.maps.leg_replay LOG --failed-at 'HH:MM:SS' [--date YYYY-MM-DD]
        [--fault JSON]... [--pin 'step: Map x,y'] [--frames DIR] [--build SHA] -o FILE

Reads the failure the program logged at that time ("[Event] 01.02/11
into_oreburgh: failed: ..."), the [Leg] records and route lines of the run up
to it, and the failure frame nearest in time, and writes the fault file: the
checkpoint, where it failed, the frame, and the log lines the faults were
worked out from. The faults are the ones given with --fault (the file's own
JSON, see Pokemon_LegReplay.h); the tool derives none the log cannot show by
itself, so it stops (exit 2) and prints the facts it found when none are
given, asking for one pinned position (--pin) taken by hand from the frame.
"""

import argparse
import glob
import json
import os
import re
import sys
from datetime import datetime

STAMP = re.compile(r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)\.\d+ - ')
FAILED = re.compile(r'\[Event\] (\d\d\.\d\d[a-z]?)/(\d+) (\w+): failed: (.*)$')
FACTS = ('[Leg] ', 'someone the map lacks', 'a trainer the map lacks', 'find us again',
         'no fade and no', 'blocked after', 'talked to us')


def failure_at(lines, stamp):
    """(index, checkpoint, step, error) of the failure logged at `stamp`."""
    for i, line in enumerate(lines):
        m = FAILED.search(line)
        if m and line.startswith(stamp):
            return i, m.group(1), m.group(3), m.group(4).strip()
    return None


def run_facts(lines, end, checkpoint):
    """The facts of the checkpoint's run that ends at line `end`."""
    start = end
    while start > 0 and f'[Checkpoint] {checkpoint}: start' not in lines[start]:
        start -= 1
    out = []
    for line in lines[start:end + 1]:
        if any(tag in line for tag in FACTS) and 'Watcher' not in line:
            out.append(STAMP.sub('', line).replace('[Console 0]: ', '').strip())
    return out


def nearest_frame(frames, stamp):
    """The failure frame closest in time to `stamp` (file names: YYYYMMDD-HHMMSS...)."""
    want = datetime.strptime(stamp, '%Y-%m-%d %H:%M:%S')
    best = None
    for path in glob.glob(os.path.join(frames, '*.*')):
        m = re.match(r'(\d{8}-\d{6})', os.path.basename(path))
        if not m:
            continue
        gap = abs((datetime.strptime(m.group(1), '%Y%m%d-%H%M%S') - want).total_seconds())
        if best is None or gap < best[0]:
            best = (gap, os.path.basename(path))
    return best[1] if best and best[0] <= 120 else ''


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('log')
    p.add_argument('--failed-at', required=True)
    p.add_argument('--date', default=datetime.now().strftime('%Y-%m-%d'))
    p.add_argument('--fault', action='append', default=[])
    p.add_argument('--pin', help="'step: Map x,y' read from the frame")
    p.add_argument('--frames')
    p.add_argument('--build', default='')
    p.add_argument('-o', '--out')
    a = p.parse_args(argv)
    stamp = f'{a.date} {a.failed_at}'
    with open(a.log, errors='replace') as f:
        lines = [l.rstrip('\n') for l in f]
    found = failure_at(lines, stamp)
    if not found:
        print(f'no failure logged at {stamp}', file=sys.stderr)
        return 1
    index, checkpoint, step, error = found
    facts = run_facts(lines, index, checkpoint)
    if not a.fault and not a.pin:
        print(f'{checkpoint} failed at {step}: {error}\nthe log shows, not enough to place the player:', file=sys.stderr)
        for fact in facts[-30:]:
            print('  ' + fact[:200], file=sys.stderr)
        print("pin one position from the frame (--pin 'step: Map x,y') or give the faults (--fault).", file=sys.stderr)
        return 2
    out = {
        'about': f'lab-B {stamp}, {checkpoint} {step}' + (f', build {a.build}' if a.build else ''),
        'checkpoint': checkpoint,
        'faults': [json.loads(x) for x in a.fault],
        'failed': {'step': step, 'error': error},
        'frame': nearest_frame(a.frames, stamp) if a.frames else '',
        'evidence': facts[-12:],
    }
    if a.pin:
        out['pin'] = a.pin
    text = json.dumps(out, indent=2) + '\n'
    if a.out:
        with open(a.out, 'w') as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
