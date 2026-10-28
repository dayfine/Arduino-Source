# Reproducing the shiny-hunt simulation

Run from the fork's `agents` notes checkout. This is offline analysis, not game
code or application unit tests. The native engine uses the existing observed
wins/attempts threshold rule and shares the deterministic RNG helper with the
previous policy study. No GUI, game checkout, resources or hardware is needed.

```sh
c++ -std=c++23 -O2 -Wall -Wextra -Werror \
  tools/max_lair_shiny_simulator.cpp -o /tmp/max-lair-shiny-simulator

MAX_LAIR_SHINY_OUT=pokemon_swsh/max_lair/shiny-study-2026-10-28
/tmp/max-lair-shiny-simulator --selftest
/tmp/max-lair-shiny-simulator "$MAX_LAIR_SHINY_OUT/results.csv" \
  "$MAX_LAIR_SHINY_OUT/comparisons.csv" 30000
/tmp/max-lair-shiny-simulator "$MAX_LAIR_SHINY_OUT/refinements-results.csv" \
  "$MAX_LAIR_SHINY_OUT/refinements-comparisons.csv" 30000 refine

python3 tools/summarize_max_lair_shiny_study.py "$MAX_LAIR_SHINY_OUT" \
  --history pokemon_swsh/max_lair/history-2026-10-28
```

Seeds are 23,000,000 through 23,029,999. A geometric(0.01) required-win count is
drawn once per seed and reused across policies. Route quality and battle
outcomes use different counter-based RNG channels. Quality is hidden from the
policy. First-shiny stopping is equivalent to drawing an independent shiny
check after each successful catch; exploration wins count and the final shiny
catch incurs no later reset.

The full sweep is 280 scenarios times five policies. Refinement adds 14
scenarios, also times five policies. Initial route histories are 10/14, but
those 14 past attempts and wins are not charged or credited to the future hunt.
For a known initial route, its true rate is separately assumed to be 60%, 70%
or 80%. With a 70% target, 10/14 is retained; 75–85% abandon it immediately.

Native checks cover first-shiny stopping, reset costs, ore exhaustion, the
initial threshold decision and geometric sampling. An always-retain known-70%
route is also checked against the analytic mean 1/(0.70*0.01) attempts and the
analytic chance of finding the shiny within 24 hours. Results agree within
three Monte Carlo SEs. Apple Clang 21, C++23, `-O2 -Wall -Wextra -Werror`.

The Python summary also performs an illustrative historical mixture-likelihood
calculation. It uses ten demonstrably new archived Xerneas episodes, excludes
the uncertain initial session route, and compares two explicitly uncalibrated
priors for excellent-route frequency. It does not run application tests or
extract production code. See REPORT.md for the model and its limitations.

The two-point elite-rate cases (85% and 95%) and uniform ordinary distributions
are sensitivity assumptions. No generation frequency is claimed as measured
from the game. Absolute hours assume 12-minute attempts plus reset overhead;
mean attempts are exported to avoid concealing that assumption. The limited
ore variant forces continue after exhausting 999 ore at 10 per reset, without
crediting any newly earned ore; it is deliberately a simplified stress test.
