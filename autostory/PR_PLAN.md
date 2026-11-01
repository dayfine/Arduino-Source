# AutoStory work → upstream PRs: the three collectors

Written 2026-11-01 (user decisions of that day are in "Decisions"). Live status is in
[PR_PROGRESS.md](PR_PROGRESS.md). The BBQ equivalent is
[../pokemon_sv/PR_PLAN.md](../pokemon_sv/PR_PLAN.md); the rules for where PRs live are in
[../pokemon_sv/PR_WORKFLOW.md](../pokemon_sv/PR_WORKFLOW.md).

## Goal

Three fork PRs, each in the shape of fork #107 (a "collector"): a branch on a `base/*` copy
of upstream main, whose commits are the future upstream PRs, one concern each, in the order
they would go upstream.

1. **Shared core** (`nav/shared`): what FRLG and BDSP both plan and run on.
2. **BDSP MVP** (`bdsp/mvp`): new game to Floaroma, minus the shared core, on Butters' #1717.
3. **FRLG MVP** (`frlg/mvp`): overworld navigation to Brock, minus the shared core.

## Ground rules

- **Rebuild from the existing PRs, do not merge their histories.** Each collector commit is the
  source PR's three-dot diff (`git diff base...head`) applied on the collector, so
  `git range-diff` against the source branch shows only the shared cmake and test-runner lines.
- **One commit = one future upstream PR**, about 400 changed lines or fewer where the code
  allows it; map data and tests do not count. Commit titles are the PR titles.
- **The base is upstream main**, not fork main: `base/upstream-main` is a plain copy
  (fast-forwarded, never rebased). BDSP's base adds Butters' #1717 head on top.
- **Upstream is serial**: at most two open there; the user publishes. Collectors and their
  source PRs are fork drafts. Superseded PRs are closed by the user, not by an agent.
- **CRLF files stay CRLF** (`SerialPrograms/cmake/SourceFiles.cmake`,
  `ComputerPrograms_UnitTestRunner.cpp`, most of CommonFramework). A diff the length of the
  file is a line-ending accident.
- **`std::format` for multi-part messages** (AGENTS.md C++ style). Our code had none and
  ~130 `std::to_string` chains; each collector commit converts its own sites with identical
  output text (user, 2026-11-01). Butters' #1717 code is left as is.
- **Go slowly**: each collector is shown to the user before the next one starts.

## Tree

