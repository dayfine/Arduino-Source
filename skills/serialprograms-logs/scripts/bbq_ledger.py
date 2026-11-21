"""Reconstruct inferred BP from quest-board differences; retain an audit trail."""
import argparse
from collections import Counter
import csv
from datetime import datetime, timedelta
import json
from pathlib import Path
import re
from read_logs import records


def save_csv(path, rows, fields):
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('logs', nargs='+', type=Path)
    parser.add_argument('--rewards', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    rewards = json.loads(args.rewards.read_text())['quests']
    events, boards, runs, tasks = [], [], [], []
    run = None
    board = previous = collecting = task = None
    processing = None
    todo = []
    pending_rerolls = []
    last_time = None

    def event(ts, kind, slug, source, note=''):
        value = rewards.get(slug, {})
        bp = value.get('reward_bp') if kind == 'reward' else value.get('reroll_bp')
        events.append(dict(time=ts, run=run['start'], kind=kind, quest=slug,
                           bp=bp if isinstance(bp, int) else '', source=source, note=note))

    for path in args.logs:
        for n, ts, tag, message, lines in records(path):
            last_time = ts
            source = f'{path.name}:{n}'
            if 'Starting Program:' in message:
                run = None
                if 'BBQSoloFarmer' in message:
                    run = dict(start=ts, end='', stop='')
                    runs.append(run)
                board = previous = collecting = task = processing = None
                pending_rerolls = []
                todo = []
            if not run:
                continue
            if message == 'Reading quests.':
                collecting = dict(time=ts, run=run['start'], source=source, slots=[])
                processing = None
            elif collecting is not None and message.startswith('String Match Result:'):
                matches = re.findall(r'\(([a-z][a-z0-9-]+)\) \(log10p', message)
                collecting['slots'].append(matches[-1] if matches and matches[-1] in rewards else None)
                if len(collecting['slots']) == 4:
                    board = collecting
                    collecting = None
                    boards.append(board)
                    if previous:
                        removed = Counter(x for x in previous['slots'] if x) - Counter(x for x in board['slots'] if x)
                        for reroll in pending_rerolls:
                            slug = reroll['quest']
                            reroll['note'] += '; next board ' + ('changed target slug' if removed[slug] else 'does not prove target changed')
                            if removed[slug]:
                                removed[slug] -= 1
                        for slug, count in removed.items():
                            for _ in range(count):
                                event(ts, 'reward', slug, source,
                                      'inferred from disappearance; completion occurred since ' + previous['time'])
                    pending_rerolls = []
                    previous = board
            if message == 'Processing quests.':
                processing = 0
                todo = []
            elif processing is not None and board:
                action = message in {
                    'Quest not possible', 'Quest not in inclusion/exclusions table. Adding to list.',
                    'Run selected. Adding quest to list.', 'Skip selected. Skipping quest.',
                    'Keep Going selected. Ignoring quest.'
                }
                reroll = message.startswith('Reroll selected.')
                if message in ['Quest not in inclusion/exclusions table. Adding to list.', 'Run selected. Adding quest to list.']:
                    todo.append(board['slots'][processing] if processing < len(board['slots']) else None)
                if reroll and processing < len(board['slots']):
                    slug = board['slots'][processing]
                    event(ts, 'reroll', slug, source, 'requested; button sequence follows, no BP balance confirmation')
                    pending_rerolls.append(events[-1])
                if action or reroll:
                    processing += 1
                if message == 'No possible quests! Rerolling all quests.':
                    for slug in board['slots']:
                        if slug:
                            event(ts, 'reroll', slug, source, 'reroll-all requested; no BP balance confirmation')
                            pending_rerolls.append(events[-1])
            if message == 'Current quest exists on list. Doing quest.':
                task = dict(start=ts, end='', run=run['start'], quest=todo.pop(0) if todo else None,
                            label='', outcome='censored', seconds='', source=source,
                            verification_failures=0,entry_failures=0,photo_prompt_failures=0)
                tasks.append(task)
            elif message == 'Current quest does not exist on list. Quest completed at some point.':
                if todo:
                    todo.pop(0)
            if task:
                if message == 'Current quest exists on list. Quest did not complete.':
                    task['verification_failures'] += 1
                if message.startswith('Did not enter battle.'):
                    task['entry_failures'] += 1
                if message == 'Photo not taken.':
                    task['photo_prompt_failures'] += 1
                if not task['label'] and message.startswith(('Quest: ', 'Photo: ', 'Catch: ')):
                    task['label'] = message
                if message in ['Quest completed successfully.', 'Quest did not complete successfully.']:
                    task.update(end=ts, outcome='success' if message == 'Quest completed successfully.' else 'failure',
                                seconds=(datetime.fromisoformat(ts)-datetime.fromisoformat(task['start'])).total_seconds())
                    task = None
            if message == 'Program State: STOPPED':
                run.update(end=ts, stop=message)
                run = None
                task = None
    if run:
        run.update(end=last_time, stop='archive ended; censored')
    args.output.mkdir(parents=True, exist_ok=True)
    save_csv(args.output/'events.csv', events, ['time','run','kind','quest','bp','source','note'])
    save_csv(args.output/'tasks.csv', tasks, ['start','end','run','quest','label','outcome','seconds','source','verification_failures','entry_failures','photo_prompt_failures'])
    save_csv(args.output/'runs.csv', runs, ['start','end','stop'])
    save_csv(args.output/'boards.csv', [dict(time=b['time'],run=b['run'],source=b['source'],slots=json.dumps(b['slots'])) for b in boards], ['time','run','source','slots'])
    # Fixed, nonoverlapping windows aligned to local midnight. Keep partial and
    # inactive windows visible; do not silently annualize partial exposures.
    observed_start = datetime.fromisoformat(min(r['start'] for r in runs))
    start = observed_start.replace(hour=0,minute=0,second=0,microsecond=0)
    end = max(datetime.fromisoformat(r['end']) for r in runs if r['end'])
    windows = []
    for minutes in [10,30,60]:
        t = start
        while t < end:
            upper = t + timedelta(minutes=minutes)
            active = sum(max(0,(min(upper,datetime.fromisoformat(r['end']))-max(t,datetime.fromisoformat(r['start']))).total_seconds()) for r in runs if r['end'])
            observed = max(0, (min(upper,end)-max(t,observed_start)).total_seconds())
            if observed:
                ee = [e for e in events if t <= datetime.fromisoformat(e['time']) < upper]
                gross = sum(e['bp'] for e in ee if e['kind']=='reward' and isinstance(e['bp'],int))
                spent = sum(e['bp'] for e in ee if e['kind']=='reroll' and isinstance(e['bp'],int))
                windows.append(dict(minutes=minutes,start=t.isoformat(' '),end=upper.isoformat(' '),observed_minutes=round(observed/60,3),active_minutes=round(active/60,3),gross_bp=gross,spent_bp=spent,net_bp=gross-spent,unknown_events=sum(not isinstance(e['bp'],int) for e in ee)))
            t = upper
    save_csv(args.output/'windows.csv',windows,['minutes','start','end','observed_minutes','active_minutes','gross_bp','spent_bp','net_bp','unknown_events'])
    print(json.dumps(dict(runs=len(runs),boards=len(boards),events=len(events),gross=sum(e['bp'] for e in events if e['kind']=='reward' and isinstance(e['bp'],int)),spent=sum(e['bp'] for e in events if e['kind']=='reroll' and isinstance(e['bp'],int)),unknown_events=[e for e in events if not isinstance(e['bp'],int)]),indent=2))


if __name__ == '__main__':
    main()
