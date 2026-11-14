# AutoStory: dropped frames and time in frames

Design proposal, 2026-11-14. No code changes. Answers two linked questions from the FRLG 13.01 run on
console B (2026-11-14 01:26): do we have a methodology for dropped video frames, and should time be
measured in frames (useful for GBA / mGBA)?

Code references are to fork `main` for infra and to `lab/dev-frlg-stage8` (aab36d157) for the FRLG
walker and the shared `Pokemon/AutoStory` code; the failure-class list is on `autostory/fault-classes`
(3c3bd5866), owned by seam (c).

## 1. Findings

### What "StreamHistory worker thread lagging: Frame dropped" is

- The message comes from the stream-history recorder (the "last 180 s" video),
  `CommonFramework/Recording/StreamHistoryTracker_SaveFrames.cpp:238-241`: every source frame is
  queued for JPEG compression on a worker; when the queue holds `MAX_PENDING_FRAMES = 10`
  (`StreamHistoryTracker_SaveFrames.h:72`) the oldest queued frame is discarded and this line is logged.
- The recorder is one `VideoFrameListener` fanned out by `VideoSource::report_source_frame()`
  (`VideoPipeline/VideoSource.cpp:30-33`). Detectors never read that queue. They read
  `VideoFeed::snapshot_recent_nonblocking()` (`VideoPipeline/VideoFeed.h`), which serves the latest
  converted frame out of `QVideoFrameCache` (one raw frame plus a sequence number,
  `Backends/QVideoFrameCache.h`) via `Backends/SnapshotManager.cpp:239-271`.
- So "Frame dropped" is a CPU-pressure indicator, not a count of frames the detectors missed. It
  only says the recorder could not JPEG-compress 30 frames/s. lab-B records at 720p, 30 fps, JPEG 80
  (`Profiles/lab-B/UserSettings/SerialPrograms-Settings.json:209-215`), so under load the recorder
  itself is part of the load.

### Were the detectors starved at 01:26?

Partly slowed, not starved. From `Profiles/lab-B/SerialPrograms.log` around 01:26:

- Source frames kept arriving at 30/s (`VideoSource::report_source_frame(): Count = 1,800` per 60 s
  window at 01:26:49).
- Conversion to `ImageRGB32` was slow: `ConvertFrame: Mean = 39.6 ms, Max = 328 ms` at 01:26:41
  (nominal 0.5 ms; the frame period is 33 ms), and it got worse after the failure (167 ms mean,
  1.9 s max at 01:27:12).
- The step's watcher was polled almost at its nominal rate: `SelectionDialogWatcher: Count = 98` over
  the 5.2 s wait (01:26:42.53 to 01:26:47.69), against a maximum of about 103 polls at the default
  50 ms inference period (`CommonTools/Async/InferenceRoutines.h`, `default_video_period = 50ms`).
- But `Count` is polls, not distinct frames. `VisualInferencePivot::run()`
  (`CommonTools/InferencePivots/VisualInferencePivot.cpp:94-106`) asks for a frame no older than the
  callback's last one and calls `process_frame()` even when it gets the same frame again
  (`SnapshotManager::snapshot_recent_nonblocking()` returns the cached frame whenever its timestamp is
  not older than `min_time`, which includes equal). We have no number for how many distinct frames the
  watcher saw. Detectors see only `VideoSnapshot::timestamp` (`VideoFeed.h`); the sequence number stays
  inside the cache.

### The 01:26 failure was not a lag failure

- The same step failed the same way at 2026-11-13 19:17:17 with zero "Frame dropped" lines in the
  window and `ConvertFrame: Mean = 0.5 ms`. `press switch north to PokemonMansion_3F (12, 6)` has run
  twice (19:17 and 01:26) and never passed.
- `press_switch()` (`VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.cpp:728-742`)
  presses A once, then `wait_until(console, context, SWITCH_PROMPT_TIMEOUT = 5000ms, {SelectionDialogWatcher})`.
  The 01:26 interrupt frame (`DebugDumps/AutoStory/route/20261114-012648123858-interrupt-a_message.png`)
  shows "A secret switch!" with the page-advance arrow: the game waits for a button press before the
  "Press it?" Yes/No box appears, so the selection watcher can never fire. That is a step-logic bug
  (A, message page, A, selection), not timing. At 19:17 the failure screenshot shows the player beside
  the statue rather than in front of it (position), and no dialog at all. The lag at 01:26 was a
  coincidence: agents were compiling on the Mac (3,224 drop lines in the 01:xx hour vs 19 in 18:xx).
- Separately, a real lag casualty exists in the same log: `[Screen] before fly_pallet: no frame seen`
  at 2026-11-13 16:24:52 with 254 drop lines in the preceding 13 s (two more at 16:53/16:54). That is
  the false failure `~/BDSPLab/heavy_gate.sh` cites in its header.

