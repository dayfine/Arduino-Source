# AutoStory shared core

Current state, decided with the user on 2026-11-17. Update this file when a decision changes; it
is not a log. The PLZA upstream queue is in [plza/UPSTREAM_PLAN.md](plza/UPSTREAM_PLAN.md).

Detail notes (local, not in git): `~/BDSPLab/notes/autostory-shared-core-map-2026-11-17.md`
(per-file map, conflicts C1-C13), `~/BDSPLab/notes/autostory-engine-migration-design-2026-11-17.md`
(FRLG/BDSP onto the engine, decisions E1-E10), `~/BDSPLab/notes/plza-lean-audit-2026-11-17.md`
(dead code, duplicates, the 21-PR MVP split).

## What the core is

- **The core is PLZA's version** of the shared AutoStory files (screen events, screen machine,
  outcome/pose/log, step types and JSON, story files, event runner, engine), where PLZA's version is
  a strict subset of FRLG's and the infra tests prove PLZA works on it. PLZA is the only
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
  StoryEngine_Tests on `ZonePose` and PokemonLZA_AutoStoryGame_Tests.

## Saves

- **Save is a policy seam:** `StorySavePolicy` (`Engine/Pokemon_StorySavePolicy.h`), held by
  `StoryGame::save`.
- The core has **only PLZA's `autosave_policy()`**.
- The core row save enum is `StoryRowSave{NONE, AUTOSAVE, UNVERIFIED}`. **No `MENU`:** autosave is
  SV/PLZA only, and menu saving is not a core concept.
- FRLG's menu save (with its phase gate) and the Hall of Fame self-save are **FRLG policies**
  (E6). BDSP's save-at-start becomes a BDSP end-of-checkpoint policy (E9).

## Migration of FRLG and BDSP (E1-E10, accepted as recommended)

| # | Decision |
|---|---|
| E1 | The old runner (`StorySegments`, `StoryStepExecutor`, `TableExecutor`, `StoryRun`, `RunnerExecutor`) never goes upstream. It is frozen on the fork (fixes only) and deleted there after BDSP switches. |
| E2 | Migrate FRLG, then BDSP, on the fork in parallel with the core going upstream. Gate: the FRLG fit check (all FRLG plans on `StoryEngine<GridPose>` in the sim) passes before core PR 8 (engine seams) goes upstream. |
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
2. Shared core #648: done, merged into `plza/lean` (3e4afef53).
3. PLZA duplicate and comment cuts on `plza/lean` (audit parts B and D).
4. FRLG merges the core (`frlg/core-adopt`), adapting the C1-C8 call sites.
5. Korean new-game run on console B: the final PLZA check before the split.
6. MVP split for 01.00 + 01.01, about 21 PRs: core C1-C9, then PLZA P10-P21
   (see [plza/UPSTREAM_PLAN.md](plza/UPSTREAM_PLAN.md)).
