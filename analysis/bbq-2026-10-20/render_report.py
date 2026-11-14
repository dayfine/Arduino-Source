"""Render distribution figures and machine-readable summary from the ledger."""
import csv
from collections import Counter
from datetime import datetime
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.dates as mdates
import matplotlib.pyplot as plt
import numpy as np

root = Path(__file__).resolve().parent
windows = list(csv.DictReader((root/'windows.csv').open()))
events = list(csv.DictReader((root/'events.csv').open()))
runs = list(csv.DictReader((root/'runs.csv').open()))
rewards = json.loads((root/'quest-rewards.json').read_text())['quests']
summaries = []
fig, axes = plt.subplots(1, 3, figsize=(13, 4))
for minutes, ax in zip([10,30,60], axes):
    selected = [r for r in windows if int(r['minutes']) == minutes and float(r['observed_minutes']) >= minutes-.001]
    values = np.array([int(r['net_bp']) for r in selected])
    row = dict(minutes=minutes, windows=len(values), mean=float(values.mean()),
               median=float(np.median(values)), p10=float(np.quantile(values,.1)),
               p90=float(np.quantile(values,.9)), minimum=int(values.min()),
               maximum=int(values.max()), zero_windows=int((values==0).sum()))
    summaries.append(row)
    freq = Counter(values)
    ax.bar(list(freq),list(freq.values()),width={10:8,30:15,60:25}[minutes],color='#236b8e')
    ax.set(title=f'{minutes}-minute windows (n={len(values)})',xlabel='Net BP per window',ylabel='Window count')
    ax.axvline(row['mean'],color='#d05c36',linestyle='--',label=f"Mean {row['mean']:.1f}")
    ax.legend()
fig.suptitle('Estimated overnight BBQ returns — full clock-aligned windows, including pauses')
fig.tight_layout()
fig.savefig(root/'return-distributions.png',dpi=170)
fig.savefig(root/'return-distributions.svg')
plt.close(fig)

fig,(ax,bar)=plt.subplots(2,1,figsize=(13,7),sharex=True,gridspec_kw={'height_ratios':[2,1]})
total=0
xs=[datetime.fromisoformat(runs[0]['start'])];ys=[0]
for e in events:
    total += int(e['bp'])*(1 if e['kind']=='reward' else -1)
    xs.append(datetime.fromisoformat(e['time']));ys.append(total)
xs.append(datetime.fromisoformat(runs[-1]['end']));ys.append(total)
ax.step(xs,ys,where='post',color='#236b8e')
ax.set(ylabel='Cumulative net BP',title='Inferred BBQ rewards minus rerolls, Oct 19–20, 2026 (local log timestamps)')
stall=datetime(2026,10,20,5,5,19)
for a in [ax,bar]:
    a.axvspan(stall,xs[-1],color='#d05c36',alpha=.12)
ax.annotate('No further observed completions',xy=(stall,total),xytext=(datetime(2026,10,20,5,25),total*.7),arrowprops={'arrowstyle':'->'})
ww=[r for r in windows if r['minutes']=='10']
bar.bar([datetime.fromisoformat(r['start']) for r in ww],[int(r['net_bp']) for r in ww],width=10/(24*60)*.9,align='edge',color='#236b8e')
bar.set(ylabel='Net BP / 10 min',xlabel='Local time')
bar.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
fig.tight_layout()
fig.savefig(root/'overnight-timeline.png',dpi=170)
fig.savefig(root/'overnight-timeline.svg')
plt.close(fig)

gross=sum(int(e['bp']) for e in events if e['kind']=='reward')
spent=sum(int(e['bp']) for e in events if e['kind']=='reroll')
active=sum((datetime.fromisoformat(r['end'])-datetime.fromisoformat(r['start'])).total_seconds() for r in runs)/3600
elapsed=(datetime.fromisoformat(runs[-1]['end'])-datetime.fromisoformat(runs[0]['start'])).total_seconds()/3600
summary=dict(gross_bp=gross,spent_bp=spent,net_bp=gross-spent,active_hours=active,elapsed_hours=elapsed,
             net_bp_per_active_hour=(gross-spent)/active,net_bp_per_elapsed_hour=(gross-spent)/elapsed,
             distributions=summaries)
(root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
with (root/'quest-catalog.csv').open('w',newline='') as stream:
    writer=csv.writer(stream,lineterminator="\n")
    writer.writerow(['quest_slug','variant','tier','reward_bp','reroll_bp','reroll_note'])
    for slug,q in rewards.items():
        options=list(zip([3,4,5],q['reward_bp'])) if isinstance(q['reward_bp'],list) else [('',q['reward_bp'])]
        for variant,reward in options:
            writer.writerow([slug,variant,q['tier'],reward,q['reroll_bp'] if q['reroll_bp'] is not None else '',
                             'gold cannot be individually rerolled (see report sources)' if q['tier']=='gold' else ''])
print(json.dumps(summary,indent=2))

# Keep generated SVG text clean in repository diffs.
for svg in root.glob("*.svg"):
    svg.write_text("\n".join(line.rstrip() for line in svg.read_text().splitlines()) + "\n")
