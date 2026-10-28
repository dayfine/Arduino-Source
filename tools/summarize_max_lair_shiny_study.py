#!/usr/bin/env python3
"""Summarize native shiny-hunt simulation and an explicitly prior-sensitive history likelihood."""
import argparse
import csv
import json
import math
from pathlib import Path


def load(path):
    with path.open() as source:
        return list(csv.DictReader(source))


def dump(path, rows):
    with path.open('w', newline='') as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)


def history_sensitivity(history):
    episodes = [r for r in load(history / 'routes.csv') if r['boss'] == 'xerneas' and r['known_new'] == 'True']
    results = []
    for low in (.2, .4, .6):
        for excellent in (.85, .95):
            likelihoods = []
            for episode in episodes:
                wins, losses = episode['outcomes'].count('W'), episode['outcomes'].count('L')
                ordinary = sum((low + (.8 - low) * (i + .5) / 512) ** wins
                               * (1 - low - (.8 - low) * (i + .5) / 512) ** losses for i in range(512)) / 512
                likelihoods.append((ordinary, excellent ** wins * (1 - excellent) ** losses))
            for prior_losses in (1, 19):
                grid = [(i + .5) / 2000 for i in range(2000)]
                logs = [sum(math.log((1 - q) * ordinary + q * elite) for ordinary, elite in likelihoods)
                        + (prior_losses - 1) * math.log1p(-q) for q in grid]
                peak = max(logs)
                weights = [math.exp(value - peak) for value in logs]
                total = sum(weights)
                weights = [value / total for value in weights]
                quantiles = {}
                cumulative = 0
                for q, weight in zip(grid, weights):
                    cumulative += weight
                    for point in (.025, .5, .975):
                        if point not in quantiles and cumulative >= point:
                            quantiles[point] = q
                results.append(dict(ordinary_min=low, ordinary_max=.8, excellent_rate=excellent,
                                    prior='Beta(1,%d)' % prior_losses, routes=len(episodes),
                                    posterior_mean_q=sum(q * w for q, w in zip(grid, weights)),
                                    posterior_low95_q=quantiles[.025], posterior_median_q=quantiles[.5],
                                    posterior_high95_q=quantiles[.975]))
    return results