### How the settle rule counts time today (shared `Pokemon/AutoStory`)

- `judge_settle()` (`Pokemon/AutoStory/Pokemon_StoryInterrupts.cpp:105-122`) sets `seen = false` when
  the watcher's last frame timestamp is older than the window start ("no frame seen"), and computes
  `quiet` as wall-clock from the last screen change to the window end. Under lag a window with two
  frames 3 s apart reads as "quiet 3 s"; a window with none reads as a failure. Neither says how many
  frames the verdict rests on. `clear_late_interrupts()` (`Pokemon_Interrupts.cpp:161`) uses the same
  `last_frame()` clamp.
- Timeouts are milliseconds everywhere: `wait_until`/`run_until` take `std::chrono::milliseconds`
  (`InferenceRoutines.h:42-48`); the PLZA AutoStory tree has 355 `ms` literals and 6 `wait_until`,
  BDSP 581 and 41, the shared `Pokemon/AutoStory` 48 (`TALK_OVER_QUIET = 5000 ms`, `DOOR_QUIET = 1000 ms`,
  ...); FRLG `SWITCH_PROMPT_TIMEOUT`, `SURF_PROMPT_TIMEOUT = 5000 ms`, `GATE_MESSAGE_WAIT = 1500 ms`
  (`PokemonFRLG_OverworldWalk.cpp:48-55`). Controllers are wall-clock too: `Controller::ticksize()`
  returns `Milliseconds` (`Controllers/Controller.h:68`).

### Where frames are already a unit

