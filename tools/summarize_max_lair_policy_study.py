#!/usr/bin/env python3
"""Summarize native simulation CSVs; this does not implement or test game logic."""
import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path


def load(path):
    with path.open() as source:
        return list(csv.DictReader(source))


def dump(path, rows):
    with path.open('w', newline='') as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)


def summarize(root):
    raw = load(root / 'results.csv')
    comparisons = load(root / 'comparisons.csv')
    base = defaultdict(dict)
    for row in raw:
        if row['variant'] == 'empirical':
            base[row['model'], row['hours'], row['split']][row['policy']] = row
    if len(base) != 24 * 3 * 2 or any(len(x) != 16 for x in base.values()):
        raise SystemExit('Study is incomplete: expected 24 models, 3 horizons, 2 splits, 16 policies.')
    rows = []
    chosen = {}
    for (model, hours, split), train in sorted(base.items()):
        if split != 'train':
            continue
        heldout = base[model, hours, 'heldout']
        selected = {}
        for family in ('observed', 'likely'):
            choices = [p for p in train if p.startswith(family + '-')]
            selected[family] = max(choices, key=lambda p: float(train[p]['mean_wins']))
        chosen[model, hours] = selected
        rows.append(dict(model=model, hours=hours,
                         best_observed=selected['observed'], best_likely=selected['likely'],
                         observed75_wins=heldout['observed-75']['mean_wins'],
                         tuned_observed_wins=heldout[selected['observed']]['mean_wins'],
                         likely75_wins=heldout['likely-75']['mean_wins'],
                         tuned_likely_wins=heldout[selected['likely']]['mean_wins'],
                         standard_wins=heldout['standard']['mean_wins']))
    dump(root / 'selected_policies.csv', rows)
    robust = []
    groups = sorted({name.rsplit('-', 1)[0] for name, _, _ in base})
    for group in groups:
        for hours in ('8', '24', '72'):
            models = sorted({m for m, h, _ in base if m.rsplit('-', 1)[0] == group and h == hours})
            for family in ('observed', 'likely'):
                policies = [p for p in base[models[0], hours, 'train'] if p.startswith(family + '-')]
                def worst_regret(policy, split):
                    regrets = []
                    for model in models:
                        results = base[model, hours, split]
                        best = max(float(x['mean_wins']) for x in results.values())
                        regrets.append(1 - float(results[policy]['mean_wins']) / best)
                    return max(regrets)
                policy = min(policies, key=lambda p: worst_regret(p, 'train'))
                robust.append(dict(group=group, hours=hours, family=family, policy=policy,
                                   train_worst_shortfall_pct=100 * worst_regret(policy, 'train'),
                                   heldout_worst_shortfall_pct=100 * worst_regret(policy, 'heldout')))
    dump(root / 'robust_choices.csv', robust)
    lookup = {(x['model'], x['hours'], x['variant'], x['policy_a'], x['policy_b']): x for x in comparisons}
    comparisons75 = []
    tuned_comparisons = []
    for row in rows:
        keys = row['model'], row['hours'], 'empirical'
        comparisons75.append(lookup[keys + ('likely-75', row['best_observed'])])
        tuned_comparisons.append(lookup[keys + (row['best_likely'], row['best_observed'])])
    def tally(data):
        positive = sum(float(x['delta_wins']) > 1.96 * float(x['paired_se_wins']) for x in data)
        negative = sum(float(x['delta_wins']) < -1.96 * float(x['paired_se_wins']) for x in data)
        return dict(positive=positive, negative=negative, unresolved=len(data) - positive - negative)
    counts = {'likely75_vs_tuned_observed': tally(comparisons75), 'tuned_likely_vs_tuned_observed': tally(tuned_comparisons)}
    standard_equivalents = 0
    for row, comparison in zip(rows, tuned_comparisons):
        if float(comparison['delta_wins']) <= 1.96 * float(comparison['paired_se_wins']):
            continue
        heldout = base[row['model'], row['hours'], 'heldout']
        algorithm = heldout[row['best_likely']]
        standard = heldout['standard']
        standard_equivalents += (algorithm['mean_wins'] == standard['mean_wins']
                                 and float(algorithm['mean_resets']) == 0)
    counts['tuned_likely_advantages_exactly_matching_standard'] = standard_equivalents
    sensitivity_counts = []
    for variant in sorted({x['variant'] for x in comparisons}):
        selected_comparisons = {}
        for comparison in comparisons:
            choice = chosen[comparison['model'], comparison['hours']]
            if comparison['variant'] == variant and comparison['policy_a'] == choice['likely'] and comparison['policy_b'] == choice['observed']:
                selected_comparisons[comparison['model'], comparison['hours']] = comparison
        sensitivity_counts.append(dict(variant=variant, **tally(list(selected_comparisons.values()))))
    dump(root / 'sensitivity_comparison_counts.csv', sensitivity_counts)
    (root / 'comparison_counts.json').write_text(json.dumps(counts, indent=2) + '\n')
    lines = ['# Strong Boss policy study, 2026-10-28', '',
             '**These are hypothetical route-quality distributions, not fitted estimates of the game.** '
             'Mewtwo, Cresselia and Xurkitree means and timing pools come from the historical Standard logs. '
             'Xerneas means of 40% and 55%, and the easy-boss mean of 90%, are explicit sensitivity assumptions. '
             'They use Mewtwo timings because the Xerneas Standard sample is too small. Pheromosa is excluded.', '',
             '## Assessment', '',
             'Keep the existing implementation as the baseline and tune its threshold before adding PR55\'s policy. '
             'The new policy does not show a consistent advantage over tuned thresholds in this model family. '
             'Its default 75% setting is particularly vulnerable to settling below the best available route.', '',
             f'Of the {counts["tuned_likely_vs_tuned_observed"]["positive"]} cases favoring tuned PR55 over tuned existing thresholds, '
             f'**{standard_equivalents} exactly match the Standard control and perform zero resets**. '
             'Those gains do not require Bayesian route keeping: they come from never retaining a route. '
             'The higher target can make PR55 discard even a first-run win; this is why a 90% setting can behave like Standard.', '',
             'Tentative starting points for a 24-hour fresh hunt, within the tested assumptions: '
             'Mewtwo around 80%, Cresselia around 85%, Xurkitree around 80%. '
             'For the hypothetical 40–55% Xerneas means, 60–70% is the robust tested range. '
             'These are candidates for validation, not established optimal settings. '
             'They do not justify discarding an already established good route.', '',
             '## Design', '',
             '- 24 models: six boss/mean groups, each with homogeneous routes, a 5% chance of a 95% route, '
             'a 35% chance of a 95% route, or a capped best-route rate. Every shape within a group has exactly the same fresh-route mean.',
             '- 8-, 24- and 72-hour budgets. All completed wins count, including exploration. '
             'Abandoning a route loses it; there is no recall or stored library of good routes.',
             '- Existing observed-rate thresholds: 0%, 60%, 70%, 75%, 80%, 85%, 90%, 95%. '
             '0% is the always-retain control. Standard-style always-continue is another control.',
             '- Real PR #55 policy linked from code revision `4bf135d85fa001071dbc9b5550d0a08bb28c5c08`, '
             'tested at 60%, 70%, 75%, 80%, 85%, 90%, 95%. No copied implementation of its Bayesian math.',
             '- 500 training seeds choose thresholds; 2,000 different held-out seeds evaluate them. '
             'Counter-based random draws give each policy the same potential routes and battle outcomes. '
             'The policy sees only simulated outcomes, never the underlying route probability.',
             '- Measured decision-to-end medians are 26.00 seconds for reset and 18.25 for continue. '
             'The base model adds their 7.74-second difference to sampled Standard attempt durations. '
             'This is a tail-overhead estimate, not a measurement of every possible reset cost.',
             '- Sensitivities: zero reset overhead, 60-second extra reset overhead, good routes taking 20% longer, '
             'and a 999-ore budget with a mature 10-ore reset cost and no replenishment. '
             'The ore case is a conservative stress model, not a reconstruction of the game\'s full ore economy. '
             'Sensitivity runs keep the thresholds selected in the base model.',
             '', '## Held-out results at 24 hours', '',
             'Cells show expected boss catches in the entire 24-hour budget. '
             'Thresholds were selected independently on training seeds for each assumed scenario. '
             'That is an optimistic benchmark: we do not know which scenario describes a real boss.', '',
             '| Model | Standard control | Existing 75% | Tuned existing threshold | Tuned existing catches | PR55 at 75% | Tuned PR55 threshold | Tuned PR55 catches |',
             '| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |']
    for r in rows:
        if r['hours'] == '24':
            lines.append(f"| {r['model']} | {float(r['standard_wins']):.2f} | {float(r['observed75_wins']):.2f} | {r['best_observed'].split('-')[1]}% | "
                         f"{float(r['tuned_observed_wins']):.2f} | {float(r['likely75_wins']):.2f} | "
                         f"{r['best_likely'].split('-')[1]}% | {float(r['tuned_likely_wins']):.2f} |")
    lines += ['', '## One threshold when the quality distribution is unknown', '',
              'The following choices minimize the worst training-set shortfall across the four shapes '
              'for a boss/mean group. They are robustness heuristics within this small model family, '
              'not proven game-optimal settings. Near ties should not be treated as meaningful recommendations.', '',
              '| Group | Existing threshold: 8h / 24h / 72h | PR55 threshold: 8h / 24h / 72h | Worst held-out 24h shortfall: existing / PR55 |',
              '| --- | --- | --- | --- |']
    ri = {(x['group'], x['hours'], x['family']): x for x in robust}
    for group in groups:
        choices = [' / '.join(ri[group, h, family]['policy'].split('-')[1] + '%' for h in ('8', '24', '72')) for family in ('observed', 'likely')]
        regrets = [ri[group, '24', f]['heldout_worst_shortfall_pct'] for f in ('observed', 'likely')]
        lines.append(f'| {group} | {choices[0]} | {choices[1]} | {regrets[0]:.1f}% / {regrets[1]:.1f}% |')
    lines += ['', '## Sensitivity comparisons', '',
              'Tuned PR55 versus tuned existing thresholds, using the base-model training choices in each sensitivity. '
              'Counts use the same paired +/-1.96 Monte Carlo SE band; they are not real-game confidence estimates.', '',
              '| Timing/budget model | PR55 higher | Existing higher | Unresolved |',
              '| --- | ---: | ---: | ---: |']
    for count in sensitivity_counts:
        lines.append(f"| {count['variant']} | {count['positive']} | {count['negative']} | {count['unresolved']} |")
    lines += ['', '## Evidence limits', '',
              f'- Across 72 base-model/horizon cases, PR55 at 75% is higher than tuned existing in '
              f'{counts["likely75_vs_tuned_observed"]["positive"]}, lower in {counts["likely75_vs_tuned_observed"]["negative"]}, '
              f'and unresolved in {counts["likely75_vs_tuned_observed"]["unresolved"]} using a +/-1.96 paired Monte Carlo SE band. '
              'The analogous counts after tuning PR55 are ' + str(counts['tuned_likely_vs_tuned_observed']) + '.',
              '- These bands describe simulation sampling error within an assumed model. '
              'They do not quantify uncertainty about the real game, and are not adjusted for multiple comparisons.',
              '- Route outcomes are modeled as independent Bernoulli trials with a stable probability per route. '
              'Timing samples are conditional on win/loss, with no other route-specific timing correlation except the explicit sensitivity.',
              '- Exploration begins without a known retained route. A session that starts on an already established '
              '14/19 or 80/100 route is a different decision problem and is not covered by these initial-state results.',
              '- The model does not stop at the first shiny, price balls, simulate interruptions, or model non-boss shiny decisions. '
              'It measures expected eligible boss catches within a fixed time budget.',
              '- Six deterministic native checks cover certain wins/losses, route retention, time cutoff/reset overhead and ore exhaustion. '
              'The application and its PR branch have not been modified by this study.', '',
              '## Reproduction', '',
              'See `RUN.md` for build/run commands. `models.tsv` fixes every probability assumption and empirical timing sample. '
              '`results.csv` contains training, held-out and sensitivity results. `comparisons.csv` includes paired differences '
              'and their Monte Carlo SE. `selected_policies.csv` covers all horizons; `robust_choices.csv` records the minimax-shortfall exercise.', '']
    (root / 'REPORT.md').write_text('\n'.join(lines))
    print(json.dumps(counts, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    summarize(parser.parse_args().directory)
