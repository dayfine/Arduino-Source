# Sword and Shield analysis

- [Max Lair history through 2026-10-28 08:30](max_lair/history-2026-10-28/REPORT.md): Standard, Strong Boss and BossFinder win rates per boss; reset-chain deduplication and per-attempt evidence.
- Rebuild with [`tools/max_lair_history.py`](../tools/max_lair_history.py). It only reads application logs. All exports and analysis belong on `agents`, not a code PR.

Use Standard as the ordinary-play baseline and Strong Boss as selected-route evidence. The report records uncertainty at program restarts and the Pheromosa success-counter anomaly. Do not infer the frequency of true 80–95% routes directly from selected, variably tested routes.
