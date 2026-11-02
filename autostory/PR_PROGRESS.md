# AutoStory PR progress

Live status of [DEMO_PLAN.md](DEMO_PLAN.md) (the tree and order; [PR_PLAN.md](PR_PLAN.md) keeps the
file-ownership rules). Update whenever a PR changes state.

Legend: ✅ merged upstream · 🔵 open upstream · 🟢 fork draft, builds and tests pass · 🟡 fork draft,
CI pending, red, or needs a merge · 🧪 console-verified · ⬜ not started · ❌ closed

Last updated: 2026-11-02 (fork stack merged onto upstream #1828's reviewed version, 18 branches,
merges only, builds pass at #120/#167/#168/#173; seg01 got PR #175; #170 and #165 closed as superseded;
FRLG running shoes 00.10/00.11 in #118–#120, 00.11 run on console B at ~135 ms/tile).

## Upstream (two open at a time; the user publishes)

| Upstream | Fork source | State | Next |
|---|---|---|---|
| [#1828](https://github.com/PokemonAutomation/Arduino-Source/pull/1828) AutoStory: shared start/end select database and story index | `autostory/story-select` (= #43 + #44, plus review fixes) | 🔵 in review (jw098, sections/DLC answered 2026-11-02), CI green | — |
| [#1754](https://github.com/PokemonAutomation/Arduino-Source/pull/1754) PokemonSV: BBQ tests in their own file | #51 `bbq/tests-file` | 🔵 open | BBQ queue: ../pokemon_sv/PR_PLAN.md |
| (queued) Max Lair: test the current boss inference | #171 → then #172 the fix | 🟢 CI green, 3 platforms | when #1828 merges: fold into one commit, user publishes |
| (queued) BDSP: MenuDetector past NEW tags; ShortDialogDetector long first line | #152, upstream copy `bdsp/detectors-upstream` dc884fdac | 🟡 CI red only for 26 missing images | after [CLT #42](https://github.com/PokemonAutomation/CommandLineTests/pull/42) merges (adds the images) |
| [CommandLineTests #42](https://github.com/PokemonAutomation/CommandLineTests/pull/42) BDSP AutoStory intro screens (16 images) | CLT `bdsp/autostory-intro-screens` 68604b1 | 🔵 open, cc Developer-Butters | — |

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
│           │                 00.01–00.09 run on console B (00.09 with CHECKS=fast; Brock's
│           │                 speech by hand); 00.10 shoes by hand; 00.11 run 1/1
│           └─ BDSP
│              └─ #134 bdsp/maps-v4 → #127 nav-v4 → #129 walk-v4 → #152 detectors-v4
│                 → #148 actions-v4 → 🧪 #128 autostory-v4 (new game → Floaroma, r85)
│                    ├─ #175 autostory-v4-seg01 (01.05–01.10 on the maps; not on console)
│                    │  └─ #173 shared-steps (named events → shared steps, to 01.03)
│                    ├─ #167 sim (grid simulation for unit tests)
│                    └─ #168 view-shift (tap counts need the view to scroll; console check due)
├─ #171 swsh/maxlair-boss-tests → #172 maxlair-boss-fix        🟢 CI green
├─ #169 fix/fatal-exception-message (FatalProgramException::message empty) — upstream candidate
├─ #61 frlg/party-room-check · #81 frlg/naming-screen · #62 switch/system-keyboard
├─ #125 devtools/route-pictures · #132 common/run-records · #60 docs/lab-build-disk
├─ #87–#91 common/move-only-* (MoveOnlyFunction migration stack)
├─ #39 prototype/sv-isolated-hunt
└─ BBQ: #51 → (#37 → #38 → #53 → #8 → …), #24/#30, #29, #33 — see ../pokemon_sv/PR_PLAN.md
base/upstream-main
└─ #107 lab/tooling → #131 design/agent-integration
lab/main (#103, lab builds only) → #149 run-records-base → #116 · #150 run-records (both 24 behind lab/main, conflict; not in lab builds)
design/autostory (#100) → #130 FRLG events after Brock · #174 sections, postgame and DLC
agents → #85 lab navigation notes · #113 this plan · #176 LeafGreen WR video notes
```

## Waiting on the user

- Upstream order once a slot frees: #171 (after #1828) and the detectors PR (after CLT #42).
- #169 upstream or not.

## Known gaps

- Fork drafts skip CI; the stack's CI runs only when dispatched. Last dispatched green: #171,
  #172, `autostory/story-select`.
- #149/#150 (lab run records) lag lab/main by 24 commits and conflict; left until lab builds need them.
- FRLG open items: preferred move set + post-battle learn-prompt loop (#120), wall-tap check
  log-only (#119), walker recovery after wild battles.

## Tips (2026-11-02, after the #1828 merge)

| PR | Branch | Tip |
|---|---|---|
| #44 | autostory/r2-story-index | 9ca64aa0c |
| #45 | autostory/r3-shared-stats | cee0b139b |
| #114 | nav/shared | 7cdb95bc8 |
| #122 | nav/story-steps | 371ac704d |
| #117 | frlg/mvp | fc901ecb4 |
| #118 | frlg/walk-wild-legs | b2e7d13dd |
| #119 | frlg/walk-interrupt-recount | f89a7bb21 |
| #120 | frlg/autostory-mvp | d621f7db1 |
| #134 | bdsp/maps-v4 | eb0df8510 |
| #127 | bdsp/nav-v4 | 71ad2b2fc |
| #129 | bdsp/walk-v4 | ace8659ca |
| #152 | bdsp/detectors-v4 | 0d4d23e23 |
| #148 | bdsp/actions-v4 | 1dd00bc3e |
| #128 | bdsp/autostory-v4 | 3a89c929e |
| #175 | bdsp/autostory-v4-seg01 | 098384a98 |
| #167 | bdsp/sim | 5ddac8976 |
| #168 | bdsp/view-shift | 7291808d2 |
| #173 | bdsp/shared-steps | 000326b46 |
| #61 | frlg/party-room-check | a5e50d152 |

When #1828 gets another review round: merge `autostory/story-select` into #44 again and cascade the
same order (#45 → #114 → #122 → FRLG #117–#120 and BDSP #134 → … → #173). The conflicts live in
#44 (`Pokemon_AutoStorySelect.h`, `SourceFiles.cmake`) and #114 (`ComputerPrograms_UnitTestRunner.cpp`).
