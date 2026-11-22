# AutoStory Common Directory & File Structure (and FRLG Rename Plan)

This document defines the target directory and file-naming conventions for AutoStory programs across games, plus the concrete cleanup/rename plan for `PokemonFRLG`.

---

## 1. Naming & Structural Rules

1. **Shared Core (`SerialPrograms/Source/Pokemon/AutoStory/`):**
   - Uses `Pokemon_Story<Role>.{h,cpp}` (`Data/`, `Engine/`, `Types/`, root), `Pokemon_AutoStorySelect.h`, `Pokemon_AutoStoryStats.h`, and `Pokemon_AutoStory_Tests.h`.
2. **Game Program (`SerialPrograms/Source/<VideoGames|Pokemon...>/Pokemon<Game>/Programs/AutoStory/`):**
   - Every file uses `Pokemon<Game>_AutoStory<Role>.{h,cpp}` (CamelCase after `AutoStory`; underscore only before `_Tests`).
   - Test files use matching `Pokemon<Game>_AutoStory<Role>_Tests.{h,cpp}` pairs.
3. **Game Overworld (`SerialPrograms/Source/<VideoGames|Pokemon...>/Pokemon<Game>/Overworld/`):**
   - Uses `Pokemon<Game>_Overworld<Role>.{h,cpp}` (and `_Tests.{h,cpp}`), matching `Pokemon/Navigation/`.
4. **Hard Boundaries:**
   - **No inline tests in production `.cpp` files:** Never embed `UnitTest` definitions or `add_tests_*()` in non-test `.cpp` files; give every test suite its own `*_Tests.{h,cpp}`.
   - **1:1 `.h`/`.cpp` pairing:** Avoid `.cpp`-only files without a matching `.h` and avoid implementing declarations from unrelated headers inside another module's `.cpp`.
   - **No misleading historical holdovers:** File names reflect what the file defines *today*, not what it held before JSON/core extraction.

---

## 2. Common AutoStory Directory Layout

| Layer | Target Pattern | Responsibility |
| --- | --- | --- |
| Authored Plans | `StoryData/<game>-<seg>.plan.json` | Checkpoint JSON definitions (`pokesim-authored-checkpoint/1`). |
| Static Game Data | `BattleData/*.json` | Species, moves, type chart, encounter/trainer data. |
| Program Shell | `Pokemon<Game>_AutoStory.{h,cpp}` | Program descriptor, UI options, `program()`, launch/retry wiring. |
| Story Loader & Order | `Pokemon<Game>_AutoStoryFiles.{h,cpp}` | `STORY_ORDER[]`, embedded JSON loader, `story_checkpoints()` (aligns with `Pokemon_StoryFiles`). |
| Action Registry | `Pokemon<Game>_AutoStoryActionTable.{h,cpp}` | `StoryActionId`, `story_action_specs()`, `make_story_action_table()`. |
| Console Executor | `Pokemon<Game>_AutoStoryExecutor.{h,cpp}` | `StoryEnv`, `StoryExecutor` (`Pokemon::StoryStepExecutor`), `build_checkpoint()`, `make_story_segment()`. |
| Story Routing & Maps | `Pokemon<Game>_AutoStoryStoryMaps.{h,cpp}` | `story_maps()`, `run_story_maps()`, story-wide route costs & planner wrapper. |
| Domain Step Handlers | `Pokemon<Game>_AutoStory<Domain>.{h,cpp}` | Console/sim step implementations grouped by domain (`Battle`, `BattlePolicy`, `BattleText`, `BattleData`, `Capture`, `FieldSteps`, `Elevator`/`Lifts`, `LevelCheck`, `Mart`, `Options`, `Party`, `PcStorage`, `Healing`, `Teach`, `Boulders`, `TrashCans`, `PrepSteps`). |
| Simulation & Replay | `Pokemon<Game>_AutoStoryRowSim.{h,cpp}`, `Pokemon<Game>_AutoStoryScenarioReplay*.{h,cpp}` | Offline step/row/scenario simulation harnesses. |
| Legacy Export Diagnostics | `Data/Pokemon<Game>_AutoStoryLegacyExport*.{h,cpp}` | Migration-only JSON export/diagnostics (never named generically like `JsonWriter`). |
| Unit Tests | `Pokemon<Game>_AutoStory<Role>_Tests.{h,cpp}` | Dedicated header + source per test suite; golden route tables split cleanly by segment (`PlanGolden01To05.h`, `PlanGolden06To09.h`, etc.). |

---

## 3. FRLG Rename & Cleanup Plan (`PokemonFRLG`)

