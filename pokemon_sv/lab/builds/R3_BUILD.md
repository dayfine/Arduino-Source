# Lab build R3: M9a Auto Battle + M14 BP status

Built 2026-10-22. Not launched.

## Source

Worktree `/Users/difan/Projects/Arduino-Source-r3`, local branch `lab/r3` (not pushed).

| Component | Exact revision | Lab integration |
| --- | --- | --- |
| Lab baseline `bbq/m7b-replan` (M7a/M7b stack) | ea047a9309ce8c429d288cbfb77f88266f32d34d | Branch starting point |
| M9a Auto Battle, fork #15/#17/#18 (`origin/bbq/auto-battle-bounds`) | 2f490c4f60d5244a8c8634c34058d5236601282b | Merge c6f9b9dd63f21dab9edc2dd621f1d21c1a150b39 |
| M14 BP status, fork #16 (`origin/bbq/bp-status`, on `bbq/bp-reader-crop` 486c0da40) | 4360d4e12c4d3f5da3d60d402f89652a7d61b6ff | Merge 59fa368a6a90cc4c47ccdfaeca89bb3b91e61faf |

**Tested integrated SHA: 59fa368a6a90cc4c47ccdfaeca89bb3b91e61faf**

`bbq/auto-battle-bounds` is based on a newer upstream main (64a811170 .. 4706977d0:
audio pipeline sink changes #1655/#1658/#1665/#1668, OCR weighted Levenshtein #1660,
Qt removal #1656/#1657, CatchFanfareDetector #1666). Those come along in this build.

## Conflicts resolved

1. Merge of #15/#17/#18, `PokemonSV_Tests.cpp`: kept both include lists, the Auto
   Battle helpers and tests, and the lab's BBQ tests. The Auto Battle block closes
   its anonymous namespace, so `namespace{` is reopened before the lab's
   `TeraEncounterScenario`. All registrations kept (BBQuestPlan, BBQReroll,
   BBQFarmerRounds, BBQAutoBattle).
2. Merge of #16, `PokemonSV_Tests.cpp`: kept both sides' registrations
   (BlueberryPoints frame tests + BBQPointsLedger).
3. Merge of #16, `PokemonSV_BBQSoloFarmer.cpp`: **ported, not a plain merge.** #16
   targets the old inline loop in `program()`; the lab runs `run_bbq_rounds()`
   behind `BBQSoloFarmerGameActions`. The ledger moved into that adapter:
   - `read_quests()` override passes `&BP` to `PokemonSV::read_quests()` and calls
     `record_BP(BP)` (same screenshot, no extra button presses).
   - `reroll_quest()`'s panel read does the same (#16 had no such read; recording
     it gives the balance just before the reroll's cost).
   - After `run_bbq_rounds()` returns, `program()` calls
     `game.record_BP(read_BP(...))` once, as #16 does after its loop.
   - Stats (`BP Earned`, `BP Spent`, derived `Net BP`, `BP/hour`) unchanged from #16.
   Panels reused by M7a (no new read) are not recorded, so there are fewer BP
   samples than reads in #16's loop; the net is unaffected.

All touched source files are CRLF, no tabs.

## Build

Mirrors m7b's `build-prep/app` (CMakeCache compared).

```sh
cd ~/Projects/Arduino-Source-r3
PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig \
cmake -S SerialPrograms -B build-prep/app -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH=$HOME/Projects/Arduino-Source/.cache/qt/6.8.3/macos \
  -DWrapOpenGL_AGL=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/System/Library/Frameworks/AGL.framework/AGL.tbd \
  > build-prep/configure.log 2>&1
nice -n 10 ninja -C build-prep/app -j6 SerialPrograms > build-prep/build.log 2>&1
```

Result: **PASS** (1477/1477, 0 errors). Logs: `configure.log`, `build.log`.

### Workarounds

- **OpenCV:** Homebrew `opencv` is v5; `PKG_CONFIG_PATH` selects `opencv@4` (4.14), as in m7b.
- **AGL:** the default CLT SDK (`MacOSX.sdk` -> 26.4) has no AGL.framework, which
  causes `ld: framework 'AGL' not found`. m7b's CMakeCache had `WrapOpenGL_AGL`
  pointing at the 15.4 SDK's `AGL.tbd`; R3 sets the same path explicitly. The
  binary links `/System/Library/Frameworks/AGL.framework/Versions/A/AGL`, which
  exists at runtime, the same as m7b's binary.
- `-j6` + `nice` only to limit contention with the running m7b lab app.

### Bundle

Same layout as m7b (compared `find SerialPrograms.app -maxdepth 4` plus sibling
dirs: identical). Like m7b, Qt frameworks are not copied into the bundle; the
binary loads Qt 6.8.3 through LC_RPATH `~/Projects/Arduino-Source/.cache/qt/6.8.3/macos/lib`.
The post-build deploy copied `Resources/` (198 MB), `Firmware/` and `Scripts/`
next to the bundle in `build-prep/app/`, the same as m7b.

**Binary:** `/Users/difan/Projects/Arduino-Source-r3/build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms`
**SHA-256:** `4fc2ab2547c32e987418c6c94b66a137f663b7de6ff2888c4e5c6cd5d3e3a2a2`

## Native tests

m9a-style runner: `build-prep/link-native-tests.py` (m9a's script with
`build-m9a` -> `build-prep`) links `build-prep/bbq-native-runner.cpp` against the
app's objects. The filter covers `PokemonSV::BBQ*` (includes BBQuestPlan),
`PokemonSV::TeraEncounterAction*` and `PokemonSV::BlueberryPoints*`.

```sh
cd ~/Projects/Arduino-Source-r3
python3 build-prep/link-native-tests.py > build-prep/native-link.log 2>&1
./build-prep/app/BBQNativeTests > build-prep/native-tests.log 2>&1
```

Result: **76 passed, 0 failed** (`native-tests.log`).

| Group | PASS |
| --- | --- |
| BBQAutoBattle | 8 |
| BBQFarmerRounds | 22 |
| BBQPointsLedger | 6 |
| BBQReroll | 13 |
| BBQuestPlan | 14 |
| BlueberryPoints (frame OCR) | 4 |
| TeraEncounterAction | 9 |

## Launch (not done)

```sh
/Users/difan/Projects/Arduino-Source-r3/build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms --profile bbq-r3
```

A new profile starts with default settings under
`~/Library/Application Support/SerialPrograms/Profiles/bbq-r3/`; copy/edit
`UserSettings/SerialPrograms-Settings.json` before launch per CONTROL.md. The
first launch of a new binary shows camera/microphone prompts.