- FRLG RNG counts game frames and converts at the edge: `FRLG_FRAMERATE = 59.999977`,
  `FRLG_FRAME_DURATION = 1000 / FRLG_FRAMERATE` (`Programs/RngManipulation/PokemonFRLG_RngCalibration.h:22-23`),
  delays computed in frames then multiplied into ms (`PokemonFRLG_RngCalibration.cpp:155-158`,
  `PokemonFRLG_RngPlan.cpp:140`, `PokemonFRLG_EggRng.cpp:567`). `EliteFourFarmer.cpp:407-411` has its own
  `FRAME_MS = 16.74` and `frames_to_ms()`; `AutoStorySlope_Tests.cpp:190` uses `1000 / 59.7275`. Three
  frame constants for one game (NSO at ~60 Hz vs the GBA's 59.7275 Hz).
- The mGBA notes (`~/BDSPLab/notes/2026-11-13-mgba-design.md` section 5 "Timing model") keep
  milliseconds in programs: mode R (real time) converts at the controller boundary with
  `ticksize() = 17ms`; mode F (frame-locked fast-forward) needs a clock seam in infra (`WallClock::now()`
  is used directly in `wait_until`, the inference loops and `context.wait_for`), which the note calls a
  large upstream-grade change. The doc review lists "describe time in frames?" as open item 5.

### CPU contention guard

`~/BDSPLab/heavy_gate.sh` waits while any lab profile's `Lab/state` says `RUNNING`, then runs the
command under `nice -n 15`. It is opt-in: an agent that runs `ninja` directly bypasses it, which is
what happened at 01:26. It also cannot slow a build that was already running when the console run
started.

## 2. Proposal

Tested against the working proposal from the brief; where it changes it, the finding above is cited.

1. **Button presses, holds and movement tables stay in milliseconds.** Agreed. The controller is
   wall-clock (`ticksize()` in ms), the walker's speed tables are measured in ms on NSO, and FRLG RNG
   already converts frames to ms at the program edge. mGBA converts at its boundary (mode R now, a
   clock seam later), as the mGBA note recommends.
2. **Timeouts stay in milliseconds; evidence is counted in frames.** This is where the working
   proposal ("waits become N fresh frames with a wall-clock cap") is wrong in emphasis. Game events
   take wall time regardless of what we capture: a fade is 500 ms whether we see 15 frames of it or 2.
   Replacing the budget with a frame count would make every wait longer under lag without making the
   verdict better. What lag breaks is the *verdict*: "quiet 5 s" and "nothing showing" from too few
   frames. So a wait keeps its ms deadline and additionally records how many distinct frames its
   watchers saw; a verdict that needs evidence (settled, quiet, unseen) requires a minimum number of
   distinct frames spanning the quiet period, and when the deadline passes with fewer than that the
   result is "inconclusive: starved", a different outcome from "unseen" or "timed out". The deadline
   may self-extend once, by a bounded amount (for example up to 2x), while the watcher is below its
   minimum frames; the hard cap stays. On mGBA mode F a distinct frame is an emulated frame, so the
   same rule is exact there.
3. **Distinct frames are counted by `VideoSnapshot::timestamp`, not by a new sequence number.**
   `VisualInferencePivot` already tracks `callback.last_timestamp`; a watcher or a thin wrapper can
   count timestamps it has not seen before. No infra change for v1. Exposing the cache's sequence
   number on `VideoSnapshot` is cleaner but is an upstream-grade `CommonFramework` change; defer.
4. **Watchers start before the action and keep watching through it.** Agreed, and the primitive
   exists: `run_until(stream, context, command, watchers)` (`InferenceRoutines.h`) runs the command with
   the watchers live. Steps written as press, `wait_for_all_requests()`, `wait_until` (the shape of
   `press_switch()` and `start_surf()`) miss anything that appears during the press. The 13.01 fix is
   still logic (watch for the message page, press A, then the selection), but it should be written with
   `run_until` so a late page counts.
5. **Each step failure logs its frame evidence, and the lag case gets its own failure class.** Log,
   per step: distinct frames seen by the step's watchers, polls, `fps_source()` (`VideoFeed.h`) and the
   last `ConvertFrame` mean. Do not log "frames dropped": that is the recorder's counter, and it is
   already in the log. Ask seam (c) to add one row to `StoryFailureClass`
   (`Pokemon/AutoStory/Pokemon_StoryFailureClasses.h`): id `frames_starved`, meaning "the step's
   watchers saw too few distinct frames to judge", detected by "distinct frames below the step's minimum
   at the deadline, with fps_source normal", retry `RETRY_STEP`. The 16:24 "no frame seen" failures
   belong there, not under `UNSEEN_STEP`.
6. **Builds go through `heavy_gate.sh` while a console run is active, enforced rather than
   requested.** Agreed, plus: put a `ninja` wrapper first on PATH in agent build trees (the launcher
   the lab builds already use) so the gate is not opt-in; have the autostory-code agent definition name
   it; and during agent build windows set lab-B stream history to 15 fps (`VIDEO_FPS` in the settings
   above) to halve the recorder's own JPEG load. Measure drop lines per hour before and after.

What this does not do: it does not change `wait_until`, `InferenceSession` or the video pipeline. All
of it lives in program code (`Pokemon/AutoStory`, the per-game walkers), where the quality bar is ours
and Qt is not involved.

## 3. First slice

One PR on the shared `Pokemon/AutoStory` code, via /tdd with the FrameReplay sim
(`Pokemon_FrameReplay.h`), red first:

1. `ScreenSettle` gains `frames` (distinct frame timestamps inside the window) and `judge_settle()`
   fills it from the event log's recorded frame times. `describe()` prints "quiet 5.0 s over 12 frames".
2. `is_settled()` requires `frames >= MIN_SETTLE_FRAMES` (start at 6 for a 1 s quiet window; one value,
   in the knobs next to `TALK_OVER_QUIET`). Below it the verdict is `starved`, which the interrupt
   loop treats as "look again" until the hard cap, then fails with the per-step frame line.
3. Red tests: a replay with frames every 33 ms settles as today; a replay with two frames 3 s apart
   does not settle and reports starved; a replay with no frame in the window reports starved, not
   "no frame seen". Fault injection: a `FaultClass` row that thins the replayed frames (seam (c) owns
   the list; the test can thin frames locally until the row exists).
4. The 13.01 `press_switch()` fix is a separate FRLG PR (message page, A, selection, written with
   `run_until`), since it is a logic bug independent of this design.

Later slices: the per-step frame log line in every walker; the `frames_starved` row; the ninja
wrapper for agent trees.

## 4. Open questions

1. Add a sequence number to `VideoSnapshot`? Recommended: no for now; timestamps identify frames well
   enough, and the pivot already dedupes on them. Revisit with the mGBA clock seam.
2. Express timeouts in frames? Recommended: no. Milliseconds stay in programs; frames count evidence;
   mGBA converts at its boundary.
3. Which FRLG frame rate is right, 59.999977 (RNG calibration, NSO) or 59.7275 (GBA, slope tests,
   mGBA)? Recommended: one constant per platform in `PokemonFRLG_RngCalibration.h`, the NSO value for
   console runs and the GBA value for mGBA, and retire the `EliteFourFarmer` copy.
4. Should AutoStory watchers poll faster than the 50 ms default (20 Hz against a 30 fps source)?
   Recommended: measure distinct frames per second with slice 1 first; change the period only if the
   settle verdicts are frame-limited at idle.
5. Lab-B stream history during runs: keep at 30 fps, drop to 15, or disable while agents build?
   Recommended: 15 fps during agent build windows; keep 30 fps for runs the user watches.
6. Who enforces the build gate? Recommended: the harness (the ninja wrapper and the agent definition),
   owned by the agents branch; the main session also checks `Lab/state` before dispatching a build.
7. Name and retry policy of the lag class? Recommended: `frames_starved`, `RETRY_STEP`, owned by seam
   (c); this doc does not edit the list.
