# Driving the app and the Switch from an agent

How Codex builds, launches and controls a lab instance, as of 2026-10-21. It
was checked from Claude's shell the same evening, read-only while a program
was running: Accessibility and Screen Recording are granted, the lab window is
visible, its button titles are readable, and window screenshots work.

## Build

- **One checkout per candidate**, e.g. `~/Projects/Arduino-Source-m7b`. The
  full arm64 app is built into `build-prep/app/` (CMake + Ninja), and the build
  output goes to `build-prep/build.log`.
- **Native tests** use `build-prep/bbq-native-runner.cpp` and
  `link-native-tests.py`, linked against the app's objects. Logs go to
  `build-prep/native-*.log`.
- **Record the binary's SHA-256** (`build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms`).
  Never infer the running revision from the source HEAD.

## Launch

- **Start it with its own profile:**
  `.../build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms --profile <name>`.
  Each profile keeps its settings, stats and log under
  `~/Library/Application Support/SerialPrograms/Profiles/<name>/`.
- **Settings:** edit `UserSettings/SerialPrograms-Settings.json` (UTF-8 with a
  BOM) before launch, and keep a copy as `settings-at-start.json` in the run
  folder.
- **Permission prompts:** the first launch of a new build shows camera and
  microphone prompts. `allow-capture` presses "Allow" ("允許") through
  Accessibility. Only do that when the user has said so.
- **Identity:** before any Stop or keypress, check that the process command
  line is exactly that app path plus `--profile <name>`.

## Control (tools in `build-prep/m7b-preparation/`, Swift)

- **`button <title>`** presses an app button through Accessibility. The titles
  seen are "Start Program...", "Stop Program...", "Screenshot", "Settings",
  "Reset Ctrl." and others. The lookup only goes 3 levels deep, which avoids
  the slow recursive helper that missed a handoff earlier.
- **`ui`** lists the lab window (PID, window number, bounds).
  - **`ui click x y`**, **`ui key <keycode> [ms]`**, **`ui scroll`** and
    **`ui drag`** post events.
  - Keys go to the app's controller mapping, so they press Switch buttons.
  - The app ignores keys while a program runs ("Keyboard Command Suppressed")
    and when its window isn't focused.
- **Pro Controller key mapping** (macOS key codes): A = Enter (36), B = Shift
  (56), X = `'` (39), R = E (14), Home = H (4). It's defined in
  `NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController_from_Keyboard.cpp`.
- **Screenshots:** `screencapture -x -o -l <window number> out.png` captures
  the lab window without touching it. The app's own "Screenshot" button saves
  the Switch frame at full resolution.

## Save the game by hand (no program running)

X (menu) → R (save) → A (confirm), then B until the overworld. Take a
screenshot after each press and check it before the next one.
`switch_key.sh <button> <name>` in Claude's scratchpad does one press plus a
screenshot.

## Run observer

`observe.py` in each run folder (e.g. `build-prep/m7b-reuse-live-2026-10-21/`):

- tails the profile log and writes `status.json` / `checkpoints.jsonl` every
  five minutes;
- at the deadline, waits for a safe point (a quest-panel read) or 10 more
  minutes, checks the identity, presses Screenshot and then "Stop Program...",
  and requires STOPPED within 25 s.

Deadline stopping had not been verified live as of 2026-10-21.

## Limits

- **One Switch.** Only one program can run at a time. Coding and native tests
  can run in parallel; live runs can't.
- **Hand over to the user's Flying Trial at a safe boundary.** Automatic
  stopping missed the handoff twice on 2026-10-21. When in doubt, ask.
