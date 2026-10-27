# BBQ PR progress

Live status of [PR_PLAN.md](PR_PLAN.md). Update whenever a PR changes state. Risk letters
(L/M/H) and what each can regress are in PR_PLAN.md; open problems are in
[KNOWN_ISSUES.md](KNOWN_ISSUES.md).

Legend: ✅ merged upstream · 🔵 open upstream · 🟢 fork draft, built and tested ·
🟡 fork draft, needs build/tests · 🛠 branch in progress · ⬜ not started · ❌ closed

Last updated: 2026-10-26 23:00.

```
upstream/main
├── ✅ U#1636 Tera battle only after confirmed entry
├── 🔵 U#1631 quest identity → Options module (L) ........ approved by kichithewolf
│   └── 🟡 B1 #2 BBQ log lines (L) ......................... rebased locally onto 6c312f238, not pushed
│       └── 🟡 B2 #3 snapshots (L) ......................... rebased locally, not pushed
├── 🔵 U#1628 BP reader crop (L) .......................... CI green after CommandLineTests#36 merged
│   ├── 🟡 #7 quest detector tests (L) .................... rebased locally; README note → CommandLineTests follow-up
│   └── 🟡 B3 #16 BP status (L) ........................... rebased locally, not pushed
│
├── P prep moves
│   ├── 🛠 P1 BBQ tests into PokemonSV_BBQTests.cpp (L) ... branch bbq/tests-file b9d9a10e0, build pending
│   ├── ⬜ P2 spot routes → BlueberryRoutes (L)
│   └── ⬜ P3 BlueberryCatchPhoto split (L)
│
├── A L1 cores
│   ├── ⬜ A1 MapDestinationMenu snow fix (M) ............. needs day/night/snow menu samples in CommandLineTests
│   ├── 🛠 A2 error report without stream (L) ............. branch fix/error-report-without-stream d65c9f427, build pending
│   ├── ⬜ A3 SpeciesTypes table (L)
│   ├── ⬜ A4 PlaceNameBannerReader (L)
│   ├── ⬜ A5 PrideRockDetector (L)
│   ├── ⬜ A6 TeraGlowDetector (L)
│   └── ⬜ A7 OverworldControls (M) ........................ blocked: lock-on-first rework
│
├── C navigation ........................................... ⬜ C1 · 🟡 C2a/C2b #24 · 🟡 C3 #30 · ⬜ C4 · ⬜ C5a-c
├── D battle & catch ....................................... ⬜ D1 · ⬜ D2 · ⬜ D3 · 🟡 D4 #23
├── E Auto Battle .......................................... 🟡 #15 → #17 → #18 → #21 → #31 → #32 · ⬜ E7
├── F controller ........................................... 🟡 #37 → #5 #4 #6 · 🟡 #38 → #8 → #27 #25 #10 → #12 → #13 → #22 → #26 · ⬜ F14 F15
├── G routes ............................................... ⬜ G1–G5 · 🟡 G6 #35 · 🟡 G7 #33 · 🟡 G8 #29
├── Z docs ................................................. ⬜ Z1 · ⬜ Z2
│
└── ❌ closed as superseded: #9 #14 #20 #28 #34 #36
```

## Working set rules (user, 2026-10-26)

- **Upstream: exactly two open at a time.** When one merges, the user publishes the next from the
  queue below the same day.
- **Working set: about 10 fork drafts** that are being built, tested or fixed. When it drops under
  6, start the next batch of 8–10 from the plan, lowest risk first.
- A PR leaves the working set when it goes upstream or is closed.

## Upstream queue (publish in this order)

| # | PR | Risk | State |
|---|---|---|---|
| — | U#1631 quest identity | L | open upstream, approved |
| — | U#1628 BP reader crop | L | open upstream, CI green |
| 1 | A2 error report without stream | L | 🛠 build pending |
| 2 | P1 BBQ tests file | L | 🛠 build pending |
| 3 | #7 quest detector tests (after U#1628) | L | 🟡 rebased |
| 4 | B3 #16 BP status (after U#1628) | L | 🟡 rebased |
| 5 | B1 #2 logging (after U#1631; decision 4) | L | 🟡 rebased |
| 6 | D2 BasicCatcher options | L | ⬜ |
| 7 | A6 TeraGlowDetector | L | ⬜ |
| 8 | P2 spot routes move | L | ⬜ |
| 9 | P3 CatchPhoto split | L | ⬜ |
| 10 | B2 #3 snapshots | L | 🟡 rebased |

## Current working set (10)

A2, P1, #2, #3, #7, #16, D2, A6, P2, P3.

## Next up (low risk first)

1. Build and test P1 and A2 once the running farmer test ends; open them as fork drafts.
2. Push the four local rebases (#2, #3, #7, #16) after their build and tests.
3. A3–A6: detectors and the species table, each with its tests; reference images go to
   CommandLineTests, not this repo.
4. P2 and P3 moves.
