---
name: bdsp-autostory-test
description: Test a BDSP AutoStory checkpoint or segment on the lab Switch (console B) - build, deploy, load the save, run, decide pass/fail from the log and screenshots, and report findings. Use when asked to test, re-run or verify BDSP AutoStory (segment 00/01 checkpoints, routes, battles) on the console.
---

# Testing BDSP AutoStory on console B

You run one test at a time on **console B** through a lab copy of SerialPrograms, read the
outcome from its log, and report. You do not fix code unless asked; you report what happened
with evidence.

## Hard rules (never break)

- **Console A is the user's (Max Lair, release app). Never touch it.** Reading its log is fine.
- Run `sh ~/BDSPLab/switch_guard.sh B` before any console action. If it says BUSY, stop and report.
- Never quit or kill an app that is running a program you did not start.
- Never press keys while a program is RUNNING; stop it first (`sh ~/BDSPLab/stopB.sh`).
- Do not use macOS accessibility (AX) tools on the lab app.
- **Stop at the first failure**, close the game (Lab Console `close_game`), report. No retries to
  "gather proof" of a failure you can already see; retries are only for real randomness.
- **Save policy:** follow the segment's phase in `autostory/SAVE_POLICY.md` /
  `autostory/segments.tsv` (testing never saves; verification saves once after 2-3 passes;
  reproduction saves per checkpoint). Check postconditions yourself after every run.
- **Saves:** a run may save over the console's save. Never start a **new game** (NEW_GAME_INTRO,
  start checkpoint 00.00) without the user's explicit OK in this conversation: it overwrites
  the only save. Note in the report which save the console holds afterwards.
- When idle, leave console B awake (Lab Console idle, or Prevent Sleep).
- PRs and pushes: follow `~/.claude/CLAUDE.md` (fork drafts only; never upstream).

## Paths

| What | Where |
|---|---|
| Code worktree | the lab tree in `~/BDSPLab/keep-worktrees.txt` (pass it as `BDSP_WT`; deploy requires it) |
| Lab scripts, builds, run outputs | `~/BDSPLab` (override: `BDSP_LAB`) |
| Lab profile (settings, log) | `~/Library/Application Support/SerialPrograms/Profiles/lab-B/` |
| Log | `…/lab-B/SerialPrograms.log` |
| Screenshots | `…/lab-B/Screenshots/AutoStory/{failures,saves,route}/` |
| Lab Console outbox (shots, `.out`) | `…/lab-B/LabConsole/outbox/` |
| Design docs (plans, lessons) | `~/Projects/Arduino-Source-autostory/docs/design/AutoStory-*.md` |

## Steps

0. **Lab tree = code + lab tooling.** The lab tree is the code branch with `origin/lab/main` and `origin/lab/tooling` merged on top locally (never pushed). Without lab/tooling the app has no LabControl: `labctl.sh supports <build>` fails and `lc.sh` hangs. Check `supports` after every deploy.
1. **Build and unit-test**
   ```sh
   cd $BDSP_WT && ninja -C build-reorg SerialPrograms
   PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig python3 ~/BDSPLab/bdsptests/link-v2.py && ./build-reorg/BDSPNativeTests
   ```
   All tests must pass (they include offline route-plan checks). A failing test is a finding;
   do not go to the console.
2. **Deploy** under a new run name (`bdsp-rNN`, next free number in `~/BDSPLab/builds`):
   `sh ~/BDSPLab/bdsp_deploy.sh bdsp-rNN` - prints the commit; `+dirty` means uncommitted code.
3. **Choose what runs.** The program uses the lab-B profile's AutoStory settings. Set them with
   `PANEL_SET` (JSON merged into `99-Panels["PokemonBDSP:AutoStory"]` while the app is closed):
   - segment 00 checkpoints: `ENABLE_ADVANCED_MODE: true`, `START_SEGMENT`/`END_SEGMENT: "0"`,
     `START_CHECKPOINT_TUTORIAL` / `END_CHECKPOINT_TUTORIAL`: `"0"`..`"5"` (00.00..00.05)
   - `DIAGNOSTIC_POST_CATCH_ONLY: true` with start `"4"`: start 00.04 at the post-catch position
   - `NEW_GAME_INTRO`: only with the user's OK (see rules)
   - `HELP_MODE: "no_outside_help"`, `STARTERCHOICE: "piplup"` (keep)
   - segment 01 options: added with segment 01's code; check `PokemonBDSP_AutoStory.cpp` for the
     option names and update this list.
   The save the console holds must match the start checkpoint (see the plan docs for which
   save exists now). If it does not, stop and report.
