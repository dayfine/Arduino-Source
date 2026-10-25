# Isolated hunt: sandwich failure and recovery build

2026-10-25 PDT. **Code and signed build ready; live recovery has not started.**
The failed run did not reach flight selection or corridor clearing.

## Live evidence

- Failed code: `be6f865defcf32feca5ee031ce13ea1c0ce52126`.
- Lab run: 00:24:57–00:28:20, profile `sv-isolated-live-20261024`.
- The recipe completed: recorded result screen shows three stars and Fire Encounter Lv2.
- `finish_sandwich_eating()` queued twenty A commands at 00:27:56.188,
  each logging 4000 ms delay and 160 ms hold, then waited for all requests.
- At 00:28:20.957 it failed with
  `finish_sandwich_eating(): cannot detect picnic after 60 seconds.`
  Actual elapsed time in that routine was about 24.8 seconds. Native replay
  of the scheduler primitives produces 80,000 ms across 40 controller states.
- Recording shows bread/pick placement and eating still transitioning during
  this interval; the result screen appears near its end. Picnic was not yet
  visible, so this is not evidence of a missed visible picnic.
- No hunt rounds, kills or encounters occurred. Result: setup failed.
- Why the hardware command wait completed early is **not established**. Do not
  call this a firmware fix or a fully reproduced controller defect. A missing
  wait-for-all is ruled out by the real templated `run_until` and the live log.
- Contact sheets in `evidence/` show the transition and power result. Original
  MP4s remain under the profile's `Screenshots` directory, notably
  `video-20261025-002820452000.mp4` and `video-20261025-002840650042.mp4`.

## Separate code commits

Branch `prototype/sv-isolated-hunt`, pushed to the fork; no PR opened.

1. `9f2416f02`: existing-sandwich resume from Zero Gate. It skips saving and
   making a sandwich, keeps automatic power verification, and continues the
   full flight/corridor flow. Exactly one enabled hunt is required. The user
   supplies a conservative remaining-power budget, default 300 seconds. The
   budget includes Zero Gate realignment, verification, travel and supervision;
   it does not assume the old meal has another 29 minutes. **The in-game timer
   is not OCR-read.** Default fresh-start behavior is unchanged.
2. `282e9b3dc`: independent shared sandwich timing safeguard. Replaces reliance
   on a batch of completed commands with one confirmation per four-second
   real observation window, watching for picnic throughout, bounded by an
   80-second monotonic deadline. Error text now states 80 seconds. This affects
   all callers of the shared sandwich finish routine and is kept separate from
   the isolated program feature. The native scripted replay demonstrates why
   20 early-completing commands must not end observation at 24.8 seconds.

## Validation

- Native red observed for resume: attempted sandwich instead of verification.
- Native red observed for remaining budget: Zero Gate realignment was excluded.
- Native red observed for eating: false at 24,800 ms, expected picnic at 28,000 ms.
- Final 11 native C++ test groups PASS; see `evidence/native-tests.log`.
  Eating coverage includes early command completion, early and late picnic,
  missing picnic, and slow commands; resume coverage includes no ingredient
  consumption, missing power rejection, remaining-time accounting and corridor dispatch.
- Incremental health: 58 changed C++ files against `origin/main`, zero failures,
  one pre-existing file-length advisory. Linter tool's 22 self-tests PASS.
- Full CMake build PASS. Strict codesign verification passes for app and executable.
- Executable SHA256: `706c8c3ffe33d71b617476bde84c327292f1d3ae4dce7c0b69bd15c653db507a`.
- Live validation of the new finish routine and resumed corridor is pending.
  Do not spend another sandwich just to exercise the finish routine while this
  existing meal can still be used.

## Current state and next run

- User placed Switch on HOME to pause while coding. Fresh capture at 00:46
  confirms HOME with Violet running. No Switch inputs were sent during this fix.
- Old stopped lab PID 82912 closed normally before build. New lab PID 91612
  is open, stopped, with `Resume from Zero Gate with existing sandwich` selected,
  a 300-second power budget and the same Charcadet/Fire/Central row.
- Correct app is `Arduino-Source-isolated/build-prep/app/SerialPrograms.app`;
  stable lab signing identity retained. Controller and capture are selected;
  fresh capture succeeded without a new camera permission prompt observed.
- Bounded recording loop has been stopped using its `STOP_RECORDING` sentinel.
- Resume requires returning to the game, dismissing the sandwich results,
  packing up the picnic, and standing in the overworld at Zero Gate on foot.
  Check remaining meal time and set a budget below it with a safety margin.
  Then start. It still pauses at Central for the route/marker checkpoint.
- Full-run automatic return to Zero Gate and corridor calibration remain the
  previously documented live-validation limitations. No AZP farming handback
  was performed or claimed during this coding task.
