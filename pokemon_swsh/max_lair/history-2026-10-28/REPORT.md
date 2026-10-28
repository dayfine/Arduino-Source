# Observed Max Lair history, 2026-10-28

Cutoff: **2026-10-28 08:30:00 America/Los_Angeles**, using the main local log archive.

Reconstructed **2,924 completed attempts** across **36 identified bosses**. 4 completed attempts have no identified boss. 46 uncompleted attempts are excluded from win-rate denominators.

## Standard versus Strong Boss

Each cell is recorded wins / completed attempts. BossFinder stays separate: its exploration and boss-selection behavior makes it a different baseline. Small samples are displayed but should not drive a settling threshold.

| Boss | Standard | Strong Boss | BossFinder |
| --- | ---: | ---: | ---: |
| articuno | 1/1 (100.0%) | — | — |
| azelf | 0/1 (0.0%) | — | 1/2 (50.0%) |
| blacephalon | 3/4 (75.0%) | — | 0/1 (0.0%) |
| buzzwole | 84/109 (77.1%) | — | 2/2 (100.0%) |
| celesteela | 16/29 (55.2%) | 12/15 (80.0%) | 0/2 (0.0%) |
| cresselia | 150/183 (82.0%) | — | 2/3 (66.7%) |
| dialga | 0/2 (0.0%) | — | 1/2 (50.0%) |
| entei | — | 130/144 (90.3%) | — |
| giratina-altered | 0/1 (0.0%) | — | 1/1 (100.0%) |
| groudon | 1/2 (50.0%) | — | — |
| guzzlord | 214/252 (84.9%) | — | — |
| heatran | 2/4 (50.0%) | — | 3/3 (100.0%) |
| ho-oh | — | — | 1/1 (100.0%) |
| kartana | 31/37 (83.8%) | — | 0/1 (0.0%) |
| kyurem | 0/1 (0.0%) | — | 0/1 (0.0%) |
| landorus-incarnate | 0/1 (0.0%) | — | 2/2 (100.0%) |
| latios | 1/1 (100.0%) | — | 3/3 (100.0%) |
| mesprit | 29/29 (100.0%) | — | 2/2 (100.0%) |
| mewtwo | 550/803 (68.5%) | 60/70 (85.7%) | 0/1 (0.0%) |
| moltres | 1/1 (100.0%) | 69/76 (90.8%) | 1/2 (50.0%) |
| nihilego | 57/67 (85.1%) | — | 0/1 (0.0%) |
| pheromosa | 0/85 (0.0%) | — | — |
| raikou | 2/2 (100.0%) | 108/112 (96.4%) | 0/1 (0.0%) |
| reshiram | 1/1 (100.0%) | — | 1/1 (100.0%) |
| solgaleo | 1/1 (100.0%) | — | 1/1 (100.0%) |
| stakataka | — | — | 2/3 (66.7%) |
| suicune | 1/2 (50.0%) | 147/149 (98.7%) | 2/2 (100.0%) |
| tapu-bulu | 21/24 (87.5%) | 324/353 (91.8%) | 2/2 (100.0%) |
| tapu-fini | — | — | 1/1 (100.0%) |
| tapu-koko | 3/3 (100.0%) | — | 1/2 (50.0%) |
| tapu-lele | 2/2 (100.0%) | — | 2/3 (66.7%) |
| tornadus-incarnate | — | — | 3/4 (75.0%) |
| xerneas | 1/2 (50.0%) | 28/45 (62.2%) | 1/2 (50.0%) |
| xurkitree | 112/168 (66.7%) | — | 2/2 (100.0%) |
| zapdos | 1/1 (100.0%) | 69/81 (85.2%) | 1/1 (100.0%) |
| zygarde-50 | — | — | 0/1 (0.0%) |

## Strong Boss: deduplicating reset chains

**46 route episodes**, representing **42 map + starting-rental fingerprint groups**. 4 matching fingerprint groups span multiple program sessions. Within a session, an explicit reset links attempts to the same episode; an explicit continue ends it. Cross-session fingerprint matches are probable duplicates, not proof of identical hidden seeds.

