# Shared recovery — offline native implementation, 2026-11-05

User authorized driving the migration without consoles. Work used isolated
worktrees; consoles, saves, profiles and other agents' active branches were
untouched. These are fork drafts for owner handoff, not merged changes.

## Review and merge order

| Draft | Branch / tested revision | Base and scope |
|---|---|---|
| [#227](https://github.com/dayfine/Arduino-Source/pull/227) | `nav/recovery-core-extraction` @ `af21f2750289adef1a370ba214eb557b26c9c0ec` | #114 `acf85aa2411a125ea1088908e66d4eff93351f1f`: game-independent convergent wall search, 9 registered native tests. |
| [#228](https://github.com/dayfine/Arduino-Source/pull/228) | `frlg/shared-recovery-adapter` @ `616d2f5f0` | #224 `51c0ce6128e5981da56b370a770874b96a1e129f`, merged #227: replace FRLG's pure search with the shared search. |
| [#229](https://github.com/dayfine/Arduino-Source/pull/229) | `bdsp/shared-recovery-tests` @ `a816258cdc62174dc43000f586d015cc4d7788f5` | #226 `f8bdd62b9df4dbf285f93853147295e0552a0367`, merged #227: BDSP adapter, native encounter outcomes and real story/walker recovery regressions and fixes. |

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
  at `33898d825` is pending. Initial Windows Clang
  [42054689499](https://github.com/dayfine/Arduino-Source/actions/runs/42054689499)
  found missing designated-field initializers in new tests. Commit `a816258cd`
  fixes those warnings only; [Windows rerun 42056081592](https://github.com/dayfine/Arduino-Source/actions/runs/42056081592)
  is pending. No warnings were suppressed.
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

Quota at 07:24 PST: primary 67%, weekly 22% remaining, from actual local
account/rateLimits/read. Primary resets 11:56 PST; weekly November10 22:35 PST.
