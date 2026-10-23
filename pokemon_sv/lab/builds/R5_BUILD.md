# Lab build R5: R4 + C14 plaza arrival check

Built 2026-10-22. Not launched.

## Source

Worktree `/Users/difan/Projects/Arduino-Source-r5`, local branch `lab/r5` (not pushed), from `lab/r4` 124ca62068a9d9d2caeba3945ab3e600c00970ad.

| Step | Source revision | Lab commit |
| --- | --- | --- |
| 1. #24 C14 plaza arrival check (`bbq/plaza-arrival-check`) | 4d6adbd5e87a5ded3fecdced589fc3e0df9db6ff | Merge 0fdf7d8ad1f0187517ee0af3182c8832c421acab |

**Tested integrated SHA (branch head): 0fdf7d8ad1f0187517ee0af3182c8832c421acab**

## Merge conflicts

C14 changes `return_to_plaza()` to take a `Language` and check the landing banner (retrying up to 3 times),
and makes `map_move_cursor_fly()` throw `TerariumFlyFailedException`, which `process_and_do_quest()` catches to
fail the attempt instead of the program. Conflicts:

1. **`PokemonSV_Terarium.cpp`**: the lab (#23) had split the fly loop into `return_to_plaza_impl(stop_at_battle)`
   for `return_to_plaza_unless_battle()`; C14 renamed the same loop to `fly_toward_plaza(laggy)` behind
   `return_to_plaza_checked()`. Merged: `fly_toward_plaza(laggy, stop_at_battle)` returns true when it stopped at
   a battle. `PlazaReturnConsoleActions` takes `stop_at_battle`; when the fly stops at a battle it throws a private
   `StoppedAtBattle` that the new `return_to_plaza_impl(language, stop_at_battle)` catches and returns true.
   So both `return_to_plaza(..., Language)` and `return_to_plaza_unless_battle(..., Language)` (header updated)
   run C14's banner check. `PlazaReturnActions` / `return_to_plaza_checked()` are unchanged from C14.
2. **`PokemonSV_BlueberryCatchPhoto.cpp`** `quest_catch()`: kept #23's catch-on-return loop; both calls pass
   `BBQ_OPTIONS.LANGUAGE`.
3. **`PokemonSV_BlueberryQuests.cpp`** `quest_auto_battle()`: kept #21's `run_auto_battle()` (C14 only edited the
   old inline loop's `return_to_plaza` call). `ConsoleBBQAutoBattleActions` gained a `Language` member and passes it
   to `return_to_plaza()`.
4. **`PokemonSV_Tests.cpp`**: both include lists, all registrations (lab + `BBQPlazaBannerToCheck`,
   `BBQPlazaBannerMatch`, `BBQPlazaReturn`). Build error after the textual merge: C14 adds a `NullLogger`
   identical to the lab's; dropped C14's copy.

All touched source files are CRLF throughout, no tabs.

## Build

R4's commands, unchanged:

```sh
cd ~/Projects/Arduino-Source-r5
PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig \
cmake -S SerialPrograms -B build-prep/app -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH=$HOME/Projects/Arduino-Source/.cache/qt/6.8.3/macos \
  -DWrapOpenGL_AGL=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/System/Library/Frameworks/AGL.framework/AGL.tbd \
  > build-prep/configure.log 2>&1
nice -n 10 ninja -C build-prep/app -j6 SerialPrograms > build-prep/build.log 2>&1
```

Result: **PASS** (0 errors; `build.log` holds the second, incremental run after the `NullLogger` fix). Binary links
`/System/Library/Frameworks/AGL.framework/Versions/A/AGL`.

**Binary:** `/Users/difan/Projects/Arduino-Source-r5/build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms`
**SHA-256:** `228f1328bed937e7fad860f461fbbe4ecdb240c93fb6d078403d4d3628927ed6`

## Native tests

`build-prep/link-native-tests.py` copied from R4 unchanged. `build-prep/bbq-native-runner.cpp` is C14's (adds the
`--banner <png>...` OCR mode) with R4's filter (`PokemonSV::BBQ*`, `TeraEncounterAction*`, `BlueberryPoints*`).

```sh
cd ~/Projects/Arduino-Source-r5
python3 build-prep/link-native-tests.py > build-prep/native-link.log 2>&1
./build-prep/app/BBQNativeTests > build-prep/native-tests.log 2>&1
```

Result: **128 passed, 0 failed** (R4: 97, + C14's 31).

| Group | PASS | R4 |
| --- | --- | --- |
| BBQAutoBattle | 16 | 16 |
| BBQCatchOnReturn | 6 | 6 |
| BBQFarmerRounds | 25 | 25 |
| BBQPlazaBannerMatch | 21 | - |
| BBQPlazaBannerToCheck | 5 | - |
| BBQPlazaReturn | 5 | - |
| BBQPointsLedger | 6 | 6 |
| BBQReroll | 17 | 17 |
| BBQuestPlan | 14 | 14 |
| BlueberryPoints (frame OCR) | 4 | 4 |
| TeraEncounterAction | 9 | 9 |

No unit test covers the `stop_at_battle` path through the checked loop (console-only).

## Launch (not done)

Profile `bbq-r5` holds a byte-for-byte copy of `bbq-r4/UserSettings/SerialPrograms-Settings.json` (UTF-8 BOM kept;
SHA-256 `8e43d6454ba04b90619cfe8475250fc8df3c510b3909c0a838dbb6e31cb5256c`). Its BBQ language is `chi_tra`, so
landings are checked against 中心廣場.

```sh
/Users/difan/Projects/Arduino-Source-r5/build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms --profile bbq-r5
```

The first launch of a new binary shows camera/microphone prompts.