```
base/upstream-main (copy of upstream main; refreshed to b3673f2a9 on 2026-11-01)
├── nav/shared = fork #114 (7008f8c67) ... collector 1, supersedes #72 #66 #108 #79
│     S1 Common: MoveOnlyFunction  ✅ merged upstream as #1810 (2026-11-01); nav/shared
│                                  merges the base back in, so S1 drops out of the diff
│     S2 Pokemon navigation: grid types + leg planner    (#66,  +733,  7 files)
│     S3 ScreenMachine + StepTrace                       (#108, +333,  5 files)
│     S4 Pokemon navigation: story checkpoints as events (#79,  ~+1200, 6 files)
│        + story events take a game-defined position, a segment is a table of event
│          checkpoints, one JSON map schema and loader for both games (MapData,
│          StorySegments, TilePose)
│     S5 7008f8c67 Pokemon navigation: weigh open stops after a wild step (Step::wild,
│        Leg::wild, LegCosts::per_open_wild_stop, default 0; 4 toy-grid tests). Moved
│        from #119 (b4d1976a3) on 2026-11-01
│
│  ├── frlg/mvp = fork #117 (20291b6c8, merges nav/shared 7008f8c67) ... collector 3,
│  │     supersedes #67; F1–F6 as before, +3304 over nav/shared, 21 files
│  │     F0 BagDetector fix: dropped, already upstream (#1771)
│  │     F1 cc3450b8a devtools: generate FRLG overworld maps as JSON from pokefirered   (+203)
│  │     F2 e770e3fa7 FRLG: overworld maps as plain data (tiles, grid, named points, flags) (+978)
│  │     F3 d0bb032e7 FRLG: plan overworld routes with the shared leg planner           (+835)
│  │     F4 b734805ce Pokemon navigation: leg walker with game-supplied movers/interrupts (+465)
│  │     F5 6952e6c5a FRLG: read which way the standing player faces                    (+314)
│  │     F6 ab9a22e0b FRLG: walk overworld routes with the leg walker (+ BattleFlashWatcher) (+509)
│  │  └── #118 frlg/walk-wild-legs (d1afca983) ... wild legs 43ac578a0, signs f11a56672;
│  │        7 FRLG Overworld files + frlg_maps.py, +139/−23
│  │     └── #119 frlg/walk-interrupt-recount (08829d226) ... walker fixes from the Viridian
│  │           runs: open wild legs a tile at a time, ledge stops, overworld check, warp fade
│  │           vs battle, trainer battles, re-anchoring after a stopped leg. 12 files,
│  │           +1322/−78: FRLG Overworld + LegWalker (see "Who owns which file")
│  │        └── #120 frlg/autostory-mvp ... PokemonFRLG AutoStory, 00.01–00.08 new game →
│  │              Brock. Diff is PokemonFRLG only (19 files, AutoStory + BattleScreenDetector,
│  │              NamingScreenDetector, Panels, Tests, OverworldWalk 5 lines, cmake)
│
└── base/bdsp-1717 = base/upstream-main + upstream #1717 head 381c055ac + nav/shared
      └── bdsp/event-sandgem-center (lab line) ── #111 bdsp/maps-rederive
            └── #121 bdsp/mvp-v3 (50edfb0e9) ... segments 00 and 01 as event tables, maps as
                  JSON; merges nav/shared at 5e3150565, #111 and #112. Its own 14 commits
                  change only PokemonBDSP files, SourceFiles.cmake and the new
                  devtools/maps/bdsp_maps.py. The shared files in its diff against #111
                  (LegPlanner.h, Grid, StoryEvents, MapData, StorySegments, TilePose, the
                  removed old LegWalker copy, base/upstream-main's dpp/sockets/FRLG box) are
                  nav/shared's versions arriving through that merge: they leave #121's diff
                  once #111 has nav/shared. Not restructured yet; needs nav/shared 7008f8c67
                  merged in (compiles: TileGrid.cpp and the BDSP planner tests checked with
                  -fsyntax-only)
            └── bdsp/mvp collector (B1–B5), kit as before, waits for #111/#112
```

### Who owns which file (user, 2026-11-01)

A file is changed only in the PR that added it, so a child PR shows only its own files.
Changes a child needs in a parent's file go to the parent as new commits, with tests there,
and are merged back down (merges only; no rebase, no force-push).

| Files | Owner |
| --- | --- |
| Pokemon/Navigation: Grid, LegPlanner, MapData, StoryEvents, StorySegments, TilePose, Navigation_Tests.h; CommonTools/ScreenMachine | #114 nav/shared |
| Pokemon/Navigation: LegWalker (+ tests) | #117 frlg/mvp (added in F4; only FRLG uses it) |
| Pokemon/Navigation: TileGrid, MapSet | BDSP line (#109 → bdsp collector B1) |
| PokemonFRLG Overworld, BattleFlashWatcher, frlg_maps.py | #117, changed by #118/#119 as walker fixes |
| PokemonFRLG Programs/AutoStory, BattleScreenDetector, NamingScreenDetector | #120 |

Open: #119 still changes LegWalker.{h,cpp,_Tests.cpp} (+390, all game-agnostic: tile-at-a-time
open legs, stop_on_interrupt, stop_if_blocked, settle, warps). Under the rule they go to
#117 (or to #114 if LegWalker moves there; proposal in the 2026-11-01 restack report:
keep it in #117 until BDSP adopts it, as with TileGrid/MapSet). #120's 5-line OverworldWalk
change (BattleDialogWatcher → BattleScreenWatcher) uses #120's new detector; it moves with
the detector if that goes down to #119.

Sizes from 2026-11-01. BDSP's delta over #1717's current head (381c055ac): AutoStory
+23.6k/−11.2k (StorySegments 20 files, Utils 12, Data 35 files +2.7k, Tests +2k). It will need
a finer split than B1–B5 once rebuilt; decide after the shared and FRLG collectors land.

## Order of work

1. `nav/shared`: refresh the base, apply S1–S4, fork CI on three platforms, draft PR. Show.
2. ✅ `frlg/mvp` = fork #117: from #67, regrouped as F1–F6 on `nav/shared`. Console fixes go on
   branches off it (`frlg/walk-wild-legs`) and are collected later.
