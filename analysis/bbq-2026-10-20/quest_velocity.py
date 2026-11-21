"""Per-requested-quest velocity before the stall, including failed retry batches."""
import csv
import json
from collections import defaultdict
from pathlib import Path

root = Path(__file__).resolve().parent
tasks = list(csv.DictReader((root/'tasks.csv').open()))
events = list(csv.DictReader((root/'events.csv').open()))
catalog = json.loads((root/'quest-rewards.json').read_text())['quests']
cutoff = '2026-10-20 05:05:20'
selected_events = [e for e in events if e['time'] < cutoff and e['kind']=='reward']
reds = [e for e in selected_events if catalog[e['quest']]['tier']=='red']
bonus_credit = sum(int(e['bp']) for e in reds)/len(reds)/10
rows = []
for scope, lower in [('all-pre-stall','2026-10-19 23:39'), ('continuous-pre-stall','2026-10-20 01:22:16')]:
    grouped = defaultdict(list)
    for task in tasks:
        if lower <= task['start'] < cutoff:
            grouped[task['quest']].append(task)
    for quest, group in grouped.items():
        completed = [t for t in group if t['outcome']!='censored']
        seconds = sum(float(t['seconds']) for t in completed)
        success = sum(t['outcome']=='success' for t in completed)
        reward = catalog[quest]['reward_bp']
        direct = sum(int(e['bp']) for t in completed for e in selected_events
                     if e['run']==t['run'] and t['start']<=e['time']<=t['end'] and e['quest']==quest)
        assert direct == reward*success, (scope,quest,direct,reward,success)
        adjusted = direct + (bonus_credit*success if catalog[quest]['tier']=='blue' else 0)
        rows.append(dict(scope=scope,quest=quest,tier=catalog[quest]['tier'],reward_bp=reward,
                         reroll_bp=catalog[quest]['reroll_bp'],batches=len(completed),successes=success,
                         censored_batches=len(group)-len(completed),total_minutes=round(seconds/60,3),
                         mean_batch_minutes=round(seconds/60/len(completed),3) if completed else '',
                         direct_reward_bp=direct,direct_bp_per_task_hour=round(direct*3600/seconds,1) if seconds else '',
                         illustrative_bonus_adjusted_bp_per_task_hour=round(adjusted*3600/seconds,1) if seconds else '',
                         verification_failures=sum(int(t['verification_failures']) for t in completed)))
rows.sort(key=lambda r:(r['scope'], -(r['direct_bp_per_task_hour'] or 0)))
with (root/'quest-velocity.csv').open('w',newline='') as stream:
    writer=csv.DictWriter(stream,fieldnames=list(rows[0]),lineterminator="\n")
    writer.writeheader();writer.writerows(rows)
print(f'Illustrative bonus credit: {bonus_credit:.1f} BP per blue completion (not extra ledger earnings).')
for row in rows:
    if row['scope']=='all-pre-stall':
        print(row['quest'],f"{row['successes']}/{row['batches']}",row['total_minutes'],row['direct_bp_per_task_hour'],row['illustrative_bonus_adjusted_bp_per_task_hour'],f"censored={row['censored_batches']}")
