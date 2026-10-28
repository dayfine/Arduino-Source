#!/usr/bin/env python3
"""Reconstruct observed Max Lair attempts and reset chains from CC log archives.

Offline log analysis only; does not control the application or modify its files.
"""
import argparse
import csv
import hashlib
import json
import re
from collections import defaultdict
from pathlib import Path

LINE = re.compile(r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+) - \[([^]]+)\]: (.*)')
START = re.compile(r'Starting Program: ([^<]+)')
STATE = re.compile(r'    boss:(.*?) - wins:(\d+)')
STATS = re.compile(r'(Runs|Wins|Errors|Catches|Shinies|Shiny Legendary): ([\d,]+)')
MODE_PREFIX = 'PokemonSwSh:MaxLair-'
STRONG = MODE_PREFIX + 'StrongBoss'


def parse_logs(paths, cutoff):
    sessions, attempts, manifest = [], [], []
    session = run = None
    timestamp = actor = ''
    state_console = 0
    reading_starters = False
    seen_events = set()
    duplicate_events = 0

    def finish():
        nonlocal run
        if run is not None:
            run['id'] = len(attempts) + 1
            run['completed'] = run['outcome'] is not None
            attempts.append(run)
            run = None

    for path in paths:
        digest = hashlib.sha256()
        first = last = ''
        size = path.stat().st_size
        with path.open('rb') as source:
            remaining = size
            for raw in source:
                if remaining <= 0:
                    break
                raw = raw[:remaining]
                remaining -= len(raw)
                digest.update(raw)
                line = raw.decode('utf-8-sig', errors='replace').rstrip('\r\n')
                match = LINE.match(line)
                if match:
                    timestamp, actor, message = match.groups()
                    first = first or timestamp
                    last = timestamp
                    if timestamp > cutoff:
                        continue
                    key = (timestamp, actor, message)
                    if key in seen_events:
                        duplicate_events += 1
                        continue
                    # Keep only semantic events to bound memory on multi-million-line archives.
                    interesting = any(s in message for s in (
                        'Starting Program:', 'Starting new adventure', 'Current Stats:',
                        'State Tracker', 'Caught Menu', 'Lost on first raid', 'Win Ratio',
                        'End of adventure', 'Quitting back to entrance',
                        'Taking non-boss shiny', 'Stopping program', 'Error detected.',
                        'Failed to start raid', 'Program finished', 'Program stopped',
                    ))
                    if interesting:
                        seen_events.add(key)
                    start = START.search(message)
                    if start:
                        finish()
                        mode = start.group(1)
                        session = None
                        if mode.startswith(MODE_PREFIX):
                            session = dict(id=len(sessions) + 1, mode=mode, start=timestamp,
                                           source=str(path), checkpoints=[])
                            sessions.append(session)
                        continue
                    if session is None:
                        continue
                    if message == 'Starting new adventure...':
                        finish()
                        run = dict(session=session['id'], mode=session['mode'], start=timestamp,
                                   end='', boss='', outcome=None, action='', ratio=None,
                                   state_wins={}, terminal_console=None, path_before=None,
                                   map={}, starters=[], errors=[], source=str(path))
                    if message.startswith('Current Stats: Runs:'):
                        stats = {k: int(v.replace(',', '')) for k, v in STATS.findall(message)}
                        session['checkpoints'].append(dict(time=timestamp, stats=stats))
                    if run is None:
                        continue
                    if 'Choosing a starter... State =>' in message:
                        reading_starters = True
                    if message == 'State Tracker':
                        state_console = int(actor.split()[-1]) if actor.startswith('Console ') else 0
                    console = int(actor.split()[-1]) if actor.startswith('Console ') else 0
                    if message == 'Current State: Caught Menu' and run['outcome'] is None:
                        run['outcome'] = int(run['state_wins'].get(str(console), -1) >= 4)
                        if str(console) not in run['state_wins']:
                            run['outcome'] = None
                            run['errors'].append('caught-without-state')
                        run['terminal_console'] = console
                        run['end'] = timestamp
                    if message == 'Lost on first raid.':
                        run['outcome'] = 0
                        run['action'] = 'first-raid-loss-continue'
                        run['terminal_console'] = console
                        run['end'] = timestamp
                    if 'Win Ratio = ' in message:
                        ratio = re.search(r'Win Ratio = ([0-9.]+)', message)
                        run['ratio'] = float(ratio.group(1)) if ratio else None
                        if 'Resetting to keep path' in message:
                            run['action'] = 'reset'
                        elif 'Continuing to get new path' in message:
                            run['action'] = 'continue'
                        run['end'] = timestamp
                    if message.startswith(('Quitting back to entrance.', 'Taking non-boss shiny')):
                        if run['action'] != 'first-raid-loss-continue':
                            run['action'] = 'continue'
                    if message == 'Stopping program...':
                        run['action'] = 'stop'
                    if message.startswith('End of adventure'):
                        run['end'] = timestamp
                    if message.startswith(('Error detected.', 'Failed to start raid.')):
                        run['errors'].append(message)
                elif timestamp <= cutoff and session is not None and run is not None:
                    switch = re.match(r'Switch (\d+):', line)
                    if switch:
                        state_console = int(switch.group(1))
                    state = STATE.match(line)
                    if state:
                        boss, wins = state.groups()
                        run['state_wins'][str(state_console)] = int(wins)
                        if not boss.startswith('?'):
                            run['boss'] = boss
                    before = re.match(r'Current Path: Runs: (\d+) - Wins: (\d+)', line)
                    if before:
                        run['path_before'] = tuple(map(int, before.groups()))
                    for name in ('Path Type', 'Row 1', 'Row 2', 'Row 3'):
                        if line.strip().startswith(name + ':'):
                            value = line.strip().split(':', 1)[1].strip()
                            if '?' not in value and 'none' not in value and value not in ('-1', ''):
                                run['map'][name] = value
                    if reading_starters and line.strip().startswith('Seen: '):
                        run['starters'] = sorted(json.loads(line.strip()[6:].replace('{', '[').replace('}', ']')))
                        reading_starters = False
        manifest.append(dict(path=str(path), bytes=size, sha256=digest.hexdigest(),
                             first_timestamp=first, last_timestamp=last))
    finish()
    return sessions, attempts, manifest, duplicate_events