3. `bdsp/mvp`: after fork #110 and #111 merge into `bdsp/event-sandgem-center` and its three
   pre-existing CI failures are fixed (`std::lround` without `<cmath>` in
   `PokemonBDSP_AutoStory_Tests.cpp`, `int-in-bool-context` in `PokemonBDSP_AutoStoryTools.cpp`,
   the `std::move_only_function` static assertion in `MoveOnlyFunction_Tests.cpp` on Windows
   clang). Refresh the #1717 mirror (#54 is behind Butters' 2026-10-30 push) and build
   `base/bdsp-1717`. Show.
4. The user closes the superseded PRs and publishes upstream from the queue in PR_PROGRESS.md.

## Where the maps live (2026-11-01)

User, 2026-11-01: "keep maps as local files now and add TODOs to move them to Packages when
ready", and "why don't we do actual json files?".

- **In the repo, as .json files, built into the program.** The program and its tests read
  one copy; a deploy needs no map files.
  - `SerialPrograms/cmake/EmbedTextFiles.cmake` (`pa_embed_text_files()`, #114 nav/shared
    b426f749f, self-test on `Pokemon/Navigation/TestMaps/`) writes a C++ file into the build
    folder returning each `<folder>/*.json`'s name and text, split into literals of at most
    16000 bytes for MSVC; nothing generated is committed. Each game calls it once in
    `SerialPrograms/CMakeLists.txt`.
  - The generator writes the folder and checks it: `--out <folder> [--check]`.
  - FRLG: `--check` in #117 (a31833dd4). `Overworld/Maps/*.json` (25) and the CMake call in
    #118 (910c460df); `overworld_maps()` reads `overworld_map_json()` in #118 (42d7e0aca),
    the first PR whose generator writes sign tiles. #117 still loads from Packages, so its
    route tests stay skipped in CI until the #118 → #117 fold is decided.
  - BDSP: the same helper through BDSP's nav/shared merge; folder and placement by the BDSP
    agent.
- **Packages later.** TODOs at `overworld_map_json()` and in `frlg_maps.py`. A local,
  unpushed Packages branch `autostory/maps` (worktree
  `/Users/difan/.claude/jobs/bea871c7/tmp/wt-maps-packages`, 25 FRLG + 16 BDSP maps with
  READMEs) is kept for that move. Plain `Resources/` files are not versioned
  (`ResourceDownloadList.json`/`version.json` cover only the downloaded zips), so no version
  bump. When the maps move, the repo copy should stay the single source the Packages files
  are copied from (`--check` both), or the repo copy goes and the tests read Packages, which
  CI checks out.

## Decisions (user, 2026-11-01)

- #72 MoveOnlyFunction stays and is the shared collector's first commit (#79 keeps using it).
- #108 ScreenMachine/StepTrace is in the shared collector (BDSP uses it now, FRLG later).
- TileGrid/MapSet (#109) go into the BDSP collector, not the shared core, until FRLG adopts them.
- New collector PRs; the old PRs are closed as superseded (branches kept), as #107 did for
  #105/#106.

## Open items

- `Pokemon_LegPlanner.h` on the BDSP lab branch is behind nav/core's "search steps" refactor
  (79+/133−); the BDSP collector takes nav/core's version and re-checks the BDSP planner tests.
- #109's TileGrid is ~700 lines behind the lab branch (RobustPlanCosts, route costs): B1 is
  rebuilt from the lab branch, not from #109.
- Upstream #1717 is Butters' open PR. B1–B5 cannot go upstream before it merges; the
  alternative is PRs against Butters' branch. Decide with the user when the BDSP collector exists.
- Butters notes for the BDSP collector (in its PR body): Segment 01 clash, his 0006, program
  options not adopted, PP budget kept, save-prompt check, two GCC errors, two screenshot tests.
  Pre-upstream: strip DIAGNOSTIC options, #1717 merges first, close #62.
- Not part of the MVPs, stay separate: #81 naming screen, #62 system keyboard, #61 party check,
  #86 bag detector tests, #107 lab tooling, #103 lab/main, #100/#78/#56 design notes, #85 notes.