### High Priority (Actively Misleading / Inverted)

| Current File(s) | Issue Today | Target File(s) / Action |
| --- | --- | --- |
| `AutoStoryEvents.{h,cpp}` | Holds `StoryEnv`, `StoryExecutor`, console actions, `build_checkpoint()`, `make_story_segment()` — no event data; also implements `Checkpoints.h`, `PacedRoute.h`, `Poses.h`, `SaveChecks.h`. | Rename to `AutoStoryExecutor.{h,cpp}` (or `AutoStoryConsoleExecutor`); move orphaned definitions to matching `.cpp`s (`Checkpoints`, `Routing`, `Poses`, `SaveChecks`). |
| `AutoStoryTable.{h,cpp}`, `AutoStoryTable_Tests.cpp` | Holds JSON loader (`load_frlg_story`, `STORY_ORDER[]`), not a C++ checkpoint table; collides with `AutoStoryStepTable`; `.h` declares 4 unrelated `add_tests_*`. | Rename to `AutoStoryFiles.{h,cpp}` & `AutoStoryFiles_Tests.{h,cpp}`; give `AutoStory`, `RunPlan`, and `Simulation` their own `*_Tests.h`. |
| `AutoStoryItems_Tests.cpp` | No `AutoStoryItems.{h,cpp}` exists; mostly tests `AutoStoryBattle` (`Test_BattleShows`, `Test_FoeHp`, `Test_PartyHp`, etc.). | Split into `AutoStoryBattle_Tests.{h,cpp}` and `AutoStoryItemsPocket_Tests.{h,cpp}`. |
| `AutoStoryHideout.{h,cpp}`, `AutoStoryHideoutLift.h` | Owns global `field_action_specs()` / `run_field_game_step()` for *all* field `GAME` steps plus Dept Store lifts, not just Rocket Hideout. | Rename global dispatcher to `AutoStoryFieldSteps.{h,cpp}` and lift logic to `AutoStoryElevator.{h,cpp}` (or `AutoStoryLifts`); keep `Hideout` (if kept) for `poster_switch` only. |
| `AutoStoryMember.{h,cpp}` | Almost entirely `LevelCheck` / `level` step execution; `console_member_reader` belongs in `Summary`/`MemberInfo`. | Rename to `AutoStoryLevelCheck.{h,cpp}` (aligned with `Pokemon_StoryLevelCheck.h`). |
| `AutoStoryHealing.cpp` | Embeds `Test_AutoStoryHealing` and `add_tests_AutoStoryHealing()` in production `.cpp` (L176–488). | Extract tests to `AutoStoryHealing_Tests.{h,cpp}`. |
| `Data/JsonWriter.{h,cpp}`, `Data/LegacyJsonFormat.h`, `Data/Json_Tests.{h,cpp}` | Drops `PokemonFRLG_AutoStory` prefix and sounds canonical despite being legacy migration diagnostics (`pokesim-native-checkpoint-export/1`). | Rename to `Data/PokemonFRLG_AutoStoryLegacyExport.{h,cpp}`, `*Format.h`, `*_Tests.{h,cpp}` (and stop cross-calling `add_tests_StoryReplay()`). |
| `PokemonFRLG_StoryReplay*` (`StoryReplay`, `_Executor`, `_IO`, `_Tests`) | Drops `Auto`, uses `_Executor`/`_IO` underscores, collides with `FrameReplay` / `ConsoleFailure` replay. | Rename to `PokemonFRLG_AutoStoryScenarioReplay*.{h,cpp}` and `*_Tests.{h,cpp}`. |
| `AutoStoryCaptureConsole.cpp`, `AutoStoryCapture.h` | `.cpp` has no matching `.h`; `Capture.h` mixes `pc`+`capture` storage dispatch, shared `PatchSteps`, and capture actions. | Add `AutoStoryCaptureConsole.h` (or fold into `Capture`), move `PatchSteps`/`ConsolePatchSteps` to `AutoStoryPacing.{h,cpp}`, and `run_storage_game_step` to `AutoStoryStorageSteps.{h,cpp}`. |

### Medium Priority (Vague, Overloaded, or Overlapping)

