# Time to first shiny: 70% versus 85% threshold

## Result

**The answer depends strongly on whether “excellent” means a true 85% or 95% route.** Monte Carlo can evaluate the consequence of a proposed rarity; it cannot establish the game's unknown rarity by itself.

For the primary example below, ordinary new routes are uniform between 40–80% true success probability. The starting route has the old observed record 10/14, and its true success probability is assumed to be 70%. These are model assumptions, not claims that the previous Xerneas route truly had exactly 70% success.

| Chance a new route is excellent | True excellent rate | Expected hours: threshold 70% | Expected hours: threshold 85% | Faster |
| ---: | ---: | ---: | ---: | --- |
| 1% | 85% | 28.2 | 30.5 | 70% |
| 5% | 85% | 27.4 | 28.9 | 70% |
| 10% | 85% | 26.7 | 27.7 | 70% |
| 20% | 85% | 25.9 | 26.3 | 70% |
| 50% | 85% | 25.0 | 24.6 | 85% |
| 1% | 95% | 27.9 | 28.5 | 70% |
| 5% | 95% | 26.2 | 24.8 | 85% |
| 10% | 95% | 25.1 | 23.3 | 85% |
| 20% | 95% | 23.9 | 22.3 | 85% |
| 50% | 95% | 22.8 | 21.6 | 85% |

Intermediate settings at q=5%, in the same primary example:

| True excellent rate | 70% threshold | 75% threshold | 80% threshold | 85% threshold |
| ---: | ---: | ---: | ---: | ---: |
| 85.0% | 27.4 h | 27.2 h | 27.7 h | 28.9 h |
| 95.0% | 26.2 h | 25.3 h | 24.9 h | 24.8 h |

This is why the 70%-versus-85% crossover is not a claim that 85% is globally optimal: an intermediate threshold can outperform both. The 70–75% range is a conservative candidate for validation given the unresolved rarity and quality of excellent routes.

Refined tested crossover brackets in this primary example:

- True 85% excellent routes: the mean crossover is around a **30%** occurrence rate. At q=30% the difference is unresolved within Monte Carlo error; q=25% favors 70%, q=35% favors 85%.
- True 95% excellent routes: **1.5–1.75%** occurrence is the tested crossover bracket. The paired sampling-error bands distinguish the two sides at those endpoints.
- These brackets are specific to the stated ordinary-route distribution, initial history, timing and shiny assumptions. `crossovers.csv` shows how they move when those assumptions change.

## Finding a good route is not the same as retaining it

An 85% threshold is applied to the observed wins/attempts, not the hidden true probability. For example, five wins followed by a loss gives 5/6=83.3%, so the program discards that route even if its true success probability is 85%. A new route can therefore be excellent and still be abandoned.

| Excellent-route frequency, true rate 85% | Chance of drawing at least one before shiny: threshold 70% | Threshold 85% |
| ---: | ---: | ---: |
| 1% | 10.2% | 33.5% |
| 5% | 35.2% | 72.1% |
| 10% | 51.6% | 84.6% |

The aggressive policy draws more excellent routes but can still take longer to find the shiny: it spends more attempts exploring and can discard those routes after early losses. These are hidden-environment measurements; the simulated policy never knows the true rate.

## What the actual Xerneas history can tell us

The archived history through 08:30 contains **ten demonstrably new Xerneas route episodes** following continue. The initial session route is excluded because its earlier selection history is uncertain. The sequences are seven one-attempt losses, 4/6, 2/3, and an ongoing 8/10. Repeated attempts on a retained route help estimate that route, but are not new population draws.

An illustrative mixture likelihood uses each full sequence, not a naive ratio of routes above 85%. For a sequence with w wins and l losses, the contribution is `(1-q) * E_ordinary[p^w(1-p)^l] + q * p_excellent^w(1-p_excellent)^l`. There is no binomial coefficient: the ordered sequence was observed under a stopping policy. With an ignorable deterministic outcome-based stopping rule, that rule adds no q-dependent likelihood factor.

Even this small inference is strongly prior dependent. For ordinary routes uniform over 40–80% and excellent routes at 85%:

| Illustrative prior for q | Posterior mean q | 95% interval |
| --- | ---: | ---: |
| Uniform q; prior mean 50% | 18.2% | 0.5–59.5% |
| Beta(1,19); prior mean 5% | 4.2% | 0.1–14.9% |

**Neither prior is calibrated to Xerneas. These are sensitivity examples, not a defensible point estimate of q.** The wide, prior-sensitive results do not justify claiming a precise 5%, 20% or 30% discovery probability. Both continuous priors assign zero point mass to q=0 by construction; this does not establish that excellent Xerneas routes actually exist. The Monte Carlo sweep separately includes q=0. `history_prior_sensitivity.csv` also shows alternative ordinary ranges and 95% elite routes. Inference uses 512 midpoint cells for ordinary rates and 2,000 for q, with log-weight normalization.

## Simulation design

- 30,000 hunts per policy cell; 294 scenario cells times five policies = **44.1 million complete simulated hunts**.
- Shiny probability is the user's assumed 1% independently per successful boss catch. The required number of future wins is geometric, not fixed at 100 or constrained to 80–160. Each seed shares the same required-win draw across policies, reducing comparison noise. Past non-shiny wins are sunk history: they do not reduce the expected remaining target.
- Stop immediately at the first shiny, before any subsequent reset. Every win during exploration counts. No hunts are silently censored: a one-million-attempt guard would fail the run if reached.
- Each newly generated route is excellent with q in 0%, 0.1%, 0.5%, 1%, 2%, 5%, 10%, 20%, 50%, 100%. Excellent rates are separately 85% and 95%; ordinary rates are uniform over 20–80%, 40–80%, or 60–80%. The refined primary case adds q=1.25%, 1.5%, 1.75%, 25%, 30%, 35%, 40%.
- Starts: fresh, or the previous 10/14 record with true rate assumed 60%, 70% or 80%. A threshold change is applied before the first future attempt: 70% retains 10/14, while 75–85% discard it. No historical attempt time is charged again.
- Existing observed-rate thresholds 70%, 75%, 80%, 85%, plus a 0% always-retain control. This study does not evaluate PR #55's alternative Bayesian policy.
- Absolute times assume 12 minutes per attempt, plus the earlier measured 7.74-second reset-tail difference. Twelve minutes is an explicit simplifying assumption, not a measurement of every Xerneas route. Early failures or route-specific speed differences can change the real comparison. Mean attempts are also exported so timing assumptions are visible.
- Primary-case sensitivities add a 60-second reset penalty and a 999-ore budget with mature 10-ore fees and no replenishment. The ore case is a conservative stress model rather than the full game economy.
- Policy decisions use only observed win counts. True route quality and the excellent-route label are hidden. Once a route is abandoned, it cannot be recalled.
- Paired standard errors quantify Monte Carlo noise under each assumed scenario, not confidence in the assumptions. The actual optimal threshold remains uncertain until the real population is better constrained.

## Reproduction and artifacts

See `RUN.md`. Native checks cover stopping at the shiny, reset timing, ore exhaustion, the initial keep/abandon decision, and geometric sampling. `results.csv` and `refinements-results.csv` include means, medians, 90th-percentile time, shiny probabilities by 8/24/48 hours, resets, fresh routes and excellent-route draws. `comparisons.csv` and its refinement contain paired 85%-minus-70% differences. Positive differences mean the 85% policy is slower.
