# PLZA AutoStory: upstream plan

Current state, updated 2026-11-18. Update it whenever a PR in the plan is opened, merged or passes
on console; it is not a log. Core decisions (save policy seam, FRLG/BDSP migration E1-E10) are in
[../SHARED_CORE.md](../SHARED_CORE.md).

## Upstream slots

- Open on PokemonAutomation/Arduino-Source: [#1845](https://github.com/PokemonAutomation/Arduino-Source/pull/1845),
  [#1881](https://github.com/PokemonAutomation/Arduino-Source/pull/1881),
  [#1921](https://github.com/PokemonAutomation/Arduino-Source/pull/1921) (3/3).
- #1921 (fork [#572](https://github.com/dayfine/Arduino-Source/pull/572), `StoryRangeOptions`)
  is the base of the series. Core PR C1 can go up only after #1921 merges.
- **CommandLineTests upstream PRs do not count** against the three Arduino-Source slots.
- You open every upstream PR and write its description; agents prepare the branch, the tested SHA
  and the handoff notes. Before each handoff an agent merges `main` in, fixes every code-health
  finding (`--base-ref origin/upstream-main`) and gets the unit tests green.

## Working branches

| PR | Branch | Base | Role |
|---|---|---|---|
| [#572](https://github.com/dayfine/Arduino-Source/pull/572) | `autostory/story-range-options-sv` | `main` | = upstream #1921 |
| [#648](https://github.com/dayfine/Arduino-Source/pull/648) | `autostory/shared-core` | #572 | shared core (PLZA's engine + autosave policy seam); C1-C9 are cut from it |
| [#644](https://github.com/dayfine/Arduino-Source/pull/644) | `plza/lean` | `autostory/logic-tests` | PLZA working line, 01.00-03.11; merges the core; K1-K2 and P1-P10 are cut from it |

`plza/lean` replaced the old slice chain #573..#640 (one squash of it, then trimmed). **That chain
is superseded:** no new work there; close it when you're ready. `plza/autostory`
([#576](https://github.com/dayfine/Arduino-Source/pull/576)) is no longer the reference.

## Order

1. Dead code cut on `plza/lean`: done (d395be998).
2. Shared core #648: done, merged into `plza/lean` (3e4afef53).
3. **Next:** cleanup agent on `plza/lean`: start checks from recorded start minimaps, map targets
   for outdoor walks once the map unlocks, and every walk leg names its end event. GateWalk and
   ZoneGateReader are already deleted there (7f80e2f91; 03.08b uses `aimed_run_to_gate`, 3/3 on
   console B 2026-11-18).
4. FRLG merges the core (FRLG chain; see SHARED_CORE.md).
5. Spanish (es-ES) new-game run on console B, 01.00a -> 03.11: the final PLZA check.
6. MVP split (rows 01.00 + 01.01), about 21 PRs, 300-500 lines each, linear, bottom first, in the
   user's order (2026-11-18): keyboard -> core -> 01.00/01.01.

## MVP series

Order (user, 2026-11-18): keyboard -> core -> 01.00/01.01. Rows are listed in upload order.

| # | PR | Contents |
|---|---|---|
| K1-K2 | Switch keyboard | SystemKeyboardDetector, SystemKeyboardTyping, with dark-theme support. Goes first: no dependency on the core |
| C1 | Screen events | ScreenEventTypes, ScreenEvents, ScreenFades |
| C2 | Screen machine | ScreenObservation, ScreenMachine (no StepTrace) |
| C3 | Outcome, pose, log | StoryOutcome, StoryPose, StoryLog |
| C4 | Story types | StepKind, ActionArg, ActionSpec, MapUtil, Direction |
| C5 | Checkpoint and events | Step, Checkpoint, DeclaredPose, Metadata, SegmentTypes, StoryEvent, CheckpointTypes, StoryEvents, CheckpointAttempts |
| C6 | Story JSON | StoryStepJson, AuthoredCheckpointJson |
| C7 | Story files | StoryRowExecution, StoryFiles, EmbedTextFiles.cmake |
| C8 | Engine seams | StoryBackend, PoseConcept, ZonePose, observation bridge, StoryGame, StorySavePolicy, ActionRegistry, StepRunner, FakeBackend. **Gate:** the FRLG fit check (E2) passes before this goes up |
| C9 | Engine | StoryEngine |
| P1-P4 | LZA new game | intro readers, new-game choices, launch + confirm, NewGame |
| P5-P6 | LZA detectors | badge detectors; platform + dialog |
| P7-P9 | LZA runtime | screen + backend, scene machine, walks |
| P10 | LZA story wiring | program, panel, plans 01.00 and 01.01 |

K1-K2 were P10-P11 and P1-P10 were P12-P21 before 2026-11-18.

Each PR carries its tests and `SourceFiles.cmake` lines. Test frames go to
PokemonAutomation/CommandLineTests before the PR that reads them (fork
[CLT #18](https://github.com/dayfine/CommandLineTests/pull/18), `plza/all-frames`; nine
AutoStoryScreenEvents frames still need a recapture on console B).

## After the MVP

Two upstream lanes (E7): PLZA rows after 01.01, and grid-core (FRLG+BDSP shared infra). Other PRs
use the third slot.

## Still open on the fork (your call to close)

- Superseded slice chain #573..#640 (#573-#577, #580, #582, #591, #595, #596, #598, #604, #610,
  #619-#626 and later). The throw helper #590 (base `upstream-main`) is not part of it.
- Older chains: #529-#543, #555-#569; umbrellas #522, #524, #525; older PLZA #335, #354-#356, #416.
- Kept: #492 (fault sweep), #418 (CFR actions, fork only).
