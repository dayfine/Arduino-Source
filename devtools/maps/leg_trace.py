"""Leg traces: a console run's route read back from its log, and graded
against the simulation's route for the same checkpoints.

A walker writes one record per leg (SerialPrograms Pokemon_LegTrace.h):

    [Leg] {"checkpoint":"00.04","event":"to_the_signs_row","leg":1,"dir":"north",
           "tiles":1,"end_kind":"tile","planned_end":[...],"moved":1,"chunks":1,
           "ms":800,"end":[["Route 201",57,4]],"known":"counted","interrupts":[]}

A trace is {"legs": [record, ...]}; the simulation's are checked in as
golden files (Simulation/leg_trace_<row>.golden.json). Logs written before
the records existed are read from their route lines instead (replay_text()):
plan legs, their "ended at" lines, the story's tap and dialogue runs.

The diff aligns the two traces by checkpoint, event and leg, grades each
leg, and fails only on LARGE:

    OK      the same plan, and the end the same, or for an open leg within
            OPEN_BAND_TILES of the simulation's
    drift   outside that band, but a later leg of the event that re-localizes
            (a wall, a count, a fade) ends where the simulation's does; or
            a leg of a replan after a leg stopped short, until the route
            re-syncs
    LARGE   outside the band and never re-synced, a different map, legs or
            events one side lacks, a recovery loop, or a checkpoint whose
            walking time is over TIME_RATIO_LARGE times the simulation's
    masked  a leg with an interruption (wild battle, trainer, scene,
            someone in the way), and
            the console's legs after it until one ends where a simulation
            leg does

    python3 -m devtools.maps.leg_trace replay run.log -o console.json
    python3 -m devtools.maps.leg_trace diff run.log Simulation/leg_trace_00.04.golden.json
"""
import argparse
import json
import re
import sys
from dataclasses import dataclass, field

# Thresholds, in one place.
# An open leg (timed or tapped into the open) ending this many tiles or
# fewer from the simulation's end is OK: a later wall clip absorbs it.
OPEN_BAND_TILES = 2
# A checkpoint's walking time over this times the simulation's is LARGE...
TIME_RATIO_LARGE = 1.5
# ...when it is also at least this much longer (short checkpoints jitter).
TIME_MIN_EXTRA_MS = 3000
# An event with this many more legs than the simulation's is a recovery loop.
LOOP_EXTRA_LEGS = 6
# How a leg's end is known when it re-localizes the walker.
RELOCALIZING = {'wall', 'counted', 'fade', 'seen', 'dialogue'}

OK = 'OK'
DRIFT = 'drift'
LARGE = 'LARGE'
MASKED = 'masked'

LEG_TAG = '[Leg] '
DIRECTION = r'(north|south|east|west)'
TILE = re.compile(r'([^,()]+?) \((-?\d+), (-?\d+)\)')
CHECKPOINT = re.compile(r'\[Checkpoint\] (\S+): start')
EVENT = re.compile(r'\[Event\] (\S+)/\d+ (\S+): start')
PLAN_LEG = re.compile(r'(?:\[Route\] )?Leg \d+/\d+: ' + DIRECTION + r' (\d+) \((to wall|open|warp)\) to (.*?)\s*$')
LEG_END = re.compile(r'\[Route\] .*: ended at (.*) \((walked|assumed)[^)]*\)\.\s*$')
REPLAN = re.compile(r'\[Route\] .*, at (.*); planning again\.\s*$')
TAPS = re.compile(r'\[(\S+)\] (\S+): taps ' + DIRECTION + r' moved (\d+) of (\d+)(?: tiles\.|: )')
TO_DIALOGUE = re.compile(r'\[(\S+)\] (\S+): ran ' + DIRECTION +
                         r' to a dialogue: (\d+) moved chunks, (\d+) tiles at most, ([^,]+), (\d+) wild battle')
WILD = re.compile(r'\[Walk\] .*wild battle[^;]*; running\.')
TRAINER = re.compile(r'\[Route\] .* stopped us about ')
BLOCKED = re.compile(r'; waiting for the way to clear\.|: someone is in the way; talking to them\.')
WALKED = re.compile(r'\[Walk\] .*: \w+ after (\d+)ms, (\d+) moved chunks, (\d+) wild battle')


