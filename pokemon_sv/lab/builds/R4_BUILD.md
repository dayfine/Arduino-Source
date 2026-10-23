# Lab build R4: heal threshold + panel-read reuse + catch grace

Built 2026-10-22. Not launched.

## Source

Worktree `/Users/difan/Projects/Arduino-Source-r4`, local branch `lab/r4` (not pushed), from `lab/r3` 59fa368a6a90cc4c47ccdfaeca89bb3b91e61faf.

| Step | Source revision | Lab commit |
| --- | --- | --- |
| 1. #21 Auto Battle heal threshold (`origin/bbq/auto-battle-heal-threshold`) | 6a2d0c23dbfb4ddadefa753e2e19c97d46990064 | Merge 3d7bda78b07dcfc0f233e9f77f001b1fdde1811e |
| 2. #22 C9 panel-read reuse (`origin/bbq/panel-read-reuse`) | 57088de5f5229d020b6ccf5760dfc6af1a5ee2a5 | Merge 9629cc82bb453ac3f19f96a73eabeb94b94b2497 |
| 2b. Lab: record BP on reroll verification reads | - | cb7f4fa09363fad3077ad098f2bb7d61fa793f49 |
| 3. #23 catch grace and heal skip (`origin/bbq/catch-grace-and-heal`) | 99377cf34604ec5a3f1a5cdb8e586ce54d673fd3 | Merge 124ca62068a9d9d2caeba3945ab3e600c00970ad |

**Tested integrated SHA: 124ca62068a9d9d2caeba3945ab3e600c00970ad**

Not merged: #19 (diagnostics), #20 (fly-from-anywhere).

## Merges and conflicts

1. **#21**: clean merge.
2. **#22**: `bbq/panel-read-reuse` sits on `bbq/replan-after-reroll` (#13). The lab already carries
   those commits as `bbq/m7b-replan` equivalents (075376f2f = f7e3ba6f0 exclusions reroll,
   57f330fc9/ea047a930 M7a/M7b, 8aa1821b4 save regression). `git diff m7b-replan replan-after-reroll`
   only adds lines the lab had already replaced, so the merge tree is **lab/r4 + the diff of 57088de5f only**
   (both parents recorded). Conflicts while applying it:
   - `PokemonSV_BBQSoloFarmer.cpp` round loop: kept the lab's 30-read `UserSetupError` guard; dropped the
     `reused_panel` local as 57088de5f does (first dispatch now reuses the planned panel whenever work is returned).
   - `PokemonSV_BBQSoloFarmer.cpp` `reroll_quest()` adapter: took #22's `std::optional<std::vector<BBQuests>>`
     return and kept R3's `read_quests(..., &BP)` + `record_BP(BP)` on its panel read.
   - `PokemonSV_Tests.cpp` `SimulatedBBQGameActions`/`BBQRoundsScenario`: kept the lab's `out_of_bp` and
     `exclusions` plus #22's `table_rerolls` and `expected_reads`; `out_of_bp` also blocks table rerolls.
     `round_of_3` takes #22's new value; lab-only setup lines (`read_30_times`, `all_skipped_but_make_tm`) kept.
     All lab-only scenarios still pass with the new expectations unchanged.
3. **BP tracking with panel reuse (lab commit cb7f4fa09).** With #22 the next plan can start from a reroll's
   verification read instead of a `read_quests()` call, which was R3's only BP sampling point, so those reads
   would have been lost from the ledger. `reroll_quest_slot()` and `process_quest_list()` gained an optional
   `std::function<void(int BP)> record_BP` (default empty, other callers unchanged).
   `ConsoleBBQRerollActions::read_quests()` OCRs BP from the same screenshot and calls it. The farmer adapter
   passes `record_BP` for both the out-of-retries reroll and the plan/table rerolls in `process_quest_list()`.
   Every panel read now records BP, whether reused or not, with no extra button presses. This is also more
   samples than R3 (R3 did not record the verification reads at all), so Earned/Spent split better around rerolls.
   The net is unchanged. No unit test covers this path (it is console-only).
4. **#23**: conflict only in `PokemonSV_Tests.cpp`: kept both include lists (BlueberryQuests/BBQSoloFarmer +
   BlueberryCatchPhoto) and all registrations (lab BBQ, BlueberryPoints, BBQPointsLedger + BBQCatchOnReturn).
   #23's heal skip (after no-battle catch attempts, `Terarium`/`BlueberryCatchPhoto`) and #21's heal threshold
   (Auto Battle passes, `BlueberryQuests`) touch different paths.

All touched source files are CRLF throughout, no tabs.

## Build

Same as R3:

```sh
cd ~/Projects/Arduino-Source-r4
PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig \
cmake -S SerialPrograms -B build-prep/app -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH=$HOME/Projects/Arduino-Source/.cache/qt/6.8.3/macos \
  -DWrapOpenGL_AGL=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/System/Library/Frameworks/AGL.framework/AGL.tbd \
  > build-prep/configure.log 2>&1
nice -n 10 ninja -C build-prep/app -j6 SerialPrograms > build-prep/build.log 2>&1
```

Result: **PASS** (1477/1477, 0 errors). Same OpenCV 4 and AGL workarounds as R3; the binary links
`/System/Library/Frameworks/AGL.framework/Versions/A/AGL`. The bundle layout (`find SerialPrograms.app -maxdepth 4`
plus `Resources/`, `Firmware/`, `Scripts/` next to the bundle) is identical to R3's.

**Binary:** `/Users/difan/Projects/Arduino-Source-r4/build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms`
**SHA-256:** `f9e763b33810f864512bb50f4f7fcf4561ba0bd6a72192674dd08e5aa3628fe6`

## Native tests

R3's runner, copied unchanged (`build-prep/link-native-tests.py`, `build-prep/bbq-native-runner.cpp`). The filter
`PokemonSV::BBQ*`, `TeraEncounterAction*`, `BlueberryPoints*` already includes #23's `BBQCatchOnReturn`.

```sh
cd ~/Projects/Arduino-Source-r4
python3 build-prep/link-native-tests.py > build-prep/native-link.log 2>&1
./build-prep/app/BBQNativeTests > build-prep/native-tests.log 2>&1
```

Result: **97 passed, 0 failed** (R3: 76).

| Group | PASS | R3 |
| --- | --- | --- |
| BBQAutoBattle | 16 | 8 |
| BBQCatchOnReturn | 6 | - |
| BBQFarmerRounds | 25 | 22 |
| BBQPointsLedger | 6 | 6 |
| BBQReroll | 17 | 13 |
| BBQuestPlan | 14 | 14 |
| BlueberryPoints (frame OCR) | 4 | 4 |
| TeraEncounterAction | 9 | 9 |

## Launch (not done)

Profile `bbq-r4` was created with a copy of `bbq-r3/UserSettings/SerialPrograms-Settings.json` (identical SHA-256
be2e225b...). The new #21 option "Auto Battle Quest - Heal Below HP %" is not in the copied file, so it uses the default of 50.

```sh
/Users/difan/Projects/Arduino-Source-r4/build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms --profile bbq-r4
```

The first launch of a new binary shows camera/microphone prompts.
