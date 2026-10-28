# Sword and Shield analysis

- [Max Lair history through 2026-10-28 08:30](max_lair/history-2026-10-28/REPORT.md): Standard, Strong Boss and BossFinder win rates per boss; reset-chain deduplication and per-attempt evidence.
- [Strong Boss policy study](max_lair/policy-study-2026-10-28/REPORT.md): threshold sweeps versus real PR #55 policy, held-out simulation and timing/ore sensitivities. Hypothetical route-quality distributions; not live-validated settings.
- Rebuild history with [`tools/max_lair_history.py`](../tools/max_lair_history.py). It only reads application logs. All exports and analysis belong on `agents`, not a code PR.

Use Standard as the ordinary-play baseline and Strong Boss as selected-route evidence. The report records uncertainty at program restarts and the user-confirmed Pheromosa farm after the boss had already been caught (excluded from battle-win baselines). Do not infer the frequency of true 80–95% routes directly from selected, variably tested routes.