- **`AutoStoryActions.{h,cpp}`**: `.h` only holds options + Mart helpers while `.cpp` (991 lines) implements **7 headers** (`Actions`, `Intro`, `Save`, `Presses`, `StartMenu`, `TrainerLine`, `TrainerMessage`) → split into `AutoStoryOptions.{h,cpp}`, `AutoStoryMart`, and matching `.cpp` files.
- **`AutoStoryPacedRoute.{h,cpp}`**: mostly included for `story_maps()` / `run_story_maps()` / `plan_story_route()` → rename to `AutoStoryStoryMaps.{h,cpp}` (or `AutoStoryRouting`).
- **`AutoStoryAbilities.{h,cpp}` & `_Tests.cpp`**: dispatches `mart`/`teach`/`level`/`grind`/`fossil` steps, not Pokémon abilities → rename to `AutoStoryPrepSteps.{h,cpp}` (or `TrainingSteps`).
- **`AutoStoryParty_Tests.cpp` & `AutoStoryBattleDecisions.h`**: `BattleDecisions` tests live inside `Party_Tests.cpp` → split into `AutoStoryBattleDecisions_Tests.{h,cpp}` and `AutoStoryParty_Tests.{h,cpp}`.
- **`AutoStoryBattleData.{h,cpp}`**: mixes static `BattleData/*.json` tables with screen OCR (`read_wild_foes`, `read_trainer_foe`, `run_refused_shows`, `repel_wore_off_shows`, `read_repel_use`) → move OCR to `AutoStoryBattleText.{h,cpp}`.
- **`AutoStoryPlanned*` vs `AutoStoryLeague*`**: unify `Planned*` under `AutoStoryLeaguePolicy*` / `AutoStoryLeagueBattle*`; rename `AutoStoryLeaguePlan_Tests.cpp` (which tests `BattlePolicy`'s `BUDGETED_SETUP`, not `LeaguePlanData`) to `AutoStoryBattlePolicyBudgeted_Tests.{h,cpp}`; fix `AutoStoryLeagueBudget.h`'s stale reference to deleted `AutoStoryLeaguePlan.h`.
- **`AutoStoryFieldSteps.{h,cpp}` & `AutoStoryFieldMoveSteps.{h,cpp}`**: currently test-only JSON step builders → rename to `AutoStoryTestSteps.{h,cpp}` (or `FieldStepBuilders`), freeing `AutoStoryFieldSteps` for `AutoStoryHideout`'s runtime role.
- **`AutoStoryRows*_Tests.h` & `AutoStory_Tests.cpp`**: normalize `Rows` headers (give every test `.cpp` its own `*_Tests.h`, move `CeladonGym` out of `Rows` into `Rows14`, move `Boulders`/`LeaguePlan` out of `Rows19`/`Rows20`); split the 1940-line `AutoStory_Tests.cpp` catch-all.
- **Bag / Items cluster (`Bag`, `BagUse`, `ItemUse`, `ItemsPocket`, `PocketList`, `BattleBag`, `RepelStep`)**: consolidate into clean boundaries (`AutoStoryBagPocket`, `AutoStoryOverworldItemUse`, `AutoStoryBattleBag`).
- **Ambiguous test names**:
  - `AutoStoryCheckLevel_Tests` (tests `StoryCheckLevel`, not `LevelCheck`) → `AutoStoryCheckGating_Tests` (or `StoryCheckLevel_Tests`)
  - `AutoStoryFlyToCenter_Tests` → `AutoStoryFlyLint_Tests` (or fold into `StepLint_Tests`)
  - `AutoStoryEndpoints_Tests` → `AutoStoryEndPose_Tests`
  - `AutoStoryEvolutionWait_Tests` → `AutoStoryBattleExit_Tests` (or fold into `BattleDecisions_Tests`)

### Low Priority & `Overworld/` Notes

- **Golden headers**: rename `AutoStoryPlanGolden.h` to `AutoStoryPlanGolden01To05.h` (matching `06`, `10`, `12`, etc.).
- **Step/Shop naming**: `AutoStoryStepTable` → `AutoStoryActionTable`; `AutoStoryViridianMart` → `AutoStoryShopStep` (or fold into `AutoStoryMart`).
- **`Overworld/` cleanup**:
  - Rename `PokemonFRLG_HideoutLiftRoom.h` → `PokemonFRLG_OverworldLiftRooms.h` (it also defines Celadon Dept Store lifts).
  - Add missing `Overworld` prefix where appropriate (`BikeSpinMechanics`, `MovementMechanics`, `CodedRooms`, `FlyMap`, `PokemonCenterRoom`).
  - Extract inline `UnitTest`s out of `PokemonFRLG_PlayerFacingReader.cpp` and `PokemonFRLG_PokemonCenterFloorReader.cpp` into `*_Tests.{h,cpp}`.
  - Give each `Overworld` test suite its own `*_Tests.h` instead of declaring 4 suites in `OverworldSim.h`.
