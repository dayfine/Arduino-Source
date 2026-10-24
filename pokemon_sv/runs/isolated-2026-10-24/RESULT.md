# Isolated prototype controls — 2026-10-24

## Outcome

Release AZP restored at 01:40:56 PDT; first resumed kill 01:42:42. Saved at
Zero Gate, closed the prototype normally, reopened release v0.70.10 and
confirmed station traversal/healing and platform clearing. Release PID 96268.
No Chespin or Charcadet location, sandwich, shiny acquisition or sustained
hunting throughput was validated. Do not use these controls as a target benchmark.

## Controls

All three used the existing AZ platform, healthy level-100 Corviknight, no
sandwich, and stopped at three consecutive empty passes.

| Trial | Clearing interval PDT | Passes | Kill deltas | Result |
| --- | --- | --- | --- | --- |
| 1 | 01:15:58.843–01:17:07.715 | 5 | 2, 1, 0, 0, 0 | 3 kills; empty-pass stop |
| 2 | 01:20:53.023–01:21:34.339 | 3 | 0, 0, 0 | Empty-pass stop after small reposition |
| 3 | approximately 01:26:13–01:26:54.838 | 3 | 0, 0, 0 | Empty-pass stop after larger reposition |

Trial 1: 68.87 seconds, 2.61 detected kills/minute. Its final three empty
passes took about 41.3 seconds (60%). Arbitrary stationary viewpoints did not
sustain clearing. No refusals, automated encounters or shinies in these trials.
An Iron Bundle encounter occurred during manual repositioning while stopped;
it was escaped manually and is excluded from prototype encounter results.

## Setup and recovery observations

- A fresh profile lacked PA-Stats.txt; native stats saving failed until an empty
  file was created. Preserve first-trial evidence from logs; later saves passed.
- A rebuilt binary requested camera/microphone permission. Starting before video
  became ready produced a setup error. Grant permissions, wait for working video,
  then start. Not a hunt failure.
- Occasional frame/StreamHistory lag appeared; no demonstrated code regression.
- Manual map nudges were imprecise. Added a travel-only mode reusing
  return_to_outside_zero_gate(), live-verified arriving at 01:35:25. Its map nudge
  is up-left. Code commit ba454e4a8; built binary SHA-256
  89cb26f9ff7a9fb344f69d86857076c8d01c23b1c74b093dca4dc6df794030ce.

## Next experiment prerequisites

Implement target/location selection and sandwich preparation before borrowing
hardware again. Use existing shared navigation and sandwich routines, with
explicit start anchors and bounded experimental approaches. Verify the actual
spawn patch and camera, target purity, then acquisition and refresh movement.
Counters alone do not establish species purity or distinct shiny checks.

Raw logs/screenshots/private settings backups remain under
/Users/difan/Projects/Arduino-Source-isolated/build-prep/isolated-live-2026-10-24/.
