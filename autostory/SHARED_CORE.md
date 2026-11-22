# AutoStory shared core & seam tracker

**Single source of truth** for the shared AutoStory core (`Pokemon/AutoStory`) and navigation seam stack (`Pokemon/Navigation`). Update this file when a decision or PR status changes; it is not a chronological log.

Cross-references (each owns its own slice — do not duplicate here):
- **PLZA upstream queue:** [plza/UPSTREAM_PLAN.md](plza/UPSTREAM_PLAN.md)
- **Navigation module rules & layout:** [`SerialPrograms/Source/Pokemon/Navigation/README.md`](https://github.com/dayfine/Arduino-Source/blob/autostory/navigation-sim-seam/SerialPrograms/Source/Pokemon/Navigation/README.md) (PR [#771](https://github.com/dayfine/Arduino-Source/pull/771))
- **Post-`core-adopt` duplication audit & Phase 1–4 extraction backlog:** [`docs/design/AutoStory-Reusability-Audit.md`](https://github.com/dayfine/Arduino-Source/blob/docs/autostory-reusability-audit/docs/design/AutoStory-Reusability-Audit.md) (PR [#783](https://github.com/dayfine/Arduino-Source/pull/783))
- **GitHub milestone tracker:** [#144](https://github.com/dayfine/Arduino-Source/issues/144) (M1–M12)
- **Local historical detail notes (not in git):** `~/BDSPLab/notes/autostory-shared-core-map-2026-11-17.md` (`C1`–`C13`), `~/BDSPLab/notes/autostory-engine-migration-design-2026-11-17.md` (`E1`–`E10`), `~/BDSPLab/notes/plza-lean-audit-2026-11-17.md`.

## Current seam PR stack (updated 2026-11-21)

```text
main
 └─ #572 (upstream #1921) autostory/story-range-options-sv        [CLEAN]
     └─ #667 autostory/screen-machine                             [CLEAN]
         └─ #648 autostory/shared-core                            [CLEAN]
             ├─ #654 autostory/logic-tests -> #644 plza/lean      (PLZA lane)
             └─ #653 autostory/grid-core                          [CLEAN]
                 └─ #741 autostory/navigation-seam                [UNSTABLE: 1 Win CI job]
                     └─ #762 autostory/navigation-map-seam        [CLEAN]
                         └─ #764 autostory/navigation-planner-seam [CLEAN]
                             └─ #768 autostory/navigation-gated-seam  [CLEAN]
                                 └─ #769 autostory/navigation-trace-seam  [CLEAN]
                                     └─ #770 autostory/navigation-walker-seam [CLEAN]
                                         └─ #771 autostory/navigation-sim-seam [UNSTABLE: 1 Win CI job]
                                             ├─ #772 bdsp/core-adopt  [0 failing, 2 pending]
                                             └─ #779 frlg/core-adopt  [UNSTABLE: Code Health + Ubuntu]
```

Independent helper on `main`:
- [#728](https://github.com/dayfine/Arduino-Source/pull/728) `ns/system-keyboard` (`UNSTABLE`) — feedback-driven system keyboard typing (shared by BDSP/FRLG naming screens; can land in parallel).

## Landing priority vs. post-`core-adopt` deduplication

**Land the existing `#572` → `#771` (+ `#772`/`#779`) stack FIRST** before starting the new extractions identified in [#783](https://github.com/dayfine/Arduino-Source/pull/783):

1. **9 of 11 seam PRs are already `CLEAN`.** Fixing the single Windows CI failure on `#741` and `#771` unblocks merging the entire stack, immediately removing **~41k lines / ~340 files** of duplicated shared infra from both `bdsp/lean` (`#753`) and `frlg/lean` (`#652`).
2. **Existing seams already provide the Phase 1 primitives.** `#762` (`MapData`/`ConnectedMaps`), `#764` (`WallRecovery`/`RouteScorer`), `#769` (`LegTrace`/`LegReplay`), and `#771` (`SimFaults`/`SimSweep`) are the exact targets that BDSP's `AutoStoryWorld`/`StoryMaps`/`AutoStoryWallRecovery` and FRLG's `OverworldMap`/`OverworldMaps`/`OverworldLegTrace` still bypass. Extracting new seams on top of an unmerged 11-PR stack would cause heavy rebase churn.
3. **Immediate sequence:**
   1. Fix Windows CI on `#741` and `#771`, and fix Code Health + Ubuntu on `#779` (and land `#728` in parallel).
   2. Merge `#572 → #667 → #648 → #653 → #741 → #762 → #764 → #768 → #769 → #770 → #771` onto `main`, then rebase/replace `#753` (`bdsp/lean`) and `#652` (`frlg/lean`) with `#772`/`#779`.
   3. Execute the post-`core-adopt` deduplication phases tracked in [#783](https://github.com/dayfine/Arduino-Source/pull/783) (Phase 1: finish adopting `#762`/`#764`/`#769` and delete bypassed wrappers → Phase 2: `BeliefRouteRunner` + `GridWorldSim` → Phase 3: `StoryStepRegistry` + `EmbeddedStoryFiles` → Phase 4: promote FRLG's bag/healing/mart/party/battle policies before expanding BDSP past Gym 1).

## What the core is

- **The core is PLZA's version** of the shared AutoStory files (screen events, screen machine,
  outcome/log, step types and JSON, story files, event runner, engine), where PLZA's version is
  a strict subset of FRLG's and the infra tests prove PLZA works on it, cut down by S1-S17 below. PLZA is the only
  console-proven user of `StoryEngine`.
- **FRLG additions stay on the FRLG chain as extensions** of core files (step and checkpoint
  fields, conditions/recoveries/interrupts, readers, verify, grid layer). They add fields
  (`= {}`) and free functions; they do not change core signatures.
- **BDSP is mapped and merges last**, after FRLG. BDSP priority is low.
- Working branch: `autostory/shared-core`, fork
  [#648](https://github.com/dayfine/Arduino-Source/pull/648), base #572 (= upstream #1921).
  Merged into `plza/lean` ([#644](https://github.com/dayfine/Arduino-Source/pull/644)) at
  3e4afef53.
- Guard suites: every change to a core file keeps PLZA's suites green, in particular
  StoryEngine_Tests and PokemonLZA_AutoStoryGame_Tests.

## Saves

- The core keeps only the `StoryGame::save` hook (S5); it does not track saves (S6).
- PLZA owns `autosave_policy()` and reads its own `execution` block (S5, S13).
- FRLG's menu save (with its phase gate) and the Hall of Fame self-save are **FRLG policies**
  (E6). BDSP's save-at-start becomes a BDSP end-of-checkpoint policy (E9).

## Core shape (S1-S17, decided 2026-11-18)

The shared core is `Pokemon/AutoStory` on `autostory/shared-core`.

| # | Decision | Why |
|---|---|---|
| S1 | No `PoseType` template anywhere in the core. | One concrete engine; no game type leaks into it. |
| S2 | No pose in the core: `Pose`, `Position`, `RunState::pose`, `Observation::pose`, `Outcome::pose`, `StoryPose` and `ZonePose` leave. FRLG keeps its grid pose in its own backend/game state, read only by its walk actions. | PLZA never read a pose; only FRLG walks do. |
| S3 | No `StoryBackend::kind()`, `StoryBackendKind` or `console_only`: each game registers only the actions its backend can run; an unsupported step fails the lint ("no action named X"). | Capability is which actions exist, not a flag. |
| S4 | No `StoryBackend::battle()`: a game's battle action calls its own battle code. Backend = press, stick, wait, observe (returns `ScreenObservation`), log, scope. | Battles are game logic, not a console primitive. |
| S5 | `autosave_policy()` and its tests move to PLZA; the core keeps only the `StoryGame::save` hook. | Autosave is PLZA/SV only. |
| S6 | The engine does not track saves (no `start_saved`, no `CheckpointRunResult::saved`): `game.reload(backend, checkpoint)` reports whether it landed at that checkpoint's start; retry only then. | The reload knows where it landed; the engine guessed. |
| S7 | The retry loop is ~15 lines inline in `run_checkpoints`; `CheckpointAttempts`, `AttemptResult`, `CheckpointAttemptsResult` and their tests leave the core (FRLG keeps its copy until it moves onto the engine). | Three types for one loop. |
| S8 | No declared start in the core: `DeclaredPose`, `ZoneDeclared`, `read_declared_pose` and `StoryCheckpoint::start` go; plans keep `start` as notes; start checks stay game-owned (PLZA `start_signature`). | Follows S2; each game checks its start its own way. |
| S9 | No REGISTERED arg type: `StoryRegisteredArgValue` (`std::any`), `StoryArgKind`, `make_registered_arg`/`registered_arg` go. | Replaced by S10's typed args. |
| S10 | Parse, don't validate: each action provides `parse(const JsonObject& args)` returning its own typed args or an error, at plan load; a step stores a ready-to-run handler. `StoryArgValue`, `StoryArgType`, the arg map, `StoryArgSpec`, fallbacks, `with_fallbacks` and `story_args_errors` go; one small "unknown key" helper stays. Lint = parse every step of every plan. | Errors at load; no untyped arg bag at run time. |
| S11 | No `StoryStepKind` enum: a step names its action (`"action": "talk"`); games register actions by name; a one-time script rewrites plan files. | Adding an action does not touch the core. |
| S12 | `StoryCheckpointMetadata` (phase etc.) leaves the core; FRLG reads `metadata.phase` in its own save policy (an `EnumStringMap` enum named for what it describes, e.g. `CheckpointTestingStatus`). | Only FRLG's save policy reads it. |
| S13 | The core does not read `execution`: `StoryRowExecution`, `StoryRowSave`, `StoryReload`, `StoryRows`, `StoryData<Checkpoint>` and the `game_fields` workaround leave; each game reads its own block (a shared save/`reload_when_chained` helper may live in game-side code). | Execution fields differ per game. |
| S14 | One checkpoint runner: the engine runs plan steps; `StoryEvent` and the second `run_checkpoint` in `Pokemon_StoryEvents.h` are inlined or deleted. | Two runners drift. |
| S15 | Result is StatusOr-like: `std::optional<StoryFailure>`, `StoryFailure{std::string code; std::string detail; bool repeats = false;}`; `nullopt` = success. `repeats` replaces the engine's failure-code list; `GAME_NOT_RUNNING` is just a `repeats = true` failure; failure codes stay game-owned strings; `changed_nothing` moves to FRLG. | One result shape; the engine knows no game codes. |
| S16 | `CommonTools/ScreenEvents` + `ScreenMachine` come out of #648 into their own infra PR at the bottom of the stack; #648 depends on it. | Infra gets its own strict review. |
| S17 | Landing order below: one agent per step, each pushed + CI + user review before the next. | Small reviewable steps. |

### Landing order (S17)

1. Screen PR split (S16).
2. #648 deletions: S2-S4, S8, S9, S5, S12, S13, S14.
3. #648 engine core: S15, S6, S7, plus the round-2 style rules (AGENTS.md, "Types, names and
   comments": `= {}` on every field, header order, `Testing/`, `MoveOnlyFunction`, `EnumStringMap`,
   `std::formatter` + `= default` equality, enum names).
4. #648 plan format: S10, S11 + the plan-rewrite script.
5. `plza/lean` merges #648 and migrates: autosave policy and `execution` reading into PLZA, plans
   rewritten.
6. `frlg/lean` later, after its own planning round.

## Migration of FRLG and BDSP (E1-E10, accepted as recommended)

| # | Decision |
|---|---|
| E1 | The old runner (`StorySegments`, `StoryStepExecutor`, `TableExecutor`, `StoryRun`, `RunnerExecutor`) never goes upstream. It is frozen on the fork (fixes only) and deleted there after BDSP switches. |
| E2 | Migrate FRLG, then BDSP, on the fork in parallel with the core going upstream. Gate: the FRLG fit check (all FRLG plans on `StoryEngine` in the sim; no pose template, S1) passes before core PR 8 (engine seams) goes upstream. |
| E3 | A `grid-core` branch level between the core and the game lines: shared FRLG+BDSP infra, no game code, no old runner. |
| E4 | Verify mode, verify rows and postconditions are free functions over the engine (`Engine/Pokemon_StoryVerifyRuns.*`), not engine methods. |
| E5 | FRLG checkpoints already passed are proven by sim + replay, not rerun on console B. mGBA is the efficient test bed from here on. The next fresh FRLG playthrough runs on the engine end to end. |
| E6 | Hall of Fame self-save is an FRLG-only policy, not a core `AUTOSAVE` row. |
| E7 | After the core lands, two upstream lanes: PLZA and grid-core. The third slot stays for other PRs. |
| E8 | BDSP rebuild PRs #455-#462 are frozen, re-cut from the migrated BDSP tree later, and closed when replaced. |
| E9 | BDSP saves at a checkpoint's end (end of N = start of N+1). |
| E10 | Thin backends for the switch: actions keep driving the console; deepen per action only where a sim or sweep needs it. |

## Order

1. Dead code cut on `plza/lean`: done (#644, d395be998).
2. Shared core #648: first cut merged into `plza/lean` (3e4afef53); reshaped by the landing
   order above (S17), which runs before steps 4-6.
3. PLZA duplicate and comment cuts on `plza/lean` (audit parts B and D).
4. FRLG merges the core (`frlg/core-adopt`), adapting the C1-C8 call sites.
5. Korean new-game run on console B: the final PLZA check before the split.
6. MVP split for 01.00 + 01.01, about 21 PRs: core C1-C9, then PLZA P10-P21
   (see [plza/UPSTREAM_PLAN.md](plza/UPSTREAM_PLAN.md)).