def tiles_of(text):
    return [[m.group(1).strip(), int(m.group(2)), int(m.group(3))] for m in TILE.finditer(text)]


def replay(lines, attempt=None):
    """The trace of a log: its [Leg] records, or if it has none, what its
    route lines say (replay_text()). A log can hold several runs of a
    checkpoint: each record gets its run's "attempt" (1, 2, ... a
    checkpoint), and only `attempt` is kept (-1: each checkpoint's last;
    None: all)."""
    legs = []
    attempts = {}
    for line in lines:
        m = CHECKPOINT.search(line)
        if m:
            attempts[m.group(1)] = attempts.get(m.group(1), 0) + 1
        at = line.find(LEG_TAG)
        if at >= 0:
            record = json.loads(line[at + len(LEG_TAG):])
            record.setdefault('attempt', attempts.get(record['checkpoint'], 1))
            legs.append(record)
    trace = {'legs': legs} if legs else replay_text(lines)
    return pick_attempt(trace, attempt)


def pick_attempt(trace, attempt):
    """`trace` with only run `attempt` of each checkpoint (-1: the last)."""
    if attempt is None:
        return trace
    last = {}
    for leg in trace['legs']:
        last[leg['checkpoint']] = max(last.get(leg['checkpoint'], 0), leg.get('attempt', 1))
    def wanted(checkpoint, run):
        return run == (last.get(checkpoint, 0) if attempt == -1 else attempt)
    out = {'legs': [leg for leg in trace['legs'] if wanted(leg['checkpoint'], leg.get('attempt', 1))]}
    if 'events' in trace:
        out['events'] = [e for e in trace['events'] if wanted(e[0], e[2])]
    return out


def replay_text(lines):
    """A trace from a log's free-text route lines: no held time, and tap
    legs without their end tiles (the log does not name them)."""
    legs = []
    state = {'checkpoint': '', 'event': '', 'count': 0, 'open': None}
    attempts = {}
    events = []

    def start(record):
        close(None, 'unknown')
        state['count'] += 1
        record.update({'checkpoint': state['checkpoint'], 'event': state['event'], 'leg': state['count'],
                       'attempt': attempts.get(state['checkpoint'], 1)})
        record.setdefault('chunks', 0)
        record.setdefault('ms', 0)
        record.setdefault('interrupts', [])
        record.setdefault('planned_end', [])
        state['open'] = record

    def close(end, known, note=''):
        record = state['open']
        if record is None:
            return
        record['end'] = end or []
        record['known'] = known
        if note:
            record['note'] = note
        legs.append(record)
        state['open'] = None

    for line in lines:
        m = CHECKPOINT.search(line)
        if m:
            close(None, 'unknown')
            attempts[m.group(1)] = attempts.get(m.group(1), 0) + 1
            continue
        m = EVENT.search(line)
        if m:
            close(None, 'unknown')
            state.update({'checkpoint': m.group(1), 'event': m.group(2), 'count': 0})
            events.append([m.group(1), m.group(2), attempts.get(m.group(1), 1)])
            continue
        m = PLAN_LEG.search(line)
        if m:
            kind = {'to wall': 'wall', 'open': 'open', 'warp': 'tile'}[m.group(3)]
            start({'dir': m.group(1), 'tiles': int(m.group(2)), 'end_kind': kind, 'planned_end': tiles_of(m.group(4))})
            continue
        m = LEG_END.search(line)
        if m and state['open'] is not None:
            assumed = m.group(2) == 'assumed'
            if assumed and 'wild_battle' not in state['open']['interrupts']:
                state['open']['interrupts'].append('wild_battle')
            wall = state['open']['end_kind'] == 'wall' and not assumed
            close(tiles_of(m.group(1)), 'wall' if wall else 'dead_reckoned')
            continue
        m = REPLAN.search(line)
        if m and state['open'] is not None:
            close(tiles_of(m.group(1)), 'seen', 'replan')
            continue
        m = TAPS.search(line)
        if m:
            start({'dir': m.group(3), 'tiles': int(m.group(5)), 'end_kind': 'tile', 'moved': int(m.group(4)),
                   'chunks': int(m.group(4))})
            close(None, 'counted')
            continue
        m = TO_DIALOGUE.search(line)
        if m:
            start({'dir': m.group(3), 'tiles': int(m.group(5)), 'end_kind': 'open', 'chunks': int(m.group(4))})
            if int(m.group(7)) > 0:
                state['open']['interrupts'].append('wild_battle')
            close(None, 'dialogue' if 'dialog' in m.group(6) else 'unknown')
            continue
        if state['open'] is not None and WILD.search(line):
            state['open']['interrupts'].append('wild_battle')
        elif state['open'] is not None and TRAINER.search(line):
            state['open']['interrupts'].append('trainer')
        elif state['open'] is not None and BLOCKED.search(line):
            state['open']['interrupts'].append('blocked')
        elif state['open'] is not None and WALKED.search(line):
            m = WALKED.search(line)
            record = state['open']
            record['ms'] += int(m.group(1))
            record['chunks'] += int(m.group(2))
            if record['end_kind'] == 'open':
                record['moved'] = record.get('moved', 0) + int(m.group(2))
            if int(m.group(3)) > 0 and 'wild_battle' not in record['interrupts']:
                record['interrupts'].append('wild_battle')
    close(None, 'unknown')
    # The events the log ran: one with no leg lines walked legs the text
    # does not describe (a LEGS step to the walls).
    return {'legs': legs, 'events': events}