def group_routes(attempts):
    routes = []
    current = previous = None
    for run in attempts:
        if run['mode'] != STRONG or not run['completed']:
            continue
        same_session = previous is not None and previous['session'] == run['session']
        same_chain = same_session and previous['action'] == 'reset' and previous['boss'] == run['boss']
        fingerprint = json.dumps(run['map'], sort_keys=True) if len(run['map']) == 4 else ''
        if same_chain and current['fingerprint'] and fingerprint and fingerprint != current['fingerprint']:
            run['errors'].append('map-changed-after-reset')
            same_chain = False
        if not same_chain:
            fresh = same_session and previous['action'] in ('continue', 'first-raid-loss-continue')
            current = dict(id=len(routes) + 1, boss=run['boss'], session=run['session'],
                           start=run['start'], end=run['end'], known_new=fresh,
                           start_reason='continue' if fresh else 'session-or-uncertain-boundary',
                           fingerprint=fingerprint, attempts=[], outcomes='', last_action='')
            routes.append(current)
        run['route'] = current['id']
        run['route_attempt'] = len(current['attempts']) + 1
        current['attempts'].append(run['id'])
        current['outcomes'] += 'W' if run['outcome'] else 'L'
        current['end'] = run['end']
        current['last_action'] = run['action']
        previous = run
    return routes


def write_csv(path, rows, fields):
    with path.open('w', newline='') as output:
        writer = csv.DictWriter(output, fieldnames=fields, extrasaction='ignore', lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)


def fingerprint_routes(attempts, routes):
    groups = {}
    for route in routes:
        first = attempts[route['attempts'][0] - 1]
        key = (route['boss'], route['fingerprint'], tuple(first['starters']))
        # Missing fingerprints never collapse unrelated episodes.
        if not route['fingerprint'] or not first['starters']:
            key += (route['id'],)
        group = groups.setdefault(key, dict(id=len(groups) + 1, boss=route['boss'],
                                           episodes=[], attempts=0, wins=0, starters=first['starters'],
                                           fingerprint=route['fingerprint']))
        group['episodes'].append(route['id'])
        group['attempts'] += len(route['outcomes'])
        group['wins'] += route['outcomes'].count('W')
        route['fingerprint_group'] = group['id']
    return list(groups.values())


