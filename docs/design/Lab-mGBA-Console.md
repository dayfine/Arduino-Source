# Lab: mGBA as a console for FRLG

Design, 2026-11-14. Accepted by the user on 2026-11-14 ("accepts recommendations"): every open question
below is now a decision. No code changes. The user decided to go ahead with mGBA: the official build
0.10.5 runs at `/Applications/mGBA.app` (the Homebrew build crashes), and the user has loaded a FireRed
ROM locally. This doc says how SerialPrograms drives it (buttons in, frames out, one frame clock), where
it plugs in, and the slices to build it.

## First action: step 0 (user)

Before slice 0 can be accepted, the user loads the bridge by hand once per mGBA launch (0.10.5 has no
`--script` flag):

1. Open `/Applications/mGBA.app` and load the local FireRed ROM (File > Load ROM).
2. Tools > Scripting, then File > Load script, and pick `lab/mgba/bridge.lua` from a checkout of `agents`
   (added by slice 0).
3. The scripting console prints the port it listens on; the smoke client (slice 0) connects to it.

Inputs:

- `~/BDSPLab/notes/2026-11-13-mgba-design.md` (options A/B/C, the throwaway spike, the user's answers of
  2026-11-13) and `2026-11-13-mgba-geometry-gate.md` (nearest-neighbour compose passes 448 of 456 FRLG
  image tests). This doc supersedes the integration and timing parts of that note. Its findings still
  hold.
