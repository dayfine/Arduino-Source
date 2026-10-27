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
│   └── 🟢 B1 #2 BBQ log lines (L) ......................... c8ba680b3 on 6c312f238, 14/14
│       └── 🟢 B2 #3 snapshots (L) ......................... e1d2a2de5, 16/16
├── 🔵 U#1628 BP reader crop (L) .......................... CI green after CommandLineTests#36 merged
│   ├── 🟢 #7 quest detector tests (L) .................... a58ecebd8, 29/29; README note → CommandLineTests follow-up
│   └── 🟢 B3 #16 BP status (L) ........................... 57ea3f413, 19/19
│       └── 🟢 #52 BP check at run end (L) ................ 7f38c7189, 24/24
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
| 3 | 0 | #7 quest detector tests (after U#1628) | L | 🟢 a58ecebd8, 29/29 |
| 4 | 0 | B3 #16 BP status (after U#1628) | L | 🟢 57ea3f413, 19/19; to do: fork code health flags `program()` 111→129 and `read_quests` 54 lines |
| 4b | 0 | #52 BP check: rerolls vs reads at run end (on #16) | L | 🟢 7f38c7189, 24/24 |
| 5 | 1 | #37 BBQGameActions seam | M | 🟢 21b53eda2, 15 |
| 6 | 1 | #38 quest planner | M | 🟢 f4b2be7d5, 24 |
| 6b | 1 | #53 pin today's stuck behaviors (test-only) | L | 🟢 e326a90ac, 29 |
| 7 | 1 | #8 verified reroll (fold in-panel reroll) | M | 🟢 6ebb53d73, 38 |
| 8 | 1 | #6 persistent retries | M | 🟢 90cb78ef8, 40 |
| 9 | 1 | #10 out-of-retries action | H | 🟢 41b3e4fcd, 44 |
| 10 | 1 | #25 reroll impossible quests on sight | M | 🟢 8ee930626, 61 |
| 11 | 1 | #4 save milestones, #5 endless-reroll guard | L | 🟢 0bcd8071d, b4bc0f0bb, 42 |
| 12 | 1 | B1 #2 logging, B2 #3 snapshots | L | 🟢 c8ba680b3 14/14, e1d2a2de5 16/16 |

## Stage 1 stack (rebuilt 2026-10-27, linear on P1 #51)

#37 21b53eda2 → #38 f4b2be7d5 → #53 e326a90ac → #8 6ebb53d73 → #6 90cb78ef8 → #5 b4bc0f0bb →
#4 0bcd8071d → #10 41b3e4fcd → #25 8ee930626 (carries closed #9's commit). Every commit builds
and passes the BBQ tests on its own (15 → 61 tests). #53 (test-only) pins five ways the round
loop stalls or wastes time today; #6, #5 and #4 each replace their scenario in place, so their
diffs show the behavior change. Fork PR bases form one chain; stage-4 PRs (#12, #13, #22,
#26, #27) still sit on the old branches and need rebasing when stage 4 starts.

**Stage runs:** S0 (`test/stage0` 23c6bdea5) and S1 (`test/stage01`), plan and results in [STAGE_RUNS.md](STAGE_RUNS.md).

**Test build for the stage 0+1 run:** branch `test/stage01` c0025b745 = stage 1 + #50 + #1628 +
#16 + #52, with the BP stats and end-of-run check re-wired into stage 1's game-actions seam (each
verified reroll now gets an exact BP window). 76 BBQ tests pass. Binary SHA-256 prefix
81b7714581a0204e (verify-wt). Built before #53 was inserted; the stack differs only in tests, so
the binary is unchanged in behavior. Not run on the Switch yet. When #16/#52 rebase onto stage 1, this
wiring is the change they take.

**#16/#52 rebased onto stage 1 (2026-10-27):** `bbq/bp-status` 78c00504d (#25 + merge of
U#1628's branch + #16, 71 tests) → `bbq/bp-check` 852b61cb0 (#52, 76 tests). They take the BP
wiring from `test/stage01`; #52 now also logs the BP check when a run stops early. Send order
is therefore #50, #51, #7, stage 1, then #16 and #52 (stage 0's BP stats land after stage 1).

## Current working set (10)

Stage 0: A2 #50, P1 #51, #7, #16, #52. Stage 1: #37, #38, #53, #8, #6, #10, #25.
Deferred to their stage: #2/#3 (stage 1 tail), D2, A6, P2, P3 (stage 5).

## Next up (low risk first)

3. Stage 1: rebase #37 and #38 onto P1, then their children; split #10.
4. After stage 1 is queued: stage 2 (#24 split, A4) and stage 3 (Auto Battle chain).
