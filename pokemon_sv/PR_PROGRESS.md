# BBQ PR progress

Live status of [PR_PLAN.md](PR_PLAN.md). Update whenever a PR changes state. Risk letters
(L/M/H) and what each can regress are in PR_PLAN.md; open problems are in
[KNOWN_ISSUES.md](KNOWN_ISSUES.md).

Legend: ✅ merged upstream · 🔵 open upstream · 🟢 fork draft, built and tested ·
🟡 fork draft, needs build/tests · 🛠 branch in progress · ⬜ not started · ❌ closed

Last updated: 2026-10-26 23:10. Fork `main` synced with upstream `main` (4e017f888).

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
│   ├── 🟢 P1 #51 BBQ tests into PokemonSV_BBQTests.cpp (L) ... b9d9a10e0, 9/9 tests
│   ├── ⬜ P2 spot routes → BlueberryRoutes (L)
│   └── ⬜ P3 BlueberryCatchPhoto split (L)
│
├── A L1 cores
│   ├── ⬜ A1 MapDestinationMenu snow fix (M) ............. needs day/night/snow menu samples in CommandLineTests
│   ├── 🟢 A2 #50 error report without stream (L) ......... d65c9f427, built, code health 0 failures
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

## Upstream queue (publish in this order, by stage; see PR_PLAN.md "Stages")

| # | Stage | PR | Risk | State |
|---|---|---|---|---|
| — | 0 | U#1628 BP reader crop | L | open upstream, CI green |
| — | 1 | U#1631 quest identity | L | open upstream, approved |
| 1 | 0 | A2 #50 error report without stream | L | 🟢 ready |
| 2 | 0 | P1 #51 BBQ tests file | L | 🟢 ready |
| 3 | 0 | #7 quest detector tests (after U#1628) | L | 🟡 rebased, testing |
| 4 | 0 | B3 #16 BP status (after U#1628) | L | 🟡 rebased, testing |
| 5 | 1 | #37 BBQGameActions seam | M | 🟡 needs rebase onto P1 |
| 6 | 1 | #38 quest planner | M | 🟡 needs rebase onto P1 |
| 7 | 1 | #8 verified reroll (fold in-panel reroll) | M | 🟡 needs rework |
| 8 | 1 | #6 persistent retries | M | 🟡 needs rebase |
| 9 | 1 | #10 out-of-retries action (split first) | H | 🟡 needs split |
| 10 | 1 | #25 reroll impossible quests on sight | M | 🟡 needs rebase |
| 11 | 1 | #4 save milestones, #5 endless-reroll guard | L | 🟡 needs rebase |
| 12 | 1 | B1 #2 logging, B2 #3 snapshots | L | 🟡 rebased, testing |

## Current working set (10)

Stage 0: A2 #50, P1 #51, #7, #16. Stage 1: #37, #38, #8, #6, #10, #25.
Deferred to their stage: #2/#3 (stage 1 tail), D2, A6, P2, P3 (stage 5).

## Next up (low risk first)

2. Push the four local rebases (#2, #3, #7, #16) after their build and tests.
3. Stage 1: rebase #37 and #38 onto P1, then their children; split #10.
4. After stage 1 is queued: stage 2 (#24 split, A4) and stage 3 (Auto Battle chain).
