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
├── nav/shared ............ collector 1, supersedes #72 #66 #108 #79
│     S1 Common: MoveOnlyFunction  ✅ merged upstream as #1810 (2026-11-01); nav/shared
│                                  merges the base back in, so S1 drops out of the diff
│     S2 Pokemon navigation: grid types + leg planner    (#66,  +733,  7 files)
│     S3 ScreenMachine + StepTrace                       (#108, +333,  5 files)
│     S4 Pokemon navigation: story checkpoints as events (#79,  ~+1200, 6 files; its
│                                                         MoveOnlyFunction is upstream now)
├── frlg/mvp = fork #117 (on nav/shared) ... collector 3, supersedes #67; 6 commits, +3304, CI green
│     F0 BagDetector fix: dropped, already upstream (#1771)
│     F1 cc3450b8a devtools: generate FRLG overworld maps as JSON from pokefirered   (+203)
│     F2 e770e3fa7 FRLG: overworld maps as plain data (tiles, grid, named points, flags) (+978)
│     F3 d0bb032e7 FRLG: plan overworld routes with the shared leg planner           (+835)
│     F4 b734805ce Pokemon navigation: leg walker with game-supplied movers/interrupts (+465)
│     F5 6952e6c5a FRLG: read which way the standing player faces                    (+314)
│     F6 ab9a22e0b FRLG: walk overworld routes with the leg walker (+ BattleFlashWatcher) (+509)
│     (facing reader before the walk: the walk includes it. 19 to_string → std::format)
│     └── #118 frlg/walk-wild-legs (on frlg/mvp; wild legs 43ac578a0, signs f11a56672; CI green) ... fixes from the console B MVP run, to collect
│           43ac578a0 watch for wild battles only on steps where one can start (grass/water)
│           f11a56672 never plan a hold into a sign moving north or south (maps get 'S' signs)
└── base/bdsp-1717 = base/upstream-main + upstream #1717 head 381c055ac + nav/shared
      └── bdsp/mvp ............ collector 2, supersedes #68 #109 #110 #111 #112 and the lab
            branch bdsp/event-sandgem-center; kit ready, waits for #111/#112 to merge (phase 3)
            B1 Pokemon navigation: TileGrid + MapSet (+859; from the lab branch, ~700 lines
               newer than #109; MapSet written LF; no tests here, they use BDSP data → B5)
            B2 BDSP AutoStory: walker (Walk, RouteWalk + costs, Location banner, PokemonCenter,
               NameListDetector, Menu/SelectionArrow tweaks) (+2,289)
            B3 BDSP AutoStory: map data from the Diamond decomp (StoryMaps, RouteRecords, 15 maps,
               MAPS.md) (+2,893)
            B4 BDSP AutoStory: segments 00/01, Sandgem Center events, NewGame (+ SystemKeyboard-
               Detector), AutoStory/Tools merged with Butters' newer #1717 head (~+9k/−6.5k):
               our Segment 01 (Jubilife→Floaroma) replaces his 01 (user, 2026-11-01); his
               PP budget kept; GCC fixes; DIAGNOSTIC options kept for run 3
            B5 BDSP AutoStory: tests (18: TileGrid parse/wall/plans, records, events, screenshots
               skip when absent) (+2,088)
            Rehearsal on the current tip: 5 commits, 71 files, +15,386/−4,189; Windows clang
            builds and passes our tests; the only failures are #1717's two screenshot tests
            (Command File Runner and lab control stay in #107)
```

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