@dataclass
class LegGrade:
    checkpoint: str
    event: str
    console: int        # the console's leg number, 0 for none
    sim: int            # the simulation's leg number, 0 for none
    grade: str
    why: str = ''


@dataclass
class Report:
    grades: list = field(default_factory=list)
    # Checkpoint-level findings: (checkpoint, grade, why).
    findings: list = field(default_factory=list)

    def large(self):
        return any(g.grade == LARGE for g in self.grades) or any(f[1] == LARGE for f in self.findings)

    def first_divergence(self):
        for g in self.grades:
            if g.grade in (DRIFT, LARGE):
                return g
        return None

    def summary(self):
        out = []
        checkpoints = []
        for g in self.grades:
            if g.checkpoint not in checkpoints:
                checkpoints.append(g.checkpoint)
        for checkpoint in checkpoints:
            mine = [g for g in self.grades if g.checkpoint == checkpoint]
            counts = {k: sum(1 for g in mine if g.grade == k) for k in (OK, DRIFT, LARGE, MASKED)}
            extra = [f for f in self.findings if f[0] == checkpoint]
            large = counts[LARGE] + sum(1 for f in extra if f[1] == LARGE)
            line = '{}: {} legs: {} OK, {} drift, {} LARGE, {} masked'.format(
                checkpoint, len(mine), counts[OK], counts[DRIFT], large, counts[MASKED])
            first = next((g for g in mine if g.grade in (DRIFT, LARGE)), None)
            if first:
                line += '; first divergence: {} leg {} ({}): {}'.format(first.event, first.console or '-', first.grade, first.why)
            for f in extra:
                line += '; {}: {}'.format(f[1], f[2])
            out.append(line)
        return out


def _key(tile):
    return (tile[0], int(tile[1]), int(tile[2]))


def _maps(leg):
    return {t[0] for t in leg.get('end', [])}


def _distance(a, b):
    """The fewest tiles between a tile of `a` and one of `b` on the same map;
    None if they share no map."""
    best = None
    for x in a:
        for y in b:
            if x[0] == y[0]:
                d = abs(int(x[1]) - int(y[1])) + abs(int(x[2]) - int(y[2]))
                best = d if best is None else min(best, d)
    return best


def _same_plan(c, s):
    return c['dir'] == s['dir'] and c['tiles'] == s['tiles'] and c['end_kind'] == s['end_kind']


def _same_end(c, s):
    return {_key(t) for t in c.get('end', [])} == {_key(t) for t in s.get('end', [])}


def _relocalizes(c, s):
    """Whether console leg `c` ends, re-localized, where sim leg `s` does."""
    return c.get('known') in RELOCALIZING and c.get('end') and _same_end(c, s)