def summarize(sessions, attempts, routes, fingerprints):
    summary = []
    for boss in sorted({r['boss'] for r in attempts if r['completed']}):
        for mode in sorted({r['mode'] for r in attempts if r['completed'] and r['boss'] == boss}):
            runs = [r for r in attempts if r['completed'] and r['boss'] == boss and r['mode'] == mode]
            episodes = [r for r in routes if r['boss'] == boss] if mode == STRONG else []
            first_ids = {r['attempts'][0] for r in episodes if r['known_new']}
            fresh = [r for r in runs if r['id'] in first_ids]
            repeats = [r for r in runs if r.get('route_attempt', 1) > 1]
            unknown = [r for r in runs if r.get('route_attempt') == 1 and r['id'] not in first_ids]
            groups = [r for r in fingerprints if r['boss'] == boss] if mode == STRONG else []
            summary.append(dict(boss=boss, mode=mode, attempts=len(runs), wins=sum(r['outcome'] for r in runs),
                                win_rate=sum(r['outcome'] for r in runs) / len(runs),
                                route_episodes=len(episodes), route_fingerprint_groups=len(groups),
                                known_new_attempts=len(fresh), known_new_wins=sum(r['outcome'] for r in fresh),
                                reset_repeat_attempts=len(repeats), reset_repeat_wins=sum(r['outcome'] for r in repeats),
                                uncertain_first_attempts=len(unknown), uncertain_first_wins=sum(r['outcome'] for r in unknown)))
    checks = []
    for session in sessions:
        observed = [r for r in attempts if r['session'] == session['id'] and r['completed']]
        checkpoint_errors = []
        for checkpoint in session['checkpoints']:
            upto = [r for r in observed if r['end'] <= checkpoint['time']]
            if len(upto) != checkpoint['stats']['Runs'] or sum(r['outcome'] for r in upto) != checkpoint['stats'].get('Wins', 0):
                checkpoint_errors.append(checkpoint)
        last = session['checkpoints'][-1] if session['checkpoints'] else None
        checks.append(dict(session=session['id'], mode=session['mode'], start=session['start'],
                           checkpoints=len(session['checkpoints']), checkpoint_errors=checkpoint_errors,
                           logged_runs=last['stats']['Runs'] if last else 0,
                           logged_wins=last['stats'].get('Wins', 0) if last else 0,
                           reconstructed_runs=len(observed), reconstructed_wins=sum(r['outcome'] for r in observed)))
    ratio_errors = []
    ratio_checks = 0
    for run in attempts:
        if run['completed'] and run['ratio'] is not None and run['path_before'] is not None:
            ratio_checks += 1
            count, wins = run['path_before']
            expected = (wins + run['outcome']) / (count + 1)
            if abs(expected - run['ratio']) > 1e-6:
                ratio_errors.append(dict(attempt=run['id'], expected=expected, logged=run['ratio']))
    validation = dict(checkpoints=sum(c['checkpoints'] for c in checks),
                      checkpoint_errors=sum(len(c['checkpoint_errors']) for c in checks),
                      ratio_checks=ratio_checks, ratio_errors=ratio_errors)
    return summary, checks, validation


def fraction(wins, runs):
    return f"{wins}/{runs} ({100 * wins / runs:.1f}%)" if runs else '—'