| Boss | Episodes / fingerprints | Known-new first attempts | Reset retries | Uncertain session-first attempts |
| --- | ---: | ---: | ---: | ---: |
| celesteela | 3 / 3 | 1/2 (50.0%) | 10/12 (83.3%) | 1/1 (100.0%) |
| entei | 5 / 3 | — | 125/139 (89.9%) | 5/5 (100.0%) |
| mewtwo | 5 / 5 | 3/4 (75.0%) | 56/65 (86.2%) | 1/1 (100.0%) |
| moltres | 6 / 6 | 5/5 (100.0%) | 63/70 (90.0%) | 1/1 (100.0%) |
| raikou | 5 / 3 | — | 103/107 (96.3%) | 5/5 (100.0%) |
| suicune | 2 / 2 | — | 145/147 (98.6%) | 2/2 (100.0%) |
| tapu-bulu | 7 / 7 | 2/2 (100.0%) | 317/346 (91.6%) | 5/5 (100.0%) |
| xerneas | 11 / 11 | 3/10 (30.0%) | 24/34 (70.6%) | 1/1 (100.0%) |
| zapdos | 2 / 2 | 1/1 (100.0%) | 68/79 (86.1%) | 0/1 (0.0%) |

A reset retry is classified by the **preceding** decision, not by whether the current attempt ends with reset or continue. The current decision depends on its outcome; bucketing by that decision would bias the comparison. Retried attempts remain real trials for a route's observed win rate, but do not count as fresh route draws.

## What the data supports

- Standard ordinary-play baselines: Mewtwo 550/803 (68.5%), Guzzlord 214/252 (84.9%), Cresselia 150/183 (82.0%), and Xurkitree 112/168 (66.7%).
- Mewtwo Strong Boss is 60/70 (85.7%) versus Standard 550/803 (68.5%). That is consistent with retaining favorable routes, but is not a controlled experiment: sessions, settings and versions differ.
- Xerneas Strong Boss is 28/45 (62.2%); Standard is 1/2 (50.0%). The reset-chain table separates first attempts on known new routes from retries. Detailed per-route outcome sequences are in `routes.csv`.
- Long retained routes demonstrate achievable performance, but are selected routes, not estimates of a random new route's success probability.
- The retained-route sample is selected and stopped according to observed results. Its distribution of win ratios cannot directly estimate how often a fresh route has a true 80% or 95% success probability. Standard estimates the mean under its historical settings; repeated trials on multiple sampled routes are needed to learn the spread.

## Definition and limitations

- **Recorded win** means the program's success criterion: four captured Pokémon. The parser uses final state `wins >= 4` and checks it against the program counters. This ordinarily tracks beating and catching an eligible boss. Skipped catches or an already-caught boss can make battle victories invisible to that criterion.
- **Pheromosa is excluded from the battle-win baseline:** Standard recorded 0/85 (0.0%). On 2026-10-28 the user confirmed that the September 26–27 farming sessions were after Pheromosa had already been caught. The counter therefore could not measure actual boss-battle wins. Keep the raw recorded outcomes for audit, but do not treat them as a 0% battle-win estimate. The user has since lost that retained route.
- First-raid losses count even without a policy decision. Lobby/start failures and interrupted attempts without a completed result do not count as losses.
- Program restarts are uncertain route boundaries. Matching full type maps plus starting-rental sets are reported separately from explicit reset-chain identity. A map fingerprint is not a game seed and does not identify every unvisited encounter.
- BossFinder attempts whose boss was never identified remain unassigned. A previous session's “Last Boss” is never substituted for an unknown boss.
- The first reconstructed Max Lair attempt is 2026-09-22 17:38:19.812474. The input manifest gives the full archive time range. This covers the retained local archive, not any deleted or other-machine history. Error-report tails and copied excerpts are excluded because they duplicate main logs.

## Verification and reproduction

- All **2,981 cumulative Runs/Wins checkpoints** match reconstructed outcomes.
- All **1,039 logged Strong Boss ratio checks** match path counters and outcomes within 1e-6.
- These are offline consistency checks of real logs, not application unit tests or live validation.

```sh
python3 tools/max_lair_history.py \
  --as-of "2026-10-28 08:30:00" \
  --out pokemon_swsh/max_lair/history-2026-10-28
```

Artifacts: `attempts.csv` (one row per attempt), `routes.csv` (reset-chain episodes), `route_fingerprints.csv` (probable cross-session duplicates), `bosses.csv` (per mode), `sessions.csv`, `validation.json`, and `manifest.json` (input file snapshot hashes). Main application logs are read without modifying them. The active log is snapshotted at its initial byte length; later appended bytes are excluded from that file hash.