- [AutoStory-FrameTiming.md](AutoStory-FrameTiming.md) (#480): programs keep milliseconds; mGBA converts
  ms to frames at its boundary; one frame-rate constant per platform (GBA: 59.7275 fps).
- `docs/design/Agent-Integration.md` on `lab/tooling`, "Decisions": the mGBA controller and video work
  stays fork-only, behind the Lab/Developer boundary, with no change to the shared scheduler or clock,
  one connection, one controller and one video source.
- FRLG AutoStory on `origin/frlg/stage20-league` (read only): `GameSettings::DEVICE`
  (`switch_1_2`, `dev_test`, `custom`) and `GAME_BOX` in `PokemonFRLG_Settings.h`; the program entry
  takes `SingleSwitchProgramEnvironment&` and `ProControllerContext&` (`PokemonFRLG_AutoStory.h`);
  `Pokemon/AutoStory/Pokemon_FrameReplay.h` replays a folder of frames named by their ms offset through
  the real detectors.

## ROM rule (user, 2026-11-14)

The ROM stays on the user's Mac. Nothing in this design copies, commits, uploads or publishes it, or a
hash of it.

- The ROM is not a dump of the user's own cartridge; the user decided to move forward with it anyway
  (user 2026-11-14). Because of that it stays strictly local: never copied, committed, uploaded or
  named in a record.
- The ROM path comes from a local config file outside every repo:
  `~/.config/pokemonautomation/mgba.json`, for example `{"rom": "<path>", "instances": {"emu-1": 47801}}`.
  Only the lab runner reads it. It hands the path to mGBA on the command line.
- SerialPrograms never sees the ROM path. It knows only `127.0.0.1:<port>`.
- Save states and `.sav` files live under `~/FRLGLab/mgba/` and are never committed. States are keyed by
  a local label from the config (e.g. `fr-en`), not by a hash.
- The bridge's `HELLO` reply carries the game code from the cartridge header (`BPRE`) so the runner can
  refuse a wrong ROM; logs print the code, never the path or a hash.
- CI never needs the ROM or mGBA. App-side tests run against a fake bridge server
  (`Common/Cpp/Sockets/ServerSocket`); bridge tests that need the game run only on the user's Mac.

## 1. How SerialPrograms talks to mGBA

Three ways in, checked against what 0.10.5 actually ships (`strings` on its binary: Lua 5.4.7 with
`socket` `bind`/`listen`/`accept`/`receive`/`send`/`poll`/`hasdata`, `setKeys`/`addKey`/`clearKey`,
`currentFrame`, `frequency`, `frameCycles`, `screenshot`, `saveStateFile`/`loadStateFile`, `readRange`,
`callbacks:add` with `frame` and `keysRead`. It has no `--script` flag and no `screenshotToImage`; both
are 0.11-only.)

| | Lua bridge (scripting + socket) | GDB stub (`-g`) | libmgba linked into SerialPrograms |
|---|---|---|---|
| Buttons in | `emu:setKeys` from the `keysRead` callback: exact frame | no input path; KEYINPUT is set by the core each frame, memory writes do not stick | `core->setKeys`: exact |
| Frames out | 0.10.5: `emu:screenshot(path)` (PNG on local disk); 0.11: `screenshotToImage`, in memory | none | `setVideoBuffer`: zero-copy |
| Frame clock | `emu:currentFrame()` per frame | CPU halts per packet; no frame counter | `runFrame` count |
| Save states | `saveStateFile`/`loadStateFile` with flags | none | `saveState`/`loadState` |
| Determinism | lockstep by stalling the frame callback (to verify, slice 0) | n/a | exact |
| SerialPrograms change | one socket client, a video source and a controller, all Developer-only | same plus a frame source it lacks | a native C dependency in CMake on three OSes, MPL-2.0 source duties for every binary, emulator crashes inside the app |
| Runs on the installed 0.10.5 | yes; the script is loaded once per launch from Tools > Scripting | yes | needs our own libmgba build |

**Decision: the Lua bridge.** It is the only option that gives buttons, frames, a frame counter and save
states with the official build the user already runs, and it keeps the app's side to one socket client
behind the Developer boundary. The GDB stub cannot feed input or frames; at most it could later read game
memory as a test oracle, and the Lua `emu:read*` calls already do that. Linking libmgba breaks the #131
constraints (a new native dependency in infra on every CI platform, licence duties, and not upstreamable)
for one gain, skipping a localhost socket that costs well under a frame.

The earlier note's option C, a headless libmgba host process speaking the same protocol, stays the later
path to windowless parallel runs. It lives outside the app, so choosing the Lua bridge now costs nothing
when C arrives: the app talks the same protocol to either.

### The protocol (MgbaLink v1)

One TCP connection per instance on `127.0.0.1`, owned by the app. Line-based text commands; replies are a
text line, with a binary payload only for frames. The bridge services the socket once per emulated frame,
from the frame callback.

| Command | Reply | Meaning |
|---|---|---|
| `HELLO 1` | `OK 1 <game code> <version> <mode>` | protocol version; `mode` is `paced` or `free` |
| `GRANT <frame>` | `OK <current frame>` | paced mode: the emulator may run up to `<frame>` (the clock, section 2) |
| `KEYS <frame> <mask>` | `OK` or `LATE <n>` | from frame `<frame>` on, hold GBA keys `<mask>` (10 bits) |
| `FRAME` | `FRAME <frame> <w> <h> <path>` (0.10.5) or `FRAME <frame> <w> <h> <bytes>` + RGB (0.11) | the latest finished frame and its number |
| `SAVE <file>` / `LOAD <file>` | `OK <frame>` | state with flags 31 (includes SAVEDATA; mGBA's load default 29 does not) |
| `RESET` | `OK` | hardware reset |

`KEYS` is the whole controller: the app sends absolute key states, never press/release events, so a lost
or late command cannot leave a key stuck. A command addressed to a past frame applies at once and replies
`LATE <frames late>`; the controller logs it. In paced mode this should never happen (section 2).

## 2. The frame clock

The emulated frame number `F` (`emu:currentFrame()`) is the only clock on the emulator side. Each frame
lasts `1000 / 59.7275 = 16.743 ms`. Following the FrameTiming decisions, programs keep milliseconds and
the MgbaLink boundary converts:

- **ms to frames** (controller, outbound): a command scheduled at app time `t` lands on frame
  `F0 + round((t - t0) * 59.7275 / 1000)`, where `(t0, F0)` is the pair recorded at connect or after a
  `LOAD`. A press of `d` ms holds for `max(1, round(d * 59.7275 / 1000))` frames, so 80 ms is 5 frames
  and 17 ms is 1. `ticksize()` reports 17 ms so the scheduler never asks for less than a frame.
- **frames to ms** (video, inbound): a frame's `VideoSnapshot::timestamp` is
  `t0 + (F - F0) * 1000 / 59.7275`, not the time the app received it. Frame identity is `F`; the
  distinct-frame counting of FrameTiming section 2 is then exact.
- **The constant** is the GBA entry of the per-platform constant decided in FrameTiming (decision 3),
  in `PokemonFRLG_RngCalibration.h`. MgbaLink reads it from there; no fourth copy.

**Paced lockstep (default).** The app owns the clock and the emulator follows it. A pacer on the
connection's pool thread sends `GRANT F0 + floor((now - t0) * 59.7275 / 1000) + lead`, with a lead of
2 frames, about every 8 ms. The bridge runs frames until it reaches the grant, then stalls in its frame
callback, polling the socket, until the next grant. mGBA's own audio/video sync is turned off for the
instance (`-C audioSync=0 -C videoSync=0`), so the grant is the only throttle.

Because the controller schedules its commands ahead of the grant, every `KEYS` lands on the frame the
program meant, independent of socket and thread jitter. That gives:

- **Deterministic input.** The run's input log, the start state plus the `(frame, mask)` list, replays
  bit-exactly in mGBA. A failed run becomes a reproducible case without the app.
- **No change to infra time.** The app still runs on wall clock; `wait_until`, the inference loops and
  `context.wait_for` are untouched. What stays non-deterministic is which frame a watcher happens to
  sample, so a live run can still take a different branch; only the virtual-clock seam (deferred by
  FrameTiming decision 1) removes that.
- **A starved app slows the game instead of losing frames.** If the app stalls, grants stop and the
  emulator waits; nothing the program would have seen is skipped.

**Free-running (fallback).** If slice 0 shows that stalling the frame callback freezes the mGBA window
or breaks audio badly, the bridge runs at mGBA's own 1x sync and `KEYS` apply at the next frame on
arrival (`mode free` in `HELLO`). Input is then as jittery as console B (about one frame), and the input
log still records the frame each change actually landed on, so replay stays exact.

Scaled time (run k times faster and divide durations by k) and full frame-locked fast-forward are out of
scope; both need the virtual clock.

## 3. Where it plugs in

All app code is fork-only, on a branch from `lab/tooling`, and appears only in Developer mode.

**Infra (one connection, one controller, one video source):**

- `Controllers/MgbaLink/MgbaLink_Connection.{h,cpp}`: the client over `Common/Cpp/Sockets/ClientSocket`,
  the pacer and the input log; work runs on `GlobalThreadPools`, with `Mutex`/`ConditionVariable`, no
  `std::thread`. Qt-free.
- `NintendoSwitch/Controllers/MgbaLink/ProController_MgbaLink`: maps the Pro Controller to GBA keys
  (A, B, L, R; Plus to Start, Minus to Select; D-pad, and the left stick to the D-pad past half tilt).
  X, Y, ZL, ZR, Home and Capture do nothing and are logged once. Programs keep `ProControllerContext`,
  so the FRLG program does not change for input. It registers a `ControllerInterface` entry hidden
  outside Developer mode; its selector widget is the only Qt.
- `CommonFramework/VideoPipeline/Sources/VideoSource_MgbaLink`: polls `FRAME` once per frame, composes
  240x160 into a 1920x1080 frame at the default `GAME_BOX` (x=180, y=5, 6.5x) with nearest-neighbour
  scaling, per the geometry gate, and reports it with the frame-clock timestamp. Every FRLG detector then
  sees console-B geometry.

Both the controller and the video source take the same connection from one descriptor (`host:port`), so a
console slot has one link to one emulator.

**Program (FRLG):**

- `GameSettings::Device` gains `emulator`. It skips the HOME and black-border check, the 720p assert and
  the NSO online license check; the existing `home_black_border_check()` already skips non-Switch
  devices.
- AutoStory on `emulator` takes the normal gifts instead of the RNG gift steps and never claims RNG
  results (user, 2026-11-13, answer 4).
- The movement mechanics table stays the console's until a measured mGBA run says otherwise (decision 3).

**Save states as checkpoint starts.** The lab runner (agents branch, `lab/mgba/`), not the program, loads
`~/FRLGLab/mgba/<label>/states/NN.MM.ss` through the bridge before it starts AutoStory with
`START=NN.MM`; the program's own `check_save_place` validates the start, unchanged. A state is written
when a checkpoint passes clean on mGBA. Each instance has its own directory with a link to the ROM, so
instances never share a `.sav`. Unlike console B there is no one-slot risk: any checkpoint can rerun from
its own state any number of times.

**Sim and replay.** The video source can write the frames of the last N seconds as a FrameReplay case
(`<ms>.png`, with `ms` from the frame clock), the format `Pokemon_FrameReplay.h` reads from
`CommandLineTests/<game>/FrameReplay/<case>/`. An mGBA failure then becomes a replay test with exact
frames and exact timestamps, which console captures (a 30 fps stream cut at 20 fps) cannot give. Frames
are game screenshots: they stay local like the console captures, linked by path. The input log goes in
the same folder, so the case can also be rerun in mGBA from its state.

**Lab control.** mGBA instances are not console B. The pa-lab daemon's allowlist stays B; mGBA runs go
through the runner until a later entry is designed (decision 5).

## 4. Rollout

Each slice is one small PR with its own acceptance.

| # | Slice | Where | Acceptance |
|---|---|---|---|
| 0 | `lab/mgba/bridge.lua` and the protocol text; a Python smoke client | agents branch | After step 0, on the user's Mac with 0.10.5 and the local ROM: `HELLO` returns `BPRE`; `KEYS` at frame F shows the key held from F (read back with `emu:getKeys`); a `GRANT` stall holds `currentFrame` still for 2 s with the window responsive; `SAVE`/`LOAD` round trip returns the same frame; `FRAME` cost measured. Numbers recorded in the PR. |
| 1 | `MgbaLink_Connection` + `VideoSource_MgbaLink` | fork branch from `lab/tooling` | Unit tests against a fake server: framing, `LATE`, compose geometry (nearest, x=180..1740), timestamps from frame numbers. Live: the lab app (own `--profile`) shows the mGBA picture. |
| 2 | `ProController_MgbaLink`, the pacer and the input log | same stack | Unit tests: ms-to-frames table (17→1, 80→5, 1000→60), button map, scheduled `KEYS` frames. Live: FRLG start menu opens and closes. |
| 3 | FRLG `Device::emulator` | FRLG fork stack | FRLG unit tests unchanged on `switch_1_2`. Live: AutoStory 00.x runs from a new game on mGBA. |
| 4 | Runner + state library | agents branch, `lab/mgba/` | One checkpoint runs twice from its state with a verdict each time; ROM path read only from local config. |
| 5 | Input-log replay and FrameReplay export | agents (runner) + app stack | Replaying a logged run twice from its state gives identical frame checksums; one exported case loads in `Pokemon_FrameReplay` tests. |

Later, separate designs: the headless libmgba host (earlier note, option C); the virtual-clock seam;
mGBA instances in the pa-lab daemon.

## 5. Decisions (were open questions; user 2026-11-14)

1. **Stay on 0.10.5.** The official build the user installed. The cost is one manual script load per
   launch (step 0) and frames through a PNG file. Move to a pinned nightly only if slice 0 measures
   `FRAME` at over one frame (16.7 ms) or launch automation becomes the bottleneck; the protocol is the
   same on both. (user 2026-11-14)
2. **Paced lockstep by default,** provided slice 0 shows the stall holds the emulator cleanly; otherwise
   free-running. Deterministic input is the main thing mGBA gives that console B cannot. (user 2026-11-14)
3. **No own movement table for mGBA at first.** Run slice 3 with the console table and add a per-device
   row to the mechanics module only for legs that fail, with measurements. (user 2026-11-14)
4. **Compose to 1080p with nearest-neighbour now** (zero detector change, gate passed); fix the eight lost
   text reads in their own PR. Native 240x160 frames wait for the four full-frame call sites to move to
   `GAME_BOX`. (user 2026-11-14)
5. **No pa-lab daemon entry before slice 4 works.** Then a separate allowlist entry per instance, with no
   console-B lease semantics. (user 2026-11-14)
6. **ROM revision: whatever the user loaded.** The runner checks the header game code and keeps the
   revision label in the local config. Detector or text differences against the NSO build are recorded as
   findings, not fixed blind. The ROM is not a dump of the user's cartridge; the user chose to proceed
   anyway, and it stays local (ROM rule above). (user 2026-11-14)
7. **No RNG gifts on mGBA.** Skip them, as answered on 2026-11-13; console B stays the acceptance run for
   RNG. (user 2026-11-14)
