# Strong Boss policy study, 2026-10-28

**These are hypothetical route-quality distributions, not fitted estimates of the game.** Mewtwo, Cresselia and Xurkitree means and timing pools come from the historical Standard logs. Xerneas means of 40% and 55%, and the easy-boss mean of 90%, are explicit sensitivity assumptions. They use Mewtwo timings because the Xerneas Standard sample is too small. Pheromosa is excluded.

## Assessment

Keep the existing implementation as the baseline and tune its threshold before adding PR55's policy. The new policy does not show a consistent advantage over tuned thresholds in this model family. Its default 75% setting is particularly vulnerable to settling below the best available route.

Of the 23 cases favoring tuned PR55 over tuned existing thresholds, **16 exactly match the Standard control and perform zero resets**. Those gains do not require Bayesian route keeping: they come from never retaining a route. The higher target can make PR55 discard even a first-run win; this is why a 90% setting can behave like Standard.

Tentative starting points for a 24-hour fresh hunt, within the tested assumptions: Mewtwo around 80%, Cresselia around 85%, Xurkitree around 80%. For the hypothetical 40–55% Xerneas means, 60–70% is the robust tested range. These are candidates for validation, not established optimal settings. They do not justify discarding an already established good route.

## Design

- 24 models: six boss/mean groups, each with homogeneous routes, a 5% chance of a 95% route, a 35% chance of a 95% route, or a capped best-route rate. Every shape within a group has exactly the same fresh-route mean.
- 8-, 24- and 72-hour budgets. All completed wins count, including exploration. Abandoning a route loses it; there is no recall or stored library of good routes.
- Existing observed-rate thresholds: 0%, 60%, 70%, 75%, 80%, 85%, 90%, 95%. 0% is the always-retain control. Standard-style always-continue is another control.
- Real PR #55 policy linked from code revision `4bf135d85fa001071dbc9b5550d0a08bb28c5c08`, tested at 60%, 70%, 75%, 80%, 85%, 90%, 95%. No copied implementation of its Bayesian math.
- 500 training seeds choose thresholds; 2,000 different held-out seeds evaluate them. Counter-based random draws give each policy the same potential routes and battle outcomes. The policy sees only simulated outcomes, never the underlying route probability.
- Measured decision-to-end medians are 26.00 seconds for reset and 18.25 for continue. The base model adds their 7.74-second difference to sampled Standard attempt durations. This is a tail-overhead estimate, not a measurement of every possible reset cost.
- Sensitivities: zero reset overhead, 60-second extra reset overhead, good routes taking 20% longer, and a 999-ore budget with a mature 10-ore reset cost and no replenishment. The ore case is a conservative stress model, not a reconstruction of the game's full ore economy. Sensitivity runs keep the thresholds selected in the base model.

## Held-out results at 24 hours

Cells show expected boss catches in the entire 24-hour budget. Thresholds were selected independently on training seeds for each assumed scenario. That is an optimistic benchmark: we do not know which scenario describes a real boss.

| Model | Standard control | Existing 75% | Tuned existing threshold | Tuned existing catches | PR55 at 75% | Tuned PR55 threshold | Tuned PR55 catches |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| cresselia-capped | 90.80 | 90.41 | 85% | 90.80 | 90.13 | 90% | 90.80 |
| cresselia-common95 | 90.84 | 98.22 | 85% | 101.21 | 95.50 | 85% | 100.35 |
| cresselia-homogeneous | 90.78 | 89.91 | 75% | 89.91 | 89.89 | 90% | 90.78 |
| cresselia-rare95 | 90.80 | 90.67 | 90% | 92.89 | 90.15 | 85% | 91.61 |
| easy-capped | 103.47 | 102.82 | 95% | 103.37 | 102.72 | 90% | 103.47 |
| easy-common95 | 103.51 | 103.24 | 90% | 105.22 | 103.00 | 85% | 103.73 |
| easy-homogeneous | 103.53 | 102.50 | 95% | 102.60 | 102.53 | 90% | 103.53 |
| easy-rare95 | 103.53 | 102.48 | 95% | 102.95 | 102.47 | 90% | 103.53 |
| mewtwo-capped | 79.35 | 81.26 | 70% | 81.46 | 81.32 | 75% | 81.32 |
| mewtwo-common95 | 79.37 | 105.61 | 80% | 105.75 | 104.79 | 85% | 105.75 |
| mewtwo-homogeneous | 79.38 | 78.36 | 90% | 78.72 | 78.30 | 90% | 79.38 |
| mewtwo-rare95 | 79.39 | 87.68 | 85% | 90.61 | 84.24 | 85% | 89.73 |
| xerneas-low-capped | 46.76 | 77.14 | 60% | 82.97 | 81.47 | 60% | 83.21 |
| xerneas-low-common95 | 46.80 | 105.99 | 60% | 106.11 | 106.10 | 75% | 106.10 |
| xerneas-low-homogeneous | 46.54 | 46.31 | 80% | 46.34 | 46.33 | 85% | 46.30 |
| xerneas-low-rare95 | 46.65 | 88.05 | 75% | 88.05 | 86.77 | 80% | 87.89 |
| xerneas-mid-capped | 63.99 | 78.59 | 60% | 82.34 | 81.11 | 70% | 81.95 |
| xerneas-mid-common95 | 64.03 | 105.97 | 70% | 105.98 | 105.87 | 80% | 105.98 |
| xerneas-mid-homogeneous | 63.92 | 63.45 | 70% | 63.39 | 63.28 | 90% | 63.92 |
| xerneas-mid-rare95 | 63.89 | 87.58 | 80% | 88.38 | 84.25 | 85% | 88.15 |
| xurkitree-capped | 78.37 | 81.78 | 70% | 82.33 | 82.24 | 75% | 82.24 |
| xurkitree-common95 | 78.38 | 107.32 | 80% | 107.42 | 106.68 | 85% | 107.42 |
| xurkitree-homogeneous | 78.44 | 77.50 | 90% | 77.82 | 77.36 | 90% | 78.44 |
| xurkitree-rare95 | 78.44 | 88.88 | 85% | 91.56 | 84.95 | 85% | 90.78 |

