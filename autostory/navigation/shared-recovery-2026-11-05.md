# Shared recovery extraction — offline checkpoint, 2026-11-05

User authorized driving the native migration without consoles, conditioned on
credible simulator/unit coverage. No consoles, saves, profiles or other agents'
active worktrees have been changed. This is an incomplete migration checkpoint,
not a claim that BDSP recovery is fixed or that the owning PRs have adopted it.

## Isolated implementation branches

| Branch | Worktree | Revision | Base / intended handoff |
|---|---|---|---|
| `nav/recovery-core-extraction` | `~/Projects/wt/nav-recovery-core` | `af21f2750` | #114 `acf85aa2411a125ea1088908e66d4eff93351f1f`; shared search and 9 registered C++ tests. |
| `frlg/shared-recovery-adapter` | `~/Projects/wt/frlg-shared-recovery` | `616d2f5f0` | #224 `51c0ce6128e5981da56b370a770874b96a1e129f` plus merged shared branch; owner handoff for FRLG adapter. |
| `bdsp/shared-recovery-tests` | `~/Projects/wt/bdsp-shared-recovery` | `9fe6f718f` | #226 `f8bdd62b9df4dbf285f93853147295e0552a0367` plus merged shared branch; tested movement adapter only. |

FRLG pure `find_player` search now delegates to `Pokemon_WallRecovery.h` through
an adapter. Its movement, trainer-world alternatives, route-back checks, timing,
connection slack and route executor are unchanged. The shared search preserves
hold count priority, total held-tile ordering and directional tie order. Empty
input safely has no result. Existing shared registration/CMake additions are
part of the #114 owner patch; these are not direct pushes over owner branches.

BDSP's adapter uses its real `StoryWorld`/`slide_to_wall`/`step` implementation,
rejecting people as anchors, doors/unknown map edges, cross-map rays and ledge
hops. It is not yet wired into production interrupted-leg execution. It uses
native #226 map bytes; no terrain or Simulation publishing pins were changed.

## Validation actually executed

- C++23 Clang `-Wall -Wextra -Werror -fsyntax-only` passes for the new shared
  native tests, FRLG adapter and BDSP adapter/tests.
- Nine shared native UnitTestDatabase tests pass with real Common logger,
  cancellable scope, exceptions and lifetime machinery; no fake test framework.
- FRLG selected native runner: **57 passed, 0 failed** (48 existing map/route/
  re-anchor/recovery tests plus 9 shared tests). Compiled the real production
  map decoder, route planner and recovery code, existing native tests and all
  53 map JSONs embedded as C++ strings, matching the build-time resource contract.
- Rebuilt that runner with the original #224 `OverworldReanchor.cpp`: **57
  passed, 0 failed**. Result logs compare identical. This compares registered
  test outcomes; it is not a claim of exhaustive native trace equality.
- BDSP selected native runner: **17 passed, 0 failed** (9 shared tests plus
  route-planner/world suites, including the two new source-grounded checks).
  Uses the actual #226 map decoder and 22 native map JSONs.
- Current `agents` code health against each intended base: **0 failures**.
  CRLF source checked with `git -c core.whitespace=cr-at-eol diff --check`.

Temporary reproducible build sources/recipes and logs remain on this machine:

- `/tmp/nav-recovery-native-tests.cpp`, `/tmp/nav-recovery-native-tests`.
- `/tmp/frlg-recovery-native-tests.cpp`, `/tmp/frlg-recovery-native-maps.cpp`,
  `/tmp/build-native-recovery.py` (argument `baseline` builds the original).
- `/tmp/frlg-reanchor-baseline.cpp`: original source read from #224 Git object.
- `/tmp/frlg-recovery-native-results.log`, `/tmp/frlg-recovery-baseline-results.log`.
- `/tmp/bdsp-recovery-native-tests.cpp`, `/tmp/bdsp-recovery-native-maps.cpp`,
  `/tmp/build-bdsp-recovery.py`, `/tmp/bdsp-recovery-native-results.log`.

These temporary runners call existing registered native test functions directly
and avoid app/UI initialization. They do not substitute model implementations
or parse C++ source text into tests. They do not run OverworldSim, full AutoStory,
image detectors or controller I/O; those coverage gaps remain open.

## CI and publishing blocker

Untouched #224 baseline mirrored to `nav/recovery-frlg-baseline` (no code change):

- [GCC baseline run 42010288031](https://github.com/dayfine/Arduino-Source/actions/runs/42010288031)
  failed before tests in existing `PokemonFRLG_AutoStoryBattleData.cpp`, lines
  283 and 305, with `-Werror=dangling-reference`. This is not the recovery patch.
- [Clang baseline run 42010965658](https://github.com/dayfine/Arduino-Source/actions/runs/42010965658)
  was still compiling when this checkpoint was written. Inspect its eventual
  result; do not call full FRLG baseline CI green based on selected local tests.

First shared-branch push was refused. The hook has no PR from which to discover
the new branch's intended base, so falls back to `origin/main` and reports three
inherited `USE_QT_UI` declarations in CommandLine_Main.cpp, Globals.h and Main.cpp.
The actual extraction passes against #114. No hook or code-health bypass has
been performed. A narrow one-time `--no-verify` bootstrap push approval was
requested through the chat's pending question, citing `tools/hooks/pre-push`'s
explicit user-approval requirement. Approval has not arrived at this checkpoint.
Do not infer approval from elapsed time or the recommended/preselected option.
After authorized publishing, create only a fork draft with base `nav/shared`,
then normal hook pushes can discover the correct base. Honor owning-PR handoffs.

## Critical fidelity findings and next steps

1. BDSP `GridSimRouteIO::walk_taps` and `walk_to_wall` currently always report
   `wild_battles=0`. Passing grid/story simulations therefore cannot validate
   the reported battle-interrupted mine leg. Add deterministic encounter
   outcomes first: movement completed versus not completed, delayed detection,
   queued input and repeat interruptions. Separate actual simulated movement
   from the walker-reported credit; do not assume a screenshot proves the cause.
2. `StoryWalkSteps::tap_legs` presently advances the pose using intended tile
   counts after accepting `moved_chunks`; `RouteWalker::open_leg` explicitly
   assumes wild-battle tap counts remain exact. They need uncertain outcomes
   and recovery before the next leg, using the shared candidate search.
3. BDSP `RouteWalker::wall_leg` retries an encountered wall leg up to three
   times, but its `if (interrupted) return NEXT_LEG` can still advance after
   repeated encounters exhaust retries. `on_to_wall` similarly does not inspect
   the final walk outcome. Add red scripted native regressions before fixing.
4. AutoStory's simpler `walk_until_stopped` helper accepts LIMIT as wall arrival.
   Do not use it blindly for new localization. Require proven convergence plus
   appropriate observed stop/verification or fail explicitly.
5. Test complete mine inbound and outbound paths, r55, house transitions and
   unconfirmed fades through the real consumers. Re-run FRLG native recovery
   and simulation suites after any shared behavioral change. Current new mine
   test proves only that column-15 rows 15/16/17 have a common south wall endpoint.
6. Preserve the later #215/#219 behavior when handing the FRLG patch to owners.
   Do not merge the aggregate game stack into #114; core stays game-independent.

No deployment, live validation or successful full-game replay is claimed.
