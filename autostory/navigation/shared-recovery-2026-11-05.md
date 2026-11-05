# Shared recovery — offline native implementation, 2026-11-05

User authorized driving the migration without consoles. Work used isolated
worktrees; consoles, saves, profiles and other agents' active branches were
untouched. These are fork drafts for owner handoff, not merged changes.

## Review and merge order

| Draft | Branch / tested revision | Base and scope |
|---|---|---|
| [#227](https://github.com/dayfine/Arduino-Source/pull/227) | `nav/recovery-core-extraction` @ `554624edf` | #114 `acf85aa2411a125ea1088908e66d4eff93351f1f`: game-independent convergent wall search, 9 registered native tests. |
| [#228](https://github.com/dayfine/Arduino-Source/pull/228) | `frlg/shared-recovery-adapter` @ `72714076b` | #224 `51c0ce6128e5981da56b370a770874b96a1e129f`, merged #227: replace FRLG's pure search with the shared search. |
| [#229](https://github.com/dayfine/Arduino-Source/pull/229) | `bdsp/shared-recovery-tests` @ `e287144a9` | #226 `f8bdd62b9df4dbf285f93853147295e0552a0367`, merged #227: BDSP adapter, native encounter outcomes and real story/walker recovery regressions and fixes. |

Shared extraction stays separate from BDSP behavior changes. FRLG baseline
includes #213 plus later #215/#219 behavior inherited by #224. Preserve trainer
world alternatives, movement semantics, hold-count/held-tile ordering, route-back
checks, connection slack and late-interruption handling. No terrain, map bytes,
Simulation publication pins or story route legs were changed.

The reusable search belongs with #114. FRLG adapters belong with their owners;
BDSP planner/walker and simulator/story patches belong with #127/#129 and
#167/#226 respectively. Hand these branches to owners rather than pushing over
their branches. Do not merge the aggregate FRLG stack into nav-core. No upstream
publication is authorized.

## How #226 picks this up

GitHub does not propagate stacked branch changes automatically. After owners
adopt the changes, merge parents forward (never rebase/force-push) through the
current stack:

`#114 nav/shared -> #122 nav/story-steps -> #134 bdsp/maps-v4 -> #127 bdsp/nav-v4
-> #129 bdsp/walk-v4 -> #152 bdsp/detectors-v4 -> #148 bdsp/actions-v4
-> #128 bdsp/autostory-v4 -> #167 bdsp/sim -> #226 mine/floaroma`.

1. Adopt #227's reusable search in #114, then merge forward. That alone does
   **not** change BDSP execution; FRLG consumes it separately through #228.
2. Place #229's world/adapter/planner changes with #127, and movement outcome /
   RouteWalker changes with #129. Merge each owner change forward through the
   existing dependencies. StoryWalkSteps changes belong with #128.
3. Place GridSim event outcomes with #167; keep real mine story regressions
   with the story/simulator owners (#167/#226). Merge into #226 and rerun its
   native suites before any console use. Registration edits follow their tests.
4. Keep #229 as the tested aggregate reference while owners distribute patches.
   It is based on #226, so its full implementation does not flow backwards into
   earlier PRs automatically. Do not merge that aggregate into nav-core.

No map or story-leg edits in #226 are required for the tested recovery fix.
Future parent changes require a fresh combined test, not an assumption that
passing this pinned aggregate proves an arbitrary updated stack.

## Implemented BDSP behavior

A credited tap after a wild battle does not establish displacement. Keep every
legal prefix of an interrupted leg, search for safe common wall holds, execute
them through the actual RouteWalker, and route back to the leg's intended goal
before the next leg. Restore the original leg's facing explicitly. Three further
interrupted holds exhaust the recovery budget and fail rather than advancing.

First-input/late encounters that may belong to an earlier leg fail explicitly.
Unconfirmed transition/dialogue outcomes cannot establish a same-map pose.
Single-step and dialogue-approach interruptions also fail explicitly in this
slice. This deliberately does not infer a previous map/pose from a battle alone.

The BDSP adapter rejects NPC anchors, wandering areas, declared warps (including
warps on blocked glyphs), unknown map edges, cross-map rays and ledge hops.
Recovery metadata is separate from ordinary collision; crop coordinate offsets
are retained for object wandering bounds and warps. Ordinary terrain is unchanged.

## Evidence and validation

- **9 shared tests pass** with native UnitTestDatabase/Common machinery.
- **FRLG selected native tests: 57 pass, 0 fail**, both original #224 search and
  shared adapter. Same registered outcomes, not exhaustive trace equality.
- **FRLG full Clang comparison:** baseline [42010965658](https://github.com/dayfine/Arduino-Source/actions/runs/42010965658)
  and adapter [42050176912](https://github.com/dayfine/Arduino-Source/actions/runs/42050176912)
  both pass the same **83 Overworld** and **30 AutoStorySimulation** tests.
  Both full jobs fail on the same **69 FileException cases for absent image
  fixtures**; there are no additional failing test names/messages in the adapter.
- Untouched FRLG GCC [42010288031](https://github.com/dayfine/Arduino-Source/actions/runs/42010288031)
  fails before tests on existing battle-data dangling references (lines 283/305).
- Shared core full **macOS [42050010624](https://github.com/dayfine/Arduino-Source/actions/runs/42050010624)
  and Windows Clang [42050010694](https://github.com/dayfine/Arduino-Source/actions/runs/42050010694) pass**.
  Linux Clang [42050010826](https://github.com/dayfine/Arduino-Source/actions/runs/42050010826) pending at this checkpoint.
- **BDSP focused native runner: 47 pass, 0 fail** at `a816258cd`.
  Includes 9 shared tests, existing map/story-map/recorded-route/GridSim/
  planner/world/PlannedRoute tests, normalized encounter outcomes, safety
  diagnostics and 9 actual mine story-step cases. The initial focused subset
  was 33 tests; existing coordinate/route tests were added to the runner to
  check recovery metadata placement without changing the tests themselves.
- Rebuilding the original production StoryWalkSteps/PlannedRoute/Walk against
  the initial 8 consumer cases gives **6 failures**, including the reported
  `(19,15)` / `EAST 14 moved 4` shape with two interrupted advances.
  The exact deployed run/revision and actual interruption timing are unavailable;
  before/after movement and two missed advances are labelled diagnostics, not a
  claim that one observed battle caused two missing tiles.
- Frozen red reference: `bdsp/recovery-red-baseline` @ `2ac420dc1`;
  CI [42051485263](https://github.com/dayfine/Arduino-Source/actions/runs/42051485263) confirms
  exactly those 6 consumer failures, including `(19,15) short_leg`; its other
  26 failures are absent BDSP detector image fixtures. 80 BDSP AutoStory tests
  pass on the red reference.
- Patched BDSP full Clang [42054684795](https://github.com/dayfine/Arduino-Source/actions/runs/42054684795)
  at `33898d825` builds and passes **90 BDSP AutoStory tests**, including all
  **9 RecoveryWalk cases** (26,704 total passed). The only 26 failures are the
  exact same missing-image test names as the red baseline. All 6 recovery
  failures are removed. This full job remains red for the image fixtures.
  Latest commit `a816258cd` changes test initialization only; its 47 focused
  tests pass locally. Initial Windows Clang
  [42054689499](https://github.com/dayfine/Arduino-Source/actions/runs/42054689499)
  found missing designated-field initializers in new tests. Commit `a816258cd`
  fixes those warnings only; [Windows rerun 42056081592](https://github.com/dayfine/Arduino-Source/actions/runs/42056081592)
  compiles the new recovery files, then fails on 9 missing-field warnings in
  untouched `PokemonBDSP_AutoStory_StorySim_Tests.cpp` (lines 474/739/742/792/
  835/841/847/866/989). Its Git blob is `f6946a425071911dad51199316277b225fd892e6`
  in both the pinned #226 base and this tip. Hand those warnings to the #167/#226
  owner; do not suppress them or mix unrelated source changes into extraction.
- Code health against each immutable owning base: **0 failures**. CRLF diff
  checked with `git -c core.whitespace=cr-at-eol diff --check`.

Draft automatic build checks are skipped by repository policy. Explicit
workflow_dispatch runs above perform the full builds; skipped is not passed.

## Fidelity boundary and remaining work

Native tests compile the real decoder, maps, shared search, game movement,
planner, story table, StoryWalkSteps and RouteWalker. The focused runner embeds
native JSON resource bytes, uses actual Common/test machinery, and has strict
unused image/filesystem stubs which throw if called. GridSim supplies normalized
cleared encounter outcomes; it does not emulate detector pixels, battle escape
mechanics, cancellation/queue timing, free movement or sub-tile clipping.
Full application CI separately checks normal linking and test registration.

The successful mine case proves complete `below_roark` execution to `(18,29)`
facing west under the scripted outcomes. Source-grounded geometry also proves
rows 15/16/17 at column15 converge south at `(15,17)`, and r55 Route202
`(27,5)/(26,5)` converge east at `(30,5)` clear of Logan. The r55 check is geometry,
not an end-to-end native replay. Warp/NPC corridor cases are synthetic diagnostics.

Still needed: native queued-input/detector characterization, outbound mine
stairs/B1F-B2F and house transitions through real story consumers, full r55 replay,
first-input/late-origin recovery beyond explicit failure, and a wall-vs-unmapped
NPC confirmation policy. Static metadata cannot prove live obstacles absent.
The legacy generic LegInterrupt clear/same-pose contract still needs audit.
Do not claim universal recovery, full-game replay or console validation.

Broader focused recipe/result: `/tmp/build-bdsp-recovery-story.py`,
`/tmp/bdsp-recovery-story-results.log`. An attempted standalone story-suite
link required event/OCR application dependencies; those suites remain for full
CI rather than substituting their production implementations.

Temporary local recipes/logs: `/tmp/build-native-recovery.py` (FRLG; `baseline`
argument), `/tmp/build-bdsp-recovery-consumer.py` (`red` argument),
`/tmp/bdsp-recovery-consumer-results.log`, `/tmp/bdsp-recovery-consumer-red-results.log`,
`/tmp/frlg-recovery-native-results.log`, `/tmp/frlg-recovery-baseline-results.log`,
`/tmp/frlg-clang-baseline-failure.log`, `/tmp/frlg-shared-clang-failure.log`.
Tests themselves are registered beside production sources in the PRs; temporary
recipes are only local build orchestration, not application source-text tests.

The user approved one bootstrap `git push --no-verify` because no PR existed for
the hook to resolve #114 as base. It was used once for #227; all subsequent code
pushes used the normal hook. No upstream PR mutation or GitHub comments/messages.

Quota at 07:46 PST: primary 54%, weekly 20% remaining, from actual local
account/rateLimits/read. Primary resets 11:56 PST; weekly November10 22:35 PST.

## Header and test-contract review, November 5

User requested API-first template headers, namespace labels, comments on each
namespace-scope header symbol, a span input, an explicit policy role, and native
case tables with separate input/expectation structs. Separate draft
[#231](https://github.com/dayfine/Arduino-Source/pull/231) targets `agents` for the
canonical lint implementation and conventions. #227 carries the C++ changes
and replaces its stale branch-local lint workflow with the existing canonical
agents-based workflow. Merge #231 to activate the rules in that workflow and
normal pre-push source; never merge agents into a code branch.

- Linter: `8d2547845` on `lint/cpp-header-test-contracts`.
  Agents Check [42090196868](https://github.com/dayfine/Arduino-Source/actions/runs/42090196868)
  passes. 110 self-tests pass with `CODE_HEALTH_REPO` set to the native checkout;
  notes-only CI has one expected checkout-dependent skip. Harness checks pass.
- Core: `c1a55d784aaf310ee3b47a3e740d6350da8402e0` on #227. Final structural
  rules find **19 errors** in original `af21f2750`, and **zero** in the revised
  native change against #114 `acf85aa2411a125ea1088908e66d4eff93351f1f`.
  Each rule has failing then passing self-test fixtures. CLI integration checks
  failure exit status, GitHub annotations and the corrected-header success.
- A hold is a directional press maintained until a wall stops movement, not a
  tap count or duration. MovementPolicy already selects behavior at compile time;
  the stateful policy carries game map/object-world state. No safe universal
  default collision policy exists. The span is borrowed during the call; branch
  vectors own hypotheses. Existing policy hold() vector contracts are preserved.
- Shared native tests: **10 pass, zero fail**. Corridor inputs use vector;
  corner inputs use array. The prior combined corner case is now two table rows,
  giving one additional registered case. Expectations check selected position,
  direction and held-tile distance, with actual values in failure messages.
- Compatibility: compile existing FRLG `616d2f5f0` and BDSP `a816258cd` adapters
  against the actual revised header via compiler `-include`, substituting the
  revised shared native test source. No game source edits or branch merges:
  **58 FRLG and 48 BDSP focused native cases pass**, zero fail. This is API
  compatibility evidence, not a claim those game branches already inherited #227.
- Full core Windows [42090328101](https://github.com/dayfine/Arduino-Source/actions/runs/42090328101)
  and macOS [42090332966](https://github.com/dayfine/Arduino-Source/actions/runs/42090332966)
  are pending at this checkpoint. #227 code health/no-tabs checks pass.

Recipes/results: `/tmp/build-wall-review.py`, `/tmp/wall-review-native-results.log`,
`/tmp/build-frlg-review.py`, `/tmp/frlg-review-native-results.log`,
`/tmp/build-bdsp-review.py`, `/tmp/bdsp-review-native-results.log`,
`/tmp/wall-recovery-contracts-original-final-rules.log`,
`/tmp/wall-recovery-contracts-green.log`, `/tmp/contracts-selftests-with-code.log`.
Native physics/resource/strict-unused-I/O fidelity boundaries above still apply.


### Current static-policy revision (supersedes the stateful-policy review checkpoint)

User clarified that only the public template needs a separate declaration;
internal templates must be defined in place. #231 now enforces that distinction
and rejects redundant internal forward declarations. All **112 self-tests pass**;
[latest Agents Check](https://github.com/dayfine/Arduino-Source/actions/runs/42093158378)
passes at `e069109f8138c085a76d08ffa944e2b93d939588`.

Core #227 is now `554624edf`: `find_wall_anchors<Policy>(positions, context, limits)`.
All policy operations are static. Map/movement state lives in explicit Context,
not a policy instance or global map. The result vector is constructed after the
already-converged early return. Context/span are borrowed only during the call.

The test comments draw the known walls of a synthetic corridor and closed room.
Walkable coordinates are 0..10; walls are outside at -1/11. A west hold stops at
0 inside the walkable region. Room hypotheses (2,3)/(3,4) converge via north then
west at (0,0). These are analytic search policies, not native map-loader/walker
simulations. Real maps' unknown edges remain the game adapters' responsibility.

To keep dependent PRs compatible, merge #227 forward and switch adapters:

- FRLG #228 `72714076b4ee0ec6d3e002e90895d5ca01826e90`: static ReanchorPolicy,
  explicit ReanchorContext carrying world alternatives/surfing. **58 native
  cases pass**, zero fail, using the actual merged shared header.
- BDSP #229 `e287144a9196999d7b1ac2a6206ad23f23bb3230`: static WallPolicy,
  StoryWorld passed as context. **48 native cases pass**, zero fail.
- Shared **10 native cases pass**, zero fail; revised #227 passes the candidate
  #231 gate with zero findings. Normal adapter pre-push gates pass as well.

The previous core `c1a55d784` passed full Windows [42090328101](https://github.com/dayfine/Arduino-Source/actions/runs/42090328101)
and macOS [42090332966](https://github.com/dayfine/Arduino-Source/actions/runs/42090332966).
Fresh full builds for the static-policy revision are running:
[Windows 42093374031](https://github.com/dayfine/Arduino-Source/actions/runs/42093374031),
[macOS 42093379011](https://github.com/dayfine/Arduino-Source/actions/runs/42093379011).
Do not report the previous full runs as tests of the new static signature.

Recipes/results for this revision: `/tmp/build-wall-review.py`,
`/tmp/build-native-recovery.py`, `/tmp/build-bdsp-recovery-story.py`,
`/tmp/wall-review-native-results.log`, `/tmp/frlg-static-native-results.log`,
`/tmp/bdsp-static-native-results.log`, `/tmp/contracts-selftests-with-code.log`.
All adapter changes preserve map/terrain and route execution behavior; no console
or save was accessed. Quota at 11:26 PST: primary 17%, weekly 14% remaining;
primary reset 11:56 PST. These were actual account readings.
