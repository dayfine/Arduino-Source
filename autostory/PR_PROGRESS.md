# AutoStory PR progress

Live status of [DEMO_PLAN.md](DEMO_PLAN.md) (the tree and order; [PR_PLAN.md](PR_PLAN.md) keeps the
file-ownership rules). Update whenever a PR changes state.

Legend: ✅ merged upstream · 🔵 open upstream · 🟢 fork draft, builds and tests pass · 🟡 fork draft,
CI pending, red, or needs a merge · 🧪 console-verified · ⬜ not started · ❌ closed

Last updated: 2026-11-02 ~15:10 PT (FRLG #120 gained the save-overwrite fix, start-save check,
rival preset name and absolute options via OptionMenuReader; console B 14:33–14:38, build frlg-m34:
new game 00.01 → 00.04 first try, stopped for Starter RNG at Squirtle's ball; new drafts #177–#181).

## Upstream (two open at a time; the user publishes)

| Upstream | Fork source | State | Next |
|---|---|---|---|
| [#1828](https://github.com/PokemonAutomation/Arduino-Source/pull/1828) AutoStory: shared start/end select database and story index | `autostory/story-select` 101f5744f (= #43 + #44, plus review fixes) | 🔵 jw098: "looks fine", sections answered; checks pass, mergeable | waiting for merge |
| [#1754](https://github.com/PokemonAutomation/Arduino-Source/pull/1754) PokemonSV: BBQ tests in their own file | #51 `bbq/tests-file` 9e681ce09 | 🔵 conflict fixed (upstream main merged in), mergeable; CI awaits maintainer approval | BBQ queue: ../pokemon_sv/PR_PLAN.md |
| (queued) Max Lair: test the current boss inference | #171 → then #172 the fix | 🟢 CI green, 3 platforms | when a slot frees: fold into one commit, user publishes |
| (queued) BBQ quest detector tests (redo of closed #1759, tests in the detector's own file like #1771) | #180 `sv/bbq-quest-detector-tests` | 🟡 16/16 via harness; CI running | when a slot frees; user posts |
| (candidate) FRLG: Ten Lines target search in the program | #181 `frlg/rng-target-search` | 🟡 reproduces all 50 rows of the user's LeafGreen artifact; CI running | **user decides:** Ten Lines is GPL; code written fresh from reading it |
| (queued) BDSP: MenuDetector past NEW tags; ShortDialogDetector long first line | #152, upstream copy `bdsp/detectors-upstream` dc884fdac | 🟡 CI red only for 26 missing images | after [CLT #42](https://github.com/PokemonAutomation/CommandLineTests/pull/42) merges (adds the images) |
| [CommandLineTests #42](https://github.com/PokemonAutomation/CommandLineTests/pull/42) BDSP AutoStory intro screens (16 images) | CLT `bdsp/autostory-intro-screens` 68604b1 | 🔵 open, no reviews | — |

Recently merged upstream: #1810 (MoveOnlyFunction), #1791, #1798, #1771. Closed: #1767, #1759 (→ #180).

## Fork tree

```
main (= upstream main, synced daily 07:00)
├─ 🟢 #43 autostory/r1-select-db ─ #44 r2-story-index (+ #1828's review version merged in)
│  └─ 🟢 #45 r3-shared-stats
│     └─ 🟢 #114 nav/shared  (grid, planner, LegWalker, route log, JSON maps, ScreenMachine)
│        └─ 🟢 #122 nav/story-steps  (shared story steps, retry policy, run options)
│           ├─ FRLG
│           │  └─ #117 frlg/mvp (maps + planner)
│           │     └─ #118 frlg/walk-wild-legs (walker; MoveSpeed RUN, [Speed] log)
│           │        └─ #119 frlg/walk-interrupt-recount (position recovery)
│           │           └─ 🧪 #120 frlg/autostory-mvp  new game → Brock → Running Shoes
│           │              │  00.01–00.11 run before; 11-02: 00.01–00.04 first try (frlg-m34).
│           │              │  CI red only for CLT image ItemListReader/ItemsPocket_Potion5_…png
│           │              └─ #177 frlg/autostory-player-name (typed player name)
│           │                 └─ #178 frlg/autostory-sid-helper (SID Helper handoff)
│           │                    └─ #179 frlg/autostory-starter-rng (Starter RNG handoff)
│           └─ BDSP
│              └─ #134 bdsp/maps-v4 → #127 nav-v4 → #129 walk-v4 → #152 detectors-v4
│                 → #148 actions-v4 → 🧪 #128 autostory-v4 (new game → Floaroma, r85)
│                    ├─ #175 autostory-v4-seg01 (01.05–01.10 on the maps; not on console)
│                    │  └─ 🟡 #173 shared-steps (named events → shared steps, to 01.03; CI: sim test crash)
│                    ├─ #167 sim (grid simulation for unit tests)
│                    └─ #168 view-shift (tap counts need the view to scroll; console check due)
├─ #171 swsh/maxlair-boss-tests → #172 maxlair-boss-fix        🟢 CI green
├─ #180 sv/bbq-quest-detector-tests · #181 frlg/rng-target-search   (upstream candidates)
├─ #169 fix/fatal-exception-message (FatalProgramException::message empty) — upstream candidate
├─ #61 frlg/party-room-check · #81 frlg/naming-screen · #62 switch/system-keyboard
├─ #125 devtools/route-pictures · #132 common/run-records · #60 docs/lab-build-disk
├─ common/move-only-function (#1810 merged upstream as another commit) → #87–#91 migration stack
├─ #39 prototype/sv-isolated-hunt
└─ BBQ: #51 → (#37 → #38 → #53 → #8 → …), #24/#30, #29, #33 — see ../pokemon_sv/PR_PLAN.md
base/upstream-main
└─ 🟢 #107 lab/tooling (controller-ready guard; controller=ready on console B) → #131 design/agent-integration
lab/main (#103, lab builds only) → #149 run-records-base → #116 · #150 run-records (both 24 behind lab/main, conflict; not in lab builds)
design/autostory (#100) → #130 FRLG events after Brock (options reference; team plan per starter ×
  version: FR Lapras, LG Starmie via Super Rod) · #174 sections, postgame and DLC
agents → #85 lab navigation notes · #113 this plan · #176 FRLG demo plan (SID Helper, Starter RNG)
```

## Waiting on the user

- Upstream order once a slot frees: #171 (Max Lair), #180 (BBQ detector tests), the detectors PR
  (after CLT #42).
- #181 upstream or not (Ten Lines is GPL; ours is written fresh from reading it). #169 upstream or not.

## Known gaps

- Fork drafts skip CI; the stack's CI runs only when dispatched.
- #120 → #179: add `PokemonFRLG/ItemListReader/ItemsPocket_Potion5_Antidote3_ParlyzHeal1.png` to CLT
  (only CI failure). BATTLE SCENE OFF not yet run on console.
- #173: Ubuntu CI aborts in "Simulation: a blackout in 00.04b's training resets to its save" (empty
  `optional<TilePos>` dereferenced), besides the expected missing BDSP images.
- #87–#91 still sit on `common/move-only-function`, which upstream merged as a different commit (#1810).
- #7 `bbq/quest-detector-tests` is the closed #1759's branch, superseded by #180.
- BBQ children of #51 (#2, #7, #15, #19, #23, #37) are 54–63 commits behind it after #1754's merge.
- #131 is 1 behind #107; #149/#150 lag lab/main by 24 and conflict; left until lab builds need them.
- FRLG open items: preferred move set + post-battle learn-prompt loop (#120), wall-tap check
  log-only (#119), walker recovery after wild battles.

## Tips (2026-11-02 ~15:10 PT; CI = last dispatched run at this tip)

| PR | Branch | Tip | CI |
|---|---|---|---|
| #43 | autostory/r1-select-db | 8698f8c23 | green |
| #44 | autostory/r2-story-index | 9ca64aa0c | green |
| #45 | autostory/r3-shared-stats | cee0b139b | — |
| #114 | nav/shared | 7cdb95bc8 | Mac green, Windows running |
| #122 | nav/story-steps | 371ac704d | — |
| #117 | frlg/mvp | fc901ecb4 | — |
| #118 | frlg/walk-wild-legs | b2e7d13dd | — |
| #119 | frlg/walk-interrupt-recount | f89a7bb21 | — |
| #120 | frlg/autostory-mvp | fff4697e0 | red: 1 missing CLT image |
| #177 | frlg/autostory-player-name | 701564a50 | — |
| #178 | frlg/autostory-sid-helper | 53eb31ade | — |
| #179 | frlg/autostory-starter-rng | f1c2f6006 | red: same CLT image |
| #134 | bdsp/maps-v4 | eb0df8510 | — |
| #127 | bdsp/nav-v4 | 71ad2b2fc | — |
| #129 | bdsp/walk-v4 | ace8659ca | — |
| #152 | bdsp/detectors-v4 | 0d4d23e23 | — |
| #148 | bdsp/actions-v4 | 1dd00bc3e | green |
| #128 | bdsp/autostory-v4 | 3a89c929e | green |
| #175 | bdsp/autostory-v4-seg01 | 098384a98 | green |
| #167 | bdsp/sim | 5ddac8976 | green |
| #168 | bdsp/view-shift | 7291808d2 | green |
| #173 | bdsp/shared-steps | 000326b46 | red: sim test abort + images |
| #171 | swsh/maxlair-boss-tests | e819c821e | green |
| #172 | swsh/maxlair-boss-fix | 66f83197b | green |
| #180 | sv/bbq-quest-detector-tests | bab44cdc6 | running |
| #181 | frlg/rng-target-search | 9803ace69 | running (Code Health green) |
| #61 | frlg/party-room-check | a5e50d152 | green |
| #107 | lab/tooling | 2b3eac8c6 | green 3/3 |
| #130 | docs/frlg-future-events | 013ba413c | — |
| #51 | bbq/tests-file | 9e681ce09 | upstream: awaits approval |

When #1828 gets another review round: merge `autostory/story-select` into #44 again and cascade the
same order (#45 → #114 → #122 → FRLG #117–#120 → #177–#179 and BDSP #134 → … → #173). The conflicts
live in #44 (`Pokemon_AutoStorySelect.h`, `SourceFiles.cmake`) and #114
(`ComputerPrograms_UnitTestRunner.cpp`).