4. **Run and watch**
   ```sh
   PANEL_SET='{"START_CHECKPOINT_TUTORIAL":"5","END_CHECKPOINT_TUTORIAL":"5"}' \
     sh ~/BDSPLab/run_watch.sh bdsp-rNN rNN 'Completed: 00.05'
   ```
   It loads the save (Lab Console `bdsp_load_save`), starts AutoStory, waits for the regex or a
   failed attempt, stops on failure, prints step/route/battle lines and `VERDICT: PASS|FAIL|UNKNOWN`.
   Run it in the background and wait for it to exit; don't poll. `CAP_MIN` (default 60) caps the run.
5. **Assert.** PASS needs all of:
   - `VERDICT: PASS` and the expected `Completed: <checkpoint>` line(s);
   - no `attempt N failed`, no `Exception`;
   - if the checkpoint saves: a new file under `Screenshots/AutoStory/saves/` - open it and
     check the location/play time make sense;
   - the end screenshot shows the checkpoint's end state (its `end_text()`).
   FAIL: any failed attempt or exception - take the message, the step it was in (last
   `[NN.NN step i/n]` line) and the newest `failures/` screenshot. Look at the screenshot
   yourself and say what is on screen (dialogue? battle? wrong place?).
   UNKNOWN: stopped without either - report as a failure of the test, with the log tail.
6. **Close the game** after a failure (Lab Console `close_game`, step below) and leave the
   Lab Console idle.
7. **Report** (format below). For a requested "run 2x", run twice, both must PASS.

## Stepping by hand (Lab Console)

For new ground or to look at a failure: start the Lab Console on B, then send command batches.
```sh
sh ~/BDSPLab/stage_run.sh bdsp-rNN lab-B rNN-lc B console     # starts and leaves it running
printf 'bdsp_load_save\nstate\nshot\n' | sh ~/BDSPLab/lcB.sh step1   # prints the .out
```
Send `help` for the command list. Common: `bdsp_walk x y ms` (stops on blocked / transition /
limit), `stick L x y hold release`, `dpad DIR hold release` (a short tap only turns),
`press A 100 1500`, `detect bdsp_dialog|bdsp_overworld|bdsp_menu`, `shot`, `close_game`.
Keep batches to 5 to 10 commands (15 to 30 s), each ending with `shot`; read the PNG from the
outbox before sending the next. Facts: 160 ms per tile across, 240 ms up/down; A answers
Yes and closes "obtained" lines; B advances lines but never starts a talk; read positions
from feet and ground features, not roofs or heads.

## Report format

Post the report where you were asked to (the conversation, or a PR comment on the
**fork** PR if told so). Start with:

```
🎯 Result: PASS|FAIL  <checkpoint(s)>  build bdsp-rNN (<commit>)  run rNN
Problem: <one line, or "none">
Next: <one line>
```
Then: the step list with times (from the printed lines), battles fought and HP/Potion lines,
the failure message and what the screenshot shows, the save state the console is left in,
and paths to screenshots (absolute). Add a finding to
`~/Projects/Arduino-Source-autostory/docs/design/AutoStory-Lessons.md` only when asked.

## If the build directory must be recreated

```sh
cd $BDSP_WT && PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig \
  cmake -S SerialPrograms -B build-reorg -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_PREFIX_PATH=$HOME/Projects/Arduino-Source/.cache/qt/6.8.3/macos -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DWrapOpenGL_AGL=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/System/Library/Frameworks/AGL.framework/AGL.tbd
```
(The macOS 26 SDK has no AGL framework; Qt 6.8 still links it.) `Packages` in the worktree is a
symlink to `~/BDSPLab/packages`.