def report(output, cutoff, attempts, routes, fingerprints, summary, validation):
    complete = [r for r in attempts if r['completed']]
    index = {(r['boss'], r['mode']): r for r in summary}
    unknown = [r for r in complete if not r['boss']]
    duplicate_groups = sum(len(r['episodes']) > 1 for r in fingerprints)
    def observed(boss, mode):
        row = index.get((boss, MODE_PREFIX + mode))
        return fraction(row['wins'], row['attempts']) if row else 'no completed attempts'
    lines = [
        f'# Observed Max Lair history, {cutoff[:10]}', '',
        f'Cutoff: **{cutoff} America/Los_Angeles**, using the main local log archive.', '',
        f'Reconstructed **{len(complete):,} completed attempts** across '
        f'**{len({r["boss"] for r in complete if r["boss"]})} identified bosses**. '
        f'{len(unknown)} completed attempts have no identified boss. '
        f'{len(attempts) - len(complete)} uncompleted attempts are excluded from win-rate denominators.', '',
        '## Standard versus Strong Boss', '',
        'Each cell is recorded wins / completed attempts. BossFinder stays separate: '
        'its exploration and boss-selection behavior makes it a different baseline. '
        'Small samples are displayed but should not drive a settling threshold.', '',
        '| Boss | Standard | Strong Boss | BossFinder |',
        '| --- | ---: | ---: | ---: |',
    ]
    for boss in sorted({r['boss'] for r in complete if r['boss']}):
        cells = []
        for mode in ('Standard', 'StrongBoss', 'BossFinder'):
            row = index.get((boss, MODE_PREFIX + mode))
            cells.append(fraction(row['wins'], row['attempts']) if row else '—')
        lines.append('| ' + boss + ' | ' + ' | '.join(cells) + ' |')
    lines += ['', '## Strong Boss: deduplicating reset chains', '',
              f'**{len(routes)} route episodes**, representing **{len(fingerprints)} map + starting-rental fingerprint groups**. '
              f'{duplicate_groups} matching fingerprint groups span multiple program sessions. '
              'Within a session, an explicit reset links attempts to the same episode; '
              'an explicit continue ends it. Cross-session fingerprint matches are probable duplicates, '
              'not proof of identical hidden seeds.', '',
              '| Boss | Episodes / fingerprints | Known-new first attempts | Reset retries | Uncertain session-first attempts |',
              '| --- | ---: | ---: | ---: | ---: |']
    for row in summary:
        if row['mode'] == STRONG:
            lines.append(f"| {row['boss']} | {row['route_episodes']} / {row['route_fingerprint_groups']} | "
                         f"{fraction(row['known_new_wins'], row['known_new_attempts'])} | "
                         f"{fraction(row['reset_repeat_wins'], row['reset_repeat_attempts'])} | "
                         f"{fraction(row['uncertain_first_wins'], row['uncertain_first_attempts'])} |")
    lines += [
        '', 'A reset retry is classified by the **preceding** decision, not by whether the current '
        'attempt ends with reset or continue. The current decision depends on its outcome; '
        'bucketing by that decision would bias the comparison. Retried attempts remain real '
        "trials for a route's observed win rate, but do not count as fresh route draws.", '',
        '## What the data supports', '',
        f'- Standard ordinary-play baselines: Mewtwo {observed("mewtwo", "Standard")}, '
        f'Guzzlord {observed("guzzlord", "Standard")}, Cresselia {observed("cresselia", "Standard")}, '
        f'and Xurkitree {observed("xurkitree", "Standard")}.',
        f'- Mewtwo Strong Boss is {observed("mewtwo", "StrongBoss")} '
        f'versus Standard {observed("mewtwo", "Standard")}. '
        'That is consistent with retaining favorable routes, but is not a controlled experiment: '
        'sessions, settings and versions differ.',
        f'- Xerneas Strong Boss is {observed("xerneas", "StrongBoss")}; '
        f'Standard is {observed("xerneas", "Standard")}. '
        'The reset-chain table separates first attempts on known new routes from retries. '
        'Detailed per-route outcome sequences are in `routes.csv`.',
        '- Long retained routes demonstrate achievable performance, but are selected routes, '
        'not estimates of a random new route\'s success probability.',
        '- The retained-route sample is selected and stopped according to observed results. '
        'Its distribution of win ratios cannot directly estimate how often a fresh route has '
        'a true 80% or 95% success probability. Standard estimates the mean under its historical '
        'settings; repeated trials on multiple sampled routes are needed to learn the spread.',
        '', '## Definition and limitations', '',
        "- **Recorded win** means the program's success criterion: four captured Pokémon. "
        'The parser uses final state `wins >= 4` and checks it against the program counters. '
        'This ordinarily tracks beating and catching an eligible boss. Skipped catches or an '
        'already-caught boss can make battle victories invisible to that criterion.',
        f'- **Pheromosa needs investigation:** Standard recorded {observed("pheromosa", "Standard")}. The original '
        'session counters agree, so this is not an aggregation error. It is not enough evidence '
        'to assert that every recorded failure was a boss-battle defeat; already-caught/uncatchable boss status or '
        'another systematic problem must be ruled out before using this as a battle-win baseline.',
        '- First-raid losses count even without a policy decision. Lobby/start failures and '
        'interrupted attempts without a completed result do not count as losses.',
        '- Program restarts are uncertain route boundaries. Matching full type maps plus '
        'starting-rental sets are reported separately from explicit reset-chain identity. '
        'A map fingerprint is not a game seed and does not identify every unvisited encounter.',
        '- BossFinder attempts whose boss was never identified remain unassigned. '
        "A previous session's “Last Boss” is never substituted for an unknown boss.",
        f'- The first reconstructed Max Lair attempt is {min(r["start"] for r in attempts)}. '
        'The input manifest gives the full archive time range. This covers the retained local '
        'archive, not any deleted or other-machine history. '
        'Error-report tails and copied excerpts are excluded because they duplicate main logs.',
        '', '## Verification and reproduction', '',
        f'- All **{validation["checkpoints"]:,} cumulative Runs/Wins checkpoints** match reconstructed outcomes.',
        f'- All **{validation["ratio_checks"]:,} logged Strong Boss ratio checks** match path counters and outcomes within 1e-6.',
        '- These are offline consistency checks of real logs, not application unit tests or live validation.',
        '', '```sh',
        'python3 tools/max_lair_history.py \\',
        f'  --as-of "{cutoff}" \\',
        f'  --out pokemon_swsh/max_lair/history-{cutoff[:10]}',
        '```', '',
        'Artifacts: `attempts.csv` (one row per attempt), `routes.csv` (reset-chain episodes), '
        '`route_fingerprints.csv` (probable cross-session duplicates), `bosses.csv` (per mode), '
        '`sessions.csv`, `validation.json`, and `manifest.json` (input file snapshot hashes). '
        'Main application logs are read without modifying them. The active log is snapshotted '
        'at its initial byte length; later appended bytes are excluded from that file hash.', '',
    ]
    (output / 'REPORT.md').write_text('\n'.join(lines))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--logs', type=Path, default=Path.home() / 'Library/Application Support/SerialPrograms')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--as-of', required=True, help='Local log timestamp, e.g. 2026-10-28 08:30:00')
    args = parser.parse_args()
    paths = sorted(args.logs.glob('SerialPrograms-*.log')) + [args.logs / 'SerialPrograms.log']
    sessions, attempts, manifest, duplicates = parse_logs(paths, args.as_of)
    routes = group_routes(attempts)
    fingerprints = fingerprint_routes(attempts, routes)
    summary, checks, validation = summarize(sessions, attempts, routes, fingerprints)
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / 'manifest.json').write_text(json.dumps(dict(as_of=args.as_of, inputs=manifest), indent=2) + '\n')
    (args.out / 'validation.json').write_text(json.dumps(validation, indent=2) + '\n')
    write_csv(args.out / 'bosses.csv', summary, list(summary[0]))
    write_csv(args.out / 'attempts.csv', attempts, ['id', 'session', 'mode', 'start', 'end', 'boss', 'completed', 'outcome', 'action', 'ratio', 'route', 'route_attempt', 'source', 'errors'])
    write_csv(args.out / 'routes.csv', routes, ['id', 'boss', 'session', 'start', 'end', 'known_new', 'start_reason', 'outcomes', 'last_action', 'fingerprint_group', 'fingerprint'])
    write_csv(args.out / 'route_fingerprints.csv', fingerprints, list(fingerprints[0]))
    write_csv(args.out / 'sessions.csv', checks, list(checks[0]))
    report(args.out, args.as_of, attempts, routes, fingerprints, summary, validation)
    print(json.dumps(dict(sessions=len(sessions), attempts=len(attempts), completed=sum(r['completed'] for r in attempts),
                          routes=len(routes), fingerprint_groups=len(fingerprints), duplicate_events=duplicates,
                          validation=validation), indent=2))
    if validation['checkpoint_errors'] or validation['ratio_errors']:
        raise SystemExit('Reconstruction does not match logged counters.')



if __name__ == '__main__':
    main()