def grade_leg(c, s):
    """(grade, why) of console leg `c` against sim leg `s`, of the same plan;
    DRIFT here means "outside the band": the caller decides whether a later
    leg re-syncs it."""
    if c.get('end') and s.get('end') and _maps(c) != _maps(s):
        return LARGE, 'ended on {} where the simulation ended on {}'.format(sorted(_maps(c)), sorted(_maps(s)))
    if c['end_kind'] == 'tile' and 'moved' in c and 'moved' in s and c['moved'] != s['moved']:
        return DRIFT, 'moved {} of {} tiles, the simulation {}'.format(c['moved'], c['tiles'], s['moved'])
    # An open leg's end is dead-reckoned from its plan: the taps that moved
    # are what it measured.
    if c['end_kind'] == 'open' and 'moved' in c and 'moved' in s and abs(c['moved'] - s['moved']) > OPEN_BAND_TILES:
        return DRIFT, 'open leg moved {} of {} tiles, the simulation {}'.format(c['moved'], c['tiles'], s['moved'])
    if not c.get('end') or not s.get('end') or _same_end(c, s):
        return OK, ''
    distance = _distance(c['end'], s['end'])
    if c['end_kind'] == 'open' and distance is not None and distance <= OPEN_BAND_TILES:
        return OK, 'open leg {} tile(s) off'.format(distance)
    return DRIFT, 'ended {} tile(s) from the simulation: {} vs {}'.format(distance, c['end'], s['end'])


def _resync(console, i, sim, j):
    """After an interruption: the first console leg from `i` that ends where
    a sim leg from `j` does, as (console index, sim index), or None. The
    legs go on from the ones after both."""
    for k in range(i, len(console)):
        for m in range(j, len(sim)):
            if console[k].get('end') and _same_end(console[k], sim[m]) and not console[k].get('interrupts'):
                return k, m
    return None


def _grade_event(checkpoint, event, console, sim, grades):
    pending = []    # grades outside the band, waiting for a re-sync
    i = j = 0

    def add(c, s, grade, why=''):
        g = LegGrade(checkpoint, event, c['leg'] if c else 0, s['leg'] if s else 0, grade, why)
        grades.append(g)
        return g

    while i < len(console) and j < len(sim):
        c, s = console[i], sim[j]
        if c.get('interrupts') or s.get('interrupts'):
            add(c, s, MASKED, 'interrupted: ' + ', '.join(c.get('interrupts', []) + s.get('interrupts', [])))
            found = _resync(console, i + 1, sim, j)
            if found is None:
                for rest in console[i + 1:]:
                    add(rest, None, MASKED, 'after an interruption, never re-synced')
                return
            k, m = found
            for rest in console[i + 1:k]:
                add(rest, None, MASKED, 'after an interruption')
            add(console[k], sim[m], MASKED, 're-synced: ends where the simulation leg {} does'.format(sim[m]['leg']))
            i, j = k + 1, m + 1
            continue
        if not _same_plan(c, s):
            add(c, s, LARGE, 'planned {} {} ({}), the simulation {} {} ({})'.format(
                c['dir'], c['tiles'], c['end_kind'], s['dir'], s['tiles'], s['end_kind']))
            i += 1
            j += 1
            continue
        if c.get('note') == 'replan':
            # Stopped short and planned again: the legs until the route
            # re-syncs are drift, or LARGE if it never does.
            grade, why = grade_leg(c, s)
            add(c, s, grade if grade != DRIFT else LARGE, ('stopped short and planned again; ' + why).rstrip('; '))
            found = _resync(console, i + 1, sim, j + 1)
            if found is None:
                for rest in console[i + 1:]:
                    add(rest, None, LARGE, 'after a replan, never re-synced')
                return
            k, m = found
            for rest in console[i + 1:k]:
                add(rest, None, DRIFT, 'a replanned leg; re-synced at leg {}'.format(console[k]['leg']))
            add(console[k], sim[m], OK, 're-synced: ends where the simulation leg {} does'.format(sim[m]['leg']))
            i, j = k + 1, m + 1
            continue
        grade, why = grade_leg(c, s)
        if grade == OK and _relocalizes(c, s):
            for g in pending:
                g.grade, g.why = DRIFT, g.why + '; re-synced at leg {}'.format(c['leg'])
            pending = []
        g = add(c, s, grade, why)
        if grade == DRIFT:
            pending.append(g)
        i += 1
        j += 1
    for g in pending:
        g.grade, g.why = LARGE, g.why + '; never re-synced'
    for c in console[i:]:
        add(c, None, LARGE, 'a leg the simulation does not walk')
    for s in sim[j:]:
        add(None, s, LARGE, 'a simulation leg the console did not walk')


