# Lab build R6: R5 + C15 reroll impossible quests

Built 2026-10-22. Not launched.

## Source

Worktree `/Users/difan/Projects/Arduino-Source-r6`, local branch `lab/r6` (not pushed), from `lab/r5` 0fdf7d8ad1f0187517ee0af3182c8832c421acab.

| Step | Source revision | Lab commit |
| --- | --- | --- |
| 1. #25 C15 reroll impossible quests (`bbq/reroll-impossible-quests`) | de563f7d868953fd9f8f3bb8759180de5226c80d | Cherry-pick 849e1fab3 ("Lab R6: merge C15 reroll impossible quests (#25) at de563f7d8") |
| 2. Lab tests: C15 vs the endless-reroll guard (#5) and out of BP | - | ebb896105d2389541a8cd8282407f252369571da |

C15's own commit only was cherry-picked; its base (#9, `bbq/unsupported-quest-rerolls`) is already in the lab under
other SHAs.

**Tested integrated SHA (branch head): ebb896105d2389541a8cd8282407f252369571da**

## Merge conflicts

None. The cherry-pick applied cleanly to `PokemonSV_BlueberryQuests.cpp/.h` and `PokemonSV_Tests.cpp`. All test
registrations kept. Touched files are CRLF throughout, no tabs.

## Interaction with the endless-reroll guard (#5) and out of BP

The lab's `run_bbq_rounds()` has #5's guard: a startup check that the exclusions table leaves a doable blue quest,
and a stop (`UserSetupError`) after 30 panel reads in a row with nothing doable.

- **Startup check:** unchanged. It counts blue quests minus `not_possible_quests`; C15 does not change that set.
- **Read limit:** a panel with doable quests next to a quest not possible now rerolls, which empties the batch, so
  that read counts toward the 30. The doable quests run from the next read (one extra read and process per
  impossible quest). Only rerolls that keep drawing quests not possible 30 times in a row reach the limit.
- **Out of BP (behavior change):** before C15, a panel with doable quests next to a quest not possible ran the
  doable quests and spent no BP. Now the impossible quest is rerolled first; out of BP, `reroll_quest()` returns
  `NotReplaced` and `reroll_quest_slot()` stops the program ("Check that there is enough BP", no error report)
  before the doable quests run. A red impossible quest needs 50 BP. The read limit is not reached in that case.
  A panel of only skipped/undetected quests still goes to reroll-all, which does not verify, so out of BP the
  read limit stops it as before.

New: `SimulatedBBQGameActions` can plan with `plan_quests()` (`plan_options`); a slot reroll out of BP throws
`RerollNotReplaced`, standing in for `reroll_quest_slot()`. Four `BBQFarmerRounds` scenarios:

| Scenario | Expected |
| --- | --- |
| a quest not possible is rerolled before the doable quests run | `read process reroll(slot)`, then a round of 3 |
| rerolls that keep drawing quests not possible reach the read limit | 30 x `read process reroll(slot)`, then `error` |
| out of BP, a quest not possible stops the run before doable quests | `read process reroll(slot) stopped` |
| out of BP, skipped quests alone reach the read limit | 30 x `read process reroll(all)`, then `error` |

## Build

R5's commands, unchanged:

```sh
cd ~/Projects/Arduino-Source-r6
PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig \
cmake -S SerialPrograms -B build-prep/app -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH=$HOME/Projects/Arduino-Source/.cache/qt/6.8.3/macos \
  -DWrapOpenGL_AGL=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/System/Library/Frameworks/AGL.framework/AGL.tbd \
  > build-prep/configure.log 2>&1
nice -n 10 ninja -C build-prep/app -j6 SerialPrograms > build-prep/build.log 2>&1
```

Result: **PASS** (0 errors, single full build). Binary links `/System/Library/Frameworks/AGL.framework/Versions/A/AGL`.

**Binary:** `/Users/difan/Projects/Arduino-Source-r6/build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms`
**SHA-256:** `c54adafdddcadb2b02ef6d9ed2c357a17c89de06099cddbe6955edefc6d150be`

## Native tests

`build-prep/link-native-tests.py` and `build-prep/bbq-native-runner.cpp` copied from R5 unchanged.

```sh
cd ~/Projects/Arduino-Source-r6
python3 build-prep/link-native-tests.py > build-prep/native-link.log 2>&1
./build-prep/app/BBQNativeTests > build-prep/native-tests.log 2>&1
```

Result: **138 passed, 0 failed** (R5: 128; + C15's net 6 BBQuestPlan, + 4 lab BBQFarmerRounds).

| Group | PASS | R5 |
| --- | --- | --- |
| BBQAutoBattle | 16 | 16 |
| BBQCatchOnReturn | 6 | 6 |
| BBQFarmerRounds | 29 | 25 |
| BBQPlazaBannerMatch | 21 | 21 |
| BBQPlazaBannerToCheck | 5 | 5 |
| BBQPlazaReturn | 5 | 5 |
| BBQPointsLedger | 6 | 6 |
| BBQReroll | 17 | 17 |
| BBQuestPlan | 20 | 14 |
| BlueberryPoints (frame OCR) | 4 | 4 |
| TeraEncounterAction | 9 | 9 |

## Launch (not done)

Profile `bbq-r6` holds a byte-for-byte copy of `bbq-r5/UserSettings/SerialPrograms-Settings.json` as of this build
(UTF-8 BOM kept; SHA-256 `af617f4fda24ff3a19b85f3794a2b9f67432b7d69d378a2a349413394c59f5fd`). This differs from the
R5 build note's hash: bbq-r5 was edited at 13:19 (backup `SerialPrograms-Settings.json.bak-r5a`), adding exclusion
rows `catch-flying: reroll` and `photo-flying: reroll`. Under C15 those rows do what the default now does.
BBQ language is still `chi_tra`.

```sh
/Users/difan/Projects/Arduino-Source-r6/build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms --profile bbq-r6
```

The first launch of a new binary shows camera/microphone prompts.