## One threshold when the quality distribution is unknown

The following choices minimize the worst training-set shortfall across the four shapes for a boss/mean group. They are robustness heuristics within this small model family, not proven game-optimal settings. Near ties should not be treated as meaningful recommendations.

| Group | Existing threshold: 8h / 24h / 72h | PR55 threshold: 8h / 24h / 72h | Worst held-out 24h shortfall: existing / PR55 |
| --- | --- | --- | --- |
| cresselia | 85% / 85% / 90% | 85% / 85% / 85% | 0.9% / 1.4% |
| easy | 75% / 95% / 95% | 90% / 85% / 85% | 0.9% / 1.4% |
| mewtwo | 75% / 80% / 80% | 80% / 85% / 85% | 1.1% / 1.1% |
| xerneas-low | 60% / 60% / 60% | 75% / 75% / 75% | 2.7% / 2.1% |
| xerneas-mid | 70% / 70% / 70% | 75% / 80% / 75% | 2.1% / 4.1% |
| xurkitree | 70% / 80% / 75% | 80% / 85% / 85% | 1.9% / 1.9% |

## Sensitivity comparisons

Tuned PR55 versus tuned existing thresholds, using the base-model training choices in each sensitivity. Counts use the same paired +/-1.96 Monte Carlo SE band; they are not real-game confidence estimates.

| Timing/budget model | PR55 higher | Existing higher | Unresolved |
| --- | ---: | ---: | ---: |
| 999-ore-no-replenishment | 22 | 26 | 24 |
| empirical | 23 | 25 | 24 |
| good-routes-20pct-slower | 32 | 13 | 27 |
| no-reset-overhead | 2 | 30 | 40 |
| reset-extra-60s | 28 | 27 | 17 |

## Evidence limits

- Across 72 base-model/horizon cases, PR55 at 75% is higher than tuned existing in 0, lower in 47, and unresolved in 25 using a +/-1.96 paired Monte Carlo SE band. The analogous counts after tuning PR55 are {'positive': 23, 'negative': 25, 'unresolved': 24}.
- These bands describe simulation sampling error within an assumed model. They do not quantify uncertainty about the real game, and are not adjusted for multiple comparisons.
- Route outcomes are modeled as independent Bernoulli trials with a stable probability per route. Timing samples are conditional on win/loss, with no other route-specific timing correlation except the explicit sensitivity.
- Exploration begins without a known retained route. A session that starts on an already established 14/19 or 80/100 route is a different decision problem and is not covered by these initial-state results.
- The model does not stop at the first shiny, price balls, simulate interruptions, or model non-boss shiny decisions. It measures expected eligible boss catches within a fixed time budget.
- Six deterministic native checks cover certain wins/losses, route retention, time cutoff/reset overhead and ore exhaustion. The application and its PR branch have not been modified by this study.

## Reproduction

See `RUN.md` for build/run commands. `models.tsv` fixes every probability assumption and empirical timing sample. `results.csv` contains training, held-out and sensitivity results. `comparisons.csv` includes paired differences and their Monte Carlo SE. `selected_policies.csv` covers all horizons; `robust_choices.csv` records the minimax-shortfall exercise.