def _by_event(legs):
    out = {}
    for leg in legs:
        out.setdefault((leg['checkpoint'], leg['event']), []).append(leg)
    return out


def diff(console, sim, checkpoints=None):
    """Grades `console`'s legs (a trace) against `sim`'s, for `checkpoints`
    (default: those the simulation trace has)."""
    report = Report()
    sim_legs = [leg for leg in sim['legs'] if checkpoints is None or leg['checkpoint'] in checkpoints]
    wanted = {leg['checkpoint'] for leg in sim_legs}
    console_legs = [leg for leg in console['legs'] if leg['checkpoint'] in wanted]
    c_events = _by_event(console_legs)
    s_events = _by_event(sim_legs)
    unrecorded = {(e[0], e[1]) for e in console.get('events', [])} - set(c_events)
    for key, s in s_events.items():
        c = c_events.get(key, [])
        if key in unrecorded:
            for leg in s:
                report.grades.append(LegGrade(key[0], key[1], 0, leg['leg'], MASKED, 'the log ran the event but has no lines for its legs'))
            continue
        if len(c) > len(s) + LOOP_EXTRA_LEGS:
            report.findings.append((key[0], LARGE, '{} walked {} legs, the simulation {}: a recovery loop'.format(
                key[1], len(c), len(s))))
        _grade_event(key[0], key[1], c, s, report.grades)
    for key, c in c_events.items():
        if key not in s_events:
            report.findings.append((key[0], LARGE, 'event {} walks {} legs the simulation does not'.format(key[1], len(c))))
    for checkpoint in sorted(wanted):
        c_ms = sum(leg.get('ms', 0) for leg in console_legs if leg['checkpoint'] == checkpoint and not leg.get('interrupts'))
        s_ms = sum(leg.get('ms', 0) for leg in sim_legs if leg['checkpoint'] == checkpoint)
        if s_ms > 0 and c_ms > TIME_RATIO_LARGE * s_ms and c_ms - s_ms >= TIME_MIN_EXTRA_MS:
            report.findings.append((checkpoint, LARGE, 'walked {} ms, the simulation {} ms'.format(c_ms, s_ms)))
    return report


def _load(path, attempt=None):
    with open(path, encoding='utf-8', errors='replace') as f:
        text = f.read()
    if text.lstrip().startswith('{'):
        return pick_attempt(json.loads(text), attempt)
    return replay(text.splitlines(), attempt)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = parser.add_subparsers(dest='command', required=True)
    r = sub.add_parser('replay', help='a log as a trace')
    r.add_argument('log')
    r.add_argument('-o', '--out')
    r.add_argument('--attempt', type=int, help='only this run of each checkpoint (-1: the last)')
    d = sub.add_parser('diff', help='grade a console log or trace against a simulation trace')
    d.add_argument('console')
    d.add_argument('sim')
    d.add_argument('--checkpoint', action='append')
    d.add_argument('--attempt', type=int, default=-1, help='the console run of each checkpoint to grade (default -1: the last)')
    d.add_argument('-v', '--verbose', action='store_true', help='every leg, not only those off')
    args = parser.parse_args(argv)
    if args.command == 'replay':
        text = json.dumps(_load(args.log, args.attempt), indent=1)
        if args.out:
            with open(args.out, 'w', encoding='utf-8') as f:
                f.write(text + '\n')
        else:
            print(text)
        return 0
    report = diff(_load(args.console, args.attempt), _load(args.sim), args.checkpoint)
    for g in report.grades:
        if args.verbose or g.grade != OK:
            print('{} {} console leg {} / sim leg {}: {} {}'.format(
                g.checkpoint, g.event, g.console or '-', g.sim or '-', g.grade, g.why).rstrip())
    for line in report.summary():
        print(line)
    return 1 if report.large() else 0


if __name__ == '__main__':
    sys.exit(main())
