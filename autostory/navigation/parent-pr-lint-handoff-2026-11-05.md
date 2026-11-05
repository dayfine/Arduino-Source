# Parent PR lint handoff — November 5, 2026

The nav-core parent merges are already published through #224 and #226. Do not repeat that work. #228 is now a one-file FRLG adapter diff, and #229 contains only BDSP code/registration changes. Remaining work is bringing older owning PR code into the newly merged #231 lint conventions.

This is a point-in-time inventory against each updated PR base, not a suppression baseline. Counts may fall as earlier fixes are merged forward; rerun the linter at every step.

## Order and findings

Shared first: #114 -> #122. Then either game chain can proceed independently.

FRLG: #213 -> #214 -> #215 -> #216 -> #219 -> #222 -> #224.

BDSP: #134 -> #127 -> #129 -> #152 -> #148 -> #128 -> #167 -> #226.

| PR | Branch | Total | Namespace labels | Header docs | Test-case tables | Template declarations |
|---|---|---:|---:|---:|---:|---:|
| [#114](https://github.com/dayfine/Arduino-Source/pull/114) | `nav/shared` | 129 | 66 | 53 | 6 | 4 |
| [#122](https://github.com/dayfine/Arduino-Source/pull/122) | `nav/story-steps` | 23 | 13 | 8 | 1 | 1 |
| [#213](https://github.com/dayfine/Arduino-Source/pull/213) | `frlg/stage1-core` | 369 | 256 | 103 | 10 | 0 |
| [#214](https://github.com/dayfine/Arduino-Source/pull/214) | `frlg/stage2-rng` | 232 | 161 | 51 | 20 | 0 |
| [#215](https://github.com/dayfine/Arduino-Source/pull/215) | `frlg/stage3-mtmoon` | 259 | 168 | 78 | 13 | 0 |
| [#216](https://github.com/dayfine/Arduino-Source/pull/216) | `frlg/stage4-cerulean` | 4 | 1 | 3 | 0 | 0 |
| [#219](https://github.com/dayfine/Arduino-Source/pull/219) | `frlg/stage5-bill` | 13 | 10 | 2 | 1 | 0 |
| [#222](https://github.com/dayfine/Arduino-Source/pull/222) | `frlg/stage6-vermilion` | 108 | 43 | 62 | 3 | 0 |
| [#224](https://github.com/dayfine/Arduino-Source/pull/224) | `frlg/stage7-celadon` | 16 | 8 | 7 | 1 | 0 |
| [#134](https://github.com/dayfine/Arduino-Source/pull/134) | `bdsp/maps-v4` | 20 | 14 | 1 | 5 | 0 |
| [#127](https://github.com/dayfine/Arduino-Source/pull/127) | `bdsp/nav-v4` | 122 | 77 | 32 | 13 | 0 |
| [#129](https://github.com/dayfine/Arduino-Source/pull/129) | `bdsp/walk-v4` | 105 | 79 | 17 | 9 | 0 |
| [#152](https://github.com/dayfine/Arduino-Source/pull/152) | `bdsp/detectors-v4` | 1 | 1 | 0 | 0 | 0 |
| [#148](https://github.com/dayfine/Arduino-Source/pull/148) | `bdsp/actions-v4` | 116 | 91 | 16 | 9 | 0 |
| [#128](https://github.com/dayfine/Arduino-Source/pull/128) | `bdsp/autostory-v4` | 104 | 86 | 12 | 6 | 0 |
| [#167](https://github.com/dayfine/Arduino-Source/pull/167) | `bdsp/sim` | 55 | 43 | 7 | 5 | 0 |
| [#226](https://github.com/dayfine/Arduino-Source/pull/226) | `bdsp/autostory-mine-floaroma-wip` | 47 | 29 | 14 | 4 | 0 |

## Instructions for the cleanup agent

1. Fetch `origin agents`, then read `git show origin/agents:AGENTS.md` and `git show origin/agents:pokemon_sv/PR_WORKFLOW.md`. Use current shared rules. All PRs are on dayfine/Arduino-Source; never create/reopen/mark-ready upstream PRs.
2. Work from the fresh remote owning branch in an isolated worktree. The remote parent branches advanced; other agents’ active local branches may still be behind. Do not reset or edit their active worktrees.
3. Fix comments/labels/table shapes in their owning PR, then merge the updated parent forward to its child. Preserve history: no rebase/force-push. Do not merge the aggregate game branches back into nav-core.
4. Namespace labels must match the actual namespace. Comments must explain symbol behavior rather than merely repeat its name.
5. Public namespace-scope function templates in headers declare the public API first. Internal helpers are defined in place. Do not introduce redundant internal forward declarations.
6. Native tests use separate input/expectation structs and a case struct with exactly `input`/`expectation`, plus named tables and a range-for registration loop. Preserve every scenario, assertion, expected result and descriptive name; do not mask existing assertions behind empty structs or dummy success flags. Test-table conversion needs native regression verification.
7. Keep runtime logic, movement/controller timing, routes, terrain, map packs and saved-game state unchanged. No console runs or save/profile access. Use native tests/CI, not Python/source-extraction application tests.
8. Do not repeat the one-time hook bypass used to publish these parent-only merges. Ordinary cleanup pushes must pass the canonical lint gate. Existing missing image fixtures are known full-CI failures and must be reported separately.

Run the canonical linter from an agents tools checkout, not an old branch-local copy:

```sh
# In a checkout of the current agents branch:
~/.venvs/code-health/bin/python -m devtools.code_health \
  --root /absolute/path/to/code-worktree \
  --base-ref origin/PR_BASE_BRANCH
```

Some old code branches still contain a stale devtools/code_health; running `python -m` from those code roots can import that copy instead. The canonical pre-push hook uses origin/agents and is authoritative. Stage new C++ files before checking.

Draft C++ builds need explicit workflow dispatch. Run appropriate native tests after each case refactor; leave lengthy CI running if requested, and report exact tested SHAs.

## Snapshot revisions and affected files

### #114: nav/shared

Head: `546b3c0d1d44f361bc29e6cfa1ad2e12d5cd3de2`. Base `autostory/r3-shared-stats`: `cee0b139b102d6f09826f0b46c8654493f4a0a66`.

- `SerialPrograms/Source/CommonTools/ScreenMachine/ScreenMachine.cpp`
- `SerialPrograms/Source/CommonTools/ScreenMachine/ScreenMachine.h`
- `SerialPrograms/Source/CommonTools/ScreenMachine/StepTrace.cpp`
- `SerialPrograms/Source/CommonTools/ScreenMachine/StepTrace.h`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_AutoStory_Tests.h`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StoryEvents.cpp`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StoryEvents.h`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StoryEvents_Tests.cpp`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StorySegments.cpp`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StorySegments.h`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StorySegments_Tests.cpp`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_TilePose.cpp`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_TilePose.h`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_Grid.cpp`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_Grid.h`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_LegPlanner.h`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_LegPlanner_Tests.cpp`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_LegWalker.cpp`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_LegWalker.h`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_LegWalker_Tests.cpp`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_MapData.cpp`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_MapData.h`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_MapData_Tests.cpp`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_Navigation_Tests.h`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_RouteLog.cpp`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_RouteLog.h`
- `SerialPrograms/Source/Pokemon/Navigation/Pokemon_RouteLog_Tests.cpp`

### #122: nav/story-steps

Head: `bb6db58990bd528ef9f0c54534ea3492dbbcf1e3`. Base `nav/shared`: `546b3c0d1d44f361bc29e6cfa1ad2e12d5cd3de2`.

- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_AutoStory_Tests.h`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StorySteps.cpp`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StorySteps.h`
- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StorySteps_Tests.cpp`
- `SerialPrograms/Source/Pokemon/Options/Pokemon_StoryRunOptions.cpp`
- `SerialPrograms/Source/Pokemon/Options/Pokemon_StoryRunOptions.h`

### #213: frlg/stage1-core

Head: `482067e9260d33b88c73405822bccfc8b753ac47`. Base `nav/story-steps`: `bb6db58990bd528ef9f0c54534ea3492dbbcf1e3`.

- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_NamingScreenDetector.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_NamingScreenDetector.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_OptionMenuReader.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_OptionMenuReader.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SaveCardReader.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SaveCardReader.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryMovesReader.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryMovesReader.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleHpBarReader.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleHpBarReader.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldReanchor.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldReanchor.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldReanchor_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecovery.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecovery.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecoveryWalk.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecoveryWalk.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecovery_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStory.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStory.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryActions.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryActions.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattle.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattle.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleData.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleData.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleData_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleDecisions.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleDecisions.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleSim.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleSim.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleSim_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryDamage.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryDamage.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryFoeTracker.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryFoeTracker.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryItems_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryParty.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryParty.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryParty_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySimulation_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySummary.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySummary.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTable.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTable.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryWinChance.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryWinChance.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStory_Tests.cpp`

### #214: frlg/stage2-rng

Head: `6096678ac2e425f8aa1eaa6d03e8a903ed48e529`. Base `frlg/stage1-core`: `482067e9260d33b88c73405822bccfc8b753ac47`.

- `SerialPrograms/Source/VideoGames/PokemonFRLG/PokemonFRLG_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStory.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRngLoop.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRngLoop.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRngLoop_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRngOptions.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRngOptions.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStory_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/PokemonFRLG_NamingScreen.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/PokemonFRLG_NamingScreen.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/PokemonFRLG_NamingScreenKeyboard.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/PokemonFRLG_NamingScreenKeyboard.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/PokemonFRLG_TrainerCard.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/PokemonFRLG_TrainerCard.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanInputs.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanInputs.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanInputs_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanInputs_Tests.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanLog.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanLog.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanLog_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanLog_Tests.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTestTargets.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTestTargets.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTracker.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTracker.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTracker_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTracker_Tests.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan_Tests.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngSteps_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngSteps_Tests.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngTargetSearch.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngTargetSearch.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngTargetSearch_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngTargetSearch_Tests.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SidSteps.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SidSteps.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngAttempts.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngAttempts.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngConsoleSteps.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngConsoleSteps.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngLoop.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngLoop.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngLoop_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngLoop_Tests.h`

### #215: frlg/stage3-mtmoon

Head: `e36b1b50cc86215c12f7c53c641ff54919b5b5b3`. Base `frlg/stage2-rng`: `6096678ac2e425f8aa1eaa6d03e8a903ed48e529`.

- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Box/PokemonFRLG_PcScreenReader.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Box/PokemonFRLG_PcScreenReader.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_TeachScreenReader.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_TeachScreenReader.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleTextBoxDetector.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleTextBoxDetector.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryAbilities.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryAbilities.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryAbilities_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryActions.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCapture.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCapture.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCapture_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryChoice.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryChoice.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryChoice_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryGrind.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryGrind.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryGrind_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMart.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMart.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMart_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMember.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMember.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPcStorage.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPcStorage.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPcStorage_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRngLoop.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRunPlan_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTeach.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTeach.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTeachScreens.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTeachScreens.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTeachScreens_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTeach_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryVariantWords.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryVariantWords.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_GiftRngAttempts.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_GiftRngAttempts.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_GiftRngAttempts_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_GiftRngAttempts_Tests.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_GiftRngConsoleSteps.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_GiftRngConsoleSteps.h`

### #216: frlg/stage4-cerulean

Head: `4e3cf21ebd0c94f71196c7468d0606cf773ef5b5`. Base `frlg/stage3-mtmoon`: `e36b1b50cc86215c12f7c53c641ff54919b5b5b3`.

- `SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StorySteps.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleSim.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.h`

### #219: frlg/stage5-bill

Head: `7b6a28cebae30896aa10d00378d84ebba6322d6b`. Base `frlg/stage4-cerulean`: `4e3cf21ebd0c94f71196c7468d0606cf773ef5b5`.

- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPaceSim.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPaceSim.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPaceSim_Tests.cpp`

### #222: frlg/stage6-vermilion

Head: `130344db1cb2a394693019d734145f8c295c589b`. Base `frlg/stage5-bill`: `7b6a28cebae30896aa10d00378d84ebba6322d6b`.

- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleHpBarReader.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleSim.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleSim.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryGrind.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRowSim.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRowSim.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTableSteps.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTableSteps.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTableVermilionGym.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTrashCans.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTrashCans.h`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTrashCans_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryVermilionGym_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryVermilion_Tests.cpp`

### #224: frlg/stage7-celadon

Head: `cc43650ef87c91aa4a6ac049c37114cfddf82d36`. Base `frlg/stage6-vermilion`: `130344db1cb2a394693019d734145f8c295c589b`.

- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCeladon_Tests.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTableCeladon.cpp`
- `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTableSteps.h`

### #134: bdsp/maps-v4

Head: `7e1bc25ebb37c16870937026e1b099e7c189c4a5`. Base `nav/story-steps`: `bb6db58990bd528ef9f0c54534ea3492dbbcf1e3`.

- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Maps_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Maps_Tests.h`

### #127: bdsp/nav-v4

Head: `8805523710a2ed6298583cc53652316116f0fc6e`. Base `bdsp/maps-v4`: `7e1bc25ebb37c16870937026e1b099e7c189c4a5`.

- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RouteRecords.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RouteRecords.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Segment00Lines.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Segment00Lines.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryMaps.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryMaps.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_RoutePlanner_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_RoutePlanner_Tests.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Segment00_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Segment00_Tests.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryMaps_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryMaps_Tests.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWorld_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWorld_Tests.h`

### #129: bdsp/walk-v4

Head: `89f673f74988cea2333e7dd157136f3aa5f39f0e`. Base `bdsp/nav-v4`: `8805523710a2ed6298583cc53652316116f0fc6e`.

- `SerialPrograms/Source/PokemonBDSP/Inference/PokemonBDSP_TransitionWatchers.cpp`
- `SerialPrograms/Source/PokemonBDSP/Inference/PokemonBDSP_TransitionWatchers.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_PlannedRoute_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_PlannedRoute_Tests.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Walk_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Walk_Tests.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_OverlayLog.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_OverlayLog.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteIO.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteIO.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h`

### #152: bdsp/detectors-v4

Head: `f5a3c42abf063fcc366f024a9a7986a9c97dd836`. Base `bdsp/walk-v4`: `89f673f74988cea2333e7dd157136f3aa5f39f0e`.

- `SerialPrograms/Source/PokemonBDSP/Inference/PokemonBDSP_MenuDetector.cpp`

### #148: bdsp/actions-v4

Head: `101c5b8172d5df14f8d4f029e64150b4c448c789`. Base `bdsp/detectors-v4`: `f5a3c42abf063fcc366f024a9a7986a9c97dd836`.

- `SerialPrograms/Source/NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.cpp`
- `SerialPrograms/Source/NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h`
- `SerialPrograms/Source/PokemonBDSP/Inference/PokemonBDSP_NameListDetector.cpp`
- `SerialPrograms/Source/PokemonBDSP/Inference/PokemonBDSP_NameListDetector.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_BattleMenuReader.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_BattleMenuReader.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_MenuCursorDetector.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_MenuCursorDetector.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Actions.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Actions.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Actions_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Actions_Tests.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Bag.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Bag.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Battle.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Battle.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_BattleMoves.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_BattleMoves.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Center.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Center.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_NewGame.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_NewGame.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Settings.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Settings.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_WildBattle.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_WildBattle.h`

### #128: bdsp/autostory-v4

Head: `7f16d8cab644e842a2d30a9adf89c56da7615a33`. Base `bdsp/actions-v4`: `101c5b8172d5df14f8d4f029e64150b4c448c789`.

- `SerialPrograms/Source/PokemonBDSP/PokemonBDSP_Panels.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Events.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Events.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Events_Jubilife.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Events_Lake.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Events_Oreburgh.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Events_Route204.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Events_Sandgem.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StorySimulation_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StorySimulation_Tests.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable_Tests.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Talks.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Talks.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Walk_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Walks.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Walks.h`

### #167: bdsp/sim

Head: `b6f2f4d1ba993670dafc1daa06fdc83b3744032b`. Base `bdsp/autostory-v4`: `7f16d8cab644e842a2d30a9adf89c56da7615a33`.

- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_CoordEvents.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_CoordEvents.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_GridSim_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_GridSim_Tests.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StorySim_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StorySim_Tests.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryStepIO.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryStepIO.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_GridSim.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_GridSim.h`

### #226: bdsp/autostory-mine-floaroma-wip

Head: `e65fd909297b58068e2dd92e534e969b2acc086d`. Base `bdsp/sim`: `b6f2f4d1ba993670dafc1daa06fdc83b3744032b`.

- `SerialPrograms/Source/CommonFramework/Windows/LabChannel.cpp`
- `SerialPrograms/Source/CommonFramework/Windows/LabChannel.h`
- `SerialPrograms/Source/CommonFramework/Windows/LabChannel_Tests.cpp`
- `SerialPrograms/Source/CommonFramework/Windows/LabChannel_Tests.h`
- `SerialPrograms/Source/CommonFramework/Windows/LabRequest.cpp`
- `SerialPrograms/Source/CommonFramework/Windows/LabRequest.h`
- `SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileParsing.cpp`
- `SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileParsing.h`
- `SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileRunner.cpp`
- `SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileRunner.h`
- `SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileRunner_Tests.cpp`
- `SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileRunner_Tests.h`
- `SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_LabCommands.cpp`
- `SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_LabCommands.h`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryMaps_Tests.cpp`
- `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StorySim_Tests.cpp`

## Evidence

Inventory: canonical merged style rules (the implementation behind #231), full PR diff against the updated parent, not just the six-file parent refresh. All listed findings fall into the four categories above. Logs on this machine: `/tmp/nav-parent-NUMBER-full-lint.log`; machine-readable snapshot: `/tmp/nav-parent-lint-inventory.json`.

The 16 parent-only merges themselves passed incremental lint against their previous tips and contained only the six reviewed #227 files. Their publication used one explicitly approved atomic fast-forward-only push. That approval was consumed. No functional lint cleanup has yet been applied to these parent PRs.