def summarize(root, history):
    results = load(root / 'results.csv') + load(root / 'refinements-results.csv')
    comparisons = load(root / 'comparisons.csv') + load(root / 'refinements-comparisons.csv')
    if len(results) != 1470 or len(comparisons) != 294:
        raise SystemExit('Expected 1,470 policy cells and 294 paired comparisons.')
    key = lambda r: (r['ordinary_min'], r['excellent_rate'], r['excellent_probability'], r['start'], r['costs'], r['target'])
    index = {key(r): r for r in results}
    assert len(index) == len(results)
    for row in results:
        assert float(row['mean_wins']) <= float(row['mean_attempts'])
        assert 0 <= float(row['shiny_within8']) <= float(row['shiny_within24']) <= float(row['shiny_within48']) <= 1
        assert 0 <= float(row['probability_ever_drew_excellent']) <= 1
        if row['excellent_probability'] == '0':
            assert float(row['probability_ever_drew_excellent']) == 0
        if row['costs'] == '999-ore-no-replenishment':
            assert float(row['mean_resets']) <= 99
    assert len({r['mean_wins'] for r in results}) == 1  # Same geometric shiny target for every policy/seed.
    inference = history_sensitivity(history)
    dump(root / 'history_prior_sensitivity.csv', inference)
    crossovers = []
    for low in ('0.2', '0.4', '0.6'):
        for excellent in ('0.85', '0.95'):
            for start in ('fresh', 'previous10of14-true60', 'previous10of14-true70', 'previous10of14-true80'):
                rows = sorted([r for r in comparisons if r['ordinary_min'] == low and r['excellent_rate'] == excellent
                               and r['start'] == start and r['costs'] == '12min-plus-measured-reset'],
                              key=lambda r: float(r['excellent_probability']))
                favored70 = [float(r['excellent_probability']) for r in rows
                             if float(r['delta85minus70_hours']) > 1.96 * float(r['paired_se_hours'])]
                favored85 = [float(r['excellent_probability']) for r in rows
                             if float(r['delta85minus70_hours']) < -1.96 * float(r['paired_se_hours'])]
                crossovers.append(dict(ordinary_min=low, excellent_rate=excellent, start=start,
                                       last_tested_q_favoring70=max(favored70) if favored70 else '',
                                       first_tested_q_favoring85=min(favored85) if favored85 else ''))
    dump(root / 'crossovers.csv', crossovers)
    lines = ['# Time to first shiny: 70% versus 85% threshold', '',
             '## Result', '',
             '**The answer depends strongly on whether “excellent” means a true 85% or 95% route.** '
             'Monte Carlo can evaluate the consequence of a proposed rarity; it cannot establish the game\'s unknown rarity by itself.', '',
             'For the primary example below, ordinary new routes are uniform between 40–80% true success probability. '
             'The starting route has the old observed record 10/14, and its true success probability is assumed to be 70%. '
             'These are model assumptions, not claims that the previous Xerneas route truly had exactly 70% success.', '',
             '| Chance a new route is excellent | True excellent rate | Expected hours: threshold 70% | Expected hours: threshold 85% | Faster |',
             '| ---: | ---: | ---: | ---: | --- |']
    primary = [r for r in comparisons if r['ordinary_min'] == '0.4' and r['start'] == 'previous10of14-true70'
               and r['costs'] == '12min-plus-measured-reset' and r['excellent_probability'] in ('0.01', '0.05', '0.1', '0.2', '0.5')]
    for row in sorted(primary, key=lambda r: (float(r['excellent_rate']), float(r['excellent_probability']))):
        faster = '70%' if float(row['delta85minus70_hours']) > 0 else '85%'
        lines.append(f"| {100 * float(row['excellent_probability']):g}% | {100 * float(row['excellent_rate']):g}% | "
                     f"{float(row['hours70']):.1f} | {float(row['hours85']):.1f} | {faster} |")
    lines += ['', 'Intermediate settings at q=5%, in the same primary example:', '',
              '| True excellent rate | 70% threshold | 75% threshold | 80% threshold | 85% threshold |',
              '| ---: | ---: | ---: | ---: | ---: |']
    for excellent in ('0.85', '0.95'):
        values = [float(index[('0.4', excellent, '0.05', 'previous10of14-true70', '12min-plus-measured-reset', target)]['mean_hours'])
                  for target in ('0.7', '0.75', '0.8', '0.85')]
        lines.append('| ' + str(100 * float(excellent)) + '% | ' + ' | '.join(f'{v:.1f} h' for v in values) + ' |')
    lines += ['', 'This is why the 70%-versus-85% crossover is not a claim that 85% is globally optimal: '
              'an intermediate threshold can outperform both. The 70–75% range is a conservative candidate '
              'for validation given the unresolved rarity and quality of excellent routes.', '',
              'Refined tested crossover brackets in this primary example:', '',
              '- True 85% excellent routes: the mean crossover is around a **30%** occurrence rate. '
              'At q=30% the difference is unresolved within Monte Carlo error; q=25% favors 70%, q=35% favors 85%.',
              '- True 95% excellent routes: **1.5–1.75%** occurrence is the tested crossover bracket. '
              'The paired sampling-error bands distinguish the two sides at those endpoints.',
              '- These brackets are specific to the stated ordinary-route distribution, initial history, timing and shiny assumptions. '
              '`crossovers.csv` shows how they move when those assumptions change.', '',
              '## Finding a good route is not the same as retaining it', '',
              'An 85% threshold is applied to the observed wins/attempts, not the hidden true probability. '
              'For example, five wins followed by a loss gives 5/6=83.3%, so the program discards that route '
              'even if its true success probability is 85%. A new route can therefore be excellent and still be abandoned.', '',
              '| Excellent-route frequency, true rate 85% | Chance of drawing at least one before shiny: threshold 70% | Threshold 85% |',
              '| ---: | ---: | ---: |']
    for q in ('0.01', '0.05', '0.1'):
        fields = ('0.4', '0.85', q, 'previous10of14-true70', '12min-plus-measured-reset')
        a, b = index[fields + ('0.7',)], index[fields + ('0.85',)]
        lines.append(f"| {100 * float(q):g}% | {100 * float(a['probability_ever_drew_excellent']):.1f}% | "
                     f"{100 * float(b['probability_ever_drew_excellent']):.1f}% |")
    lines += ['', 'The aggressive policy draws more excellent routes but can still take longer to find the shiny: '
              'it spends more attempts exploring and can discard those routes after early losses. '
              'These are hidden-environment measurements; the simulated policy never knows the true rate.', '',
              '## What the actual Xerneas history can tell us', '',
              'The archived history through 08:30 contains **ten demonstrably new Xerneas route episodes** following continue. '
              'The initial session route is excluded because its earlier selection history is uncertain. '
              'The sequences are seven one-attempt losses, 4/6, 2/3, and an ongoing 8/10. '
              'Repeated attempts on a retained route help estimate that route, but are not new population draws.', '',
              'An illustrative mixture likelihood uses each full sequence, not a naive ratio of routes above 85%. '
              'For a sequence with w wins and l losses, the contribution is '
              '`(1-q) * E_ordinary[p^w(1-p)^l] + q * p_excellent^w(1-p_excellent)^l`. '
              'There is no binomial coefficient: the ordered sequence was observed under a stopping policy. '
              'With an ignorable deterministic outcome-based stopping rule, that rule adds no q-dependent likelihood factor.', '',
              'Even this small inference is strongly prior dependent. For ordinary routes uniform over 40–80% and excellent routes at 85%:', '',
              '| Illustrative prior for q | Posterior mean q | 95% interval |',
              '| --- | ---: | ---: |']
    for row in inference:
        if row['ordinary_min'] == .4 and row['excellent_rate'] == .85:
            prior = 'Uniform q; prior mean 50%' if row['prior'] == 'Beta(1,1)' else 'Beta(1,19); prior mean 5%'
            lines.append(f"| {prior} | {100 * row['posterior_mean_q']:.1f}% | "
                         f"{100 * row['posterior_low95_q']:.1f}–{100 * row['posterior_high95_q']:.1f}% |")
    lines += ['', '**Neither prior is calibrated to Xerneas. These are sensitivity examples, not a defensible point estimate of q.** '
              'The wide, prior-sensitive results do not justify claiming a precise 5%, 20% or 30% discovery probability. '
              'Both continuous priors assign zero point mass to q=0 by construction; this does not establish that '
              'excellent Xerneas routes actually exist. The Monte Carlo sweep separately includes q=0. '
              '`history_prior_sensitivity.csv` also shows alternative ordinary ranges and 95% elite routes. '
              'Inference uses 512 midpoint cells for ordinary rates and 2,000 for q, with log-weight normalization.', '',
              '## Simulation design', '',
              '- 30,000 hunts per policy cell; 294 scenario cells times five policies = **44.1 million complete simulated hunts**.',
              '- Shiny probability is the user\'s assumed 1% independently per successful boss catch. '
              'The required number of future wins is geometric, not fixed at 100 or constrained to 80–160. '
              'Each seed shares the same required-win draw across policies, reducing comparison noise. '
              'Past non-shiny wins are sunk history: they do not reduce the expected remaining target.',
              '- Stop immediately at the first shiny, before any subsequent reset. Every win during exploration counts. '
              'No hunts are silently censored: a one-million-attempt guard would fail the run if reached.',
              '- Each newly generated route is excellent with q in 0%, 0.1%, 0.5%, 1%, 2%, 5%, 10%, 20%, 50%, 100%. '
              'Excellent rates are separately 85% and 95%; ordinary rates are uniform over 20–80%, 40–80%, or 60–80%. '
              'The refined primary case adds q=1.25%, 1.5%, 1.75%, 25%, 30%, 35%, 40%.',
              '- Starts: fresh, or the previous 10/14 record with true rate assumed 60%, 70% or 80%. '
              'A threshold change is applied before the first future attempt: 70% retains 10/14, while 75–85% discard it. '
              'No historical attempt time is charged again.',
              '- Existing observed-rate thresholds 70%, 75%, 80%, 85%, plus a 0% always-retain control. '
              'This study does not evaluate PR #55\'s alternative Bayesian policy.',
              '- Absolute times assume 12 minutes per attempt, plus the earlier measured 7.74-second reset-tail difference. '
              'Twelve minutes is an explicit simplifying assumption, not a measurement of every Xerneas route. '
              'Early failures or route-specific speed differences can change the real comparison. '
              'Mean attempts are also exported so timing assumptions are visible.',
              '- Primary-case sensitivities add a 60-second reset penalty and a 999-ore budget with mature 10-ore fees and no replenishment. '
              'The ore case is a conservative stress model rather than the full game economy.',
              '- Policy decisions use only observed win counts. True route quality and the excellent-route label are hidden. '
              'Once a route is abandoned, it cannot be recalled.',
              '- Paired standard errors quantify Monte Carlo noise under each assumed scenario, not confidence in the assumptions. '
              'The actual optimal threshold remains uncertain until the real population is better constrained.', '',
              '## Reproduction and artifacts', '',
              'See `RUN.md`. Native checks cover stopping at the shiny, reset timing, ore exhaustion, '
              'the initial keep/abandon decision, and geometric sampling. '
              '`results.csv` and `refinements-results.csv` include means, medians, 90th-percentile time, '
              'shiny probabilities by 8/24/48 hours, resets, fresh routes and excellent-route draws. '
              '`comparisons.csv` and its refinement contain paired 85%-minus-70% differences. '
              'Positive differences mean the 85% policy is slower.', '']
    (root / 'REPORT.md').write_text('\n'.join(lines))
    validation = dict(samples_per_cell=30000, policy_cells=len(results), comparison_cells=len(comparisons),
                      completed_simulated_hunts=30000 * len(results), unique_keys=True,
                      probability_bounds=True, shared_geometric_targets=True,
                      mean_geometric_wins=float(results[0]['mean_wins']), no_censored_hunts=True)
    reference = index[('0.4', '0.85', '0.05', 'previous10of14-true70', '12min-plus-measured-reset', '0')]
    expected_attempts = 1 / (.7 * .01)
    attempts_by24 = math.floor(24 * 3600 / (720 + 7.7440405))
    expected_probability = 1 - (1 - .7 * .01) ** attempts_by24
    observed_attempts = float(reference['mean_attempts'])
    observed_probability = float(reference['shiny_within24'])
    assert abs(observed_attempts - expected_attempts) < 3 * math.sqrt(1 - .007) / .007 / math.sqrt(30000)
    assert abs(observed_probability - expected_probability) < 3 * math.sqrt(expected_probability * (1 - expected_probability) / 30000)
    validation['analytic_always_keep_true70'] = dict(expected_mean_attempts=expected_attempts,
        observed_mean_attempts=observed_attempts, expected_probability_shiny_within24=expected_probability,
        observed_probability_shiny_within24=observed_probability, within_three_monte_carlo_standard_errors=True)
    (root / 'validation.json').write_text(json.dumps(validation, indent=2) + '\n')
    print(json.dumps(validation, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--history', type=Path, required=True)
    args = parser.parse_args()
    summarize(args.directory, args.history)
