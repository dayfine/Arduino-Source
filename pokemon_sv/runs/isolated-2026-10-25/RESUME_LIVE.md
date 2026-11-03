# Existing-meal live recovery

2026-10-25 PDT, user authorized "ok take it". Lab profile
`sv-isolated-live-20261024`, existing ordinary Fire meal. No new sandwich made.

## First resume attempt, 282e9b3dc

- At Zero Gate packed up the picnic manually. Meal panel visually showed Fire
  Encounter Lv2, 22:41 remaining. Set up existing-meal resume with 300 seconds
  total power budget, including setup/travel.
- Automatic Zero Gate realignment and two-read active-power OCR succeeded.
- Candidate flight stopped before any confirmation: no visible Pokemon Center.
  Map showed Casseroya Lake, northwest of Zero Gate. The advertised southward
  seek used +1, which this controller encodes as up.
- Outcome: travel failed; zero ingredients, kills, encounters and hunt rounds.

## Corrections and second resume attempt

- Separate shared documentation commit `3ca0fd142`: signed Y +1 is forward/up,
  -1 is backward/down. Existing helper default was already +1. The newly added
  comment incorrectly claimed the reverse.
- Isolated-program commit `2c7bd659e`: seek south with -1; ascend forward with
  +1. Native 11 groups and signed full build PASS. No source-text tests added
  for two literal direction corrections; live map selection is the integration check.
- Automatic seek reached Central. OCR read `西第1區：中央` twice at
  01:06:48.327 and .624 before confirming; landed at 01:06:56.880.
- Supervised path setup timed out at 01:08:47 after 110 seconds. Most initial
  keyboard events had focused a settings widget because this reopened window's
  video had moved down the viewport. No automated ascent ran. Do not report
  corridor validation from this run.
- Window 257412, PID 95333: logical (800,650) focuses the visible portion of
  the video; (800,450) was a settings field. Inspect current UI instead of
  reusing coordinates across layout changes. Full-frame Screenshot still works.
- A PID/path-guarded private checkbox helper can identify the otherwise unnamed
  path checkbox by the matching label and screen Y. Never acknowledge terrain
  inspection before visually checking the starting point and marker.
- Prepared marker on the correct grass strip south of the river bend, west of
  the bridge, with a river buffer; matches the user's Oct 24 arrow.
- Manually flew to name-verified Zero Gate to retry the full resume flow. The
  marker remains. Flight to Zero Gate from Central: recenter/open map, one zoom
  out, roughly east 950 ms / north 650 ms, adjust to the visible Zero Gate icon,
  verify `零區閘口` before confirming. These are supervised observations, not
  an implemented autonomous return route.

## Current attempt

Third Zero Gate resume attempt read North and correctly stopped before flight.
Initial seek is not reliable yet; icon occlusion/map view remains to investigate.

Further separate commits:
- `4f13d4fd1`: existing-meal resume at anchor, native red then green.
- `a7de93374`: local cursor offset to expose fly icon; live trial still found no
  Center because player/Pokemon overlays obscure it. This is not a verified fix.
- `11fd32106`: initial anchor resume skips flight and requires the supervised
  Central terrain/marker checkpoint. Only Central is supported. Later reset
  flights retain the name gate and remain unverified.

Final test: `11fd32106`, signed executable SHA256
`f4abc27f026930f2489e165a6cde6507422feac288c0de7fc670feb5fb4e1f82`.
11 native groups PASS and incremental health PASS. Code pushed to fork.

Manually returned to name-verified Central, moved to the lower grass/path,
set the buffered marker and handled a visually normal Charcadet encounter by
fleeing. This encounter occurred during manual setup, outside the hunt counters.
Final program start around 01:32:16, active Fire Lv2 verified. Accepted supervised
path checkpoint, then automated marker corrections and two 600 ms movement
segments. No new sandwich or herbs consumed.

At 01:33:05.042 and 01:33:07.722 the audio tracker detected defeats; Current Run
reported 2 kills. At least one game toast explicitly names Charcadet. Result
summary reports only 1 because the second occurred during reanchoring, outside
its per-clear-round delta. This is a confirmed accounting gap; do not treat
sound detections as unique shiny checks or assume all defeats were Charcadet.

Program stopped at 01:33:26 with
`leave_phone_to_overworld(): Unknown state after 10 button Y presses.`, reported
as marker reanchor failed. Two rounds, zero program battles/refusals/resets,
34 seconds hunt time. Result's 1.76 kills/min is undercounted; tracker rate is
2/34*60 = 3.53/min, a very short sample including reanchor time. Movement was
only 1.2 seconds. Two successful map heading corrections took 7.86 and 7.66
seconds; third failed after 5.30 seconds. Avoid map round trips when the marker
is already ahead; measure this before claiming any throughput improvement.

The 01:33:32 video contact sheet still shows normal game frames after the
failure timestamp. A later fresh capture showed colour bars; user confirmed
undocking/taking over. Do not assume that later signal loss caused the earlier
map failure. No inputs were sent after observing colour bars. User then said
"ok you can stop now". Recording loops stopped; no further coding or live work.

Final state: lab PID 98887 open and program stopped; user owns the undocked
Switch. No return flight, HOME pause, save or AZP restoration after takeover is
claimed. No scheduled continuation was created.

Next work, when requested: count defeats across the whole hunt, avoid redundant
map heading corrections, reproduce the phone-exit failure with valid capture,
and handle hidden/occluded local fly points without weakening destination checks.
Shared sandwich-finish safeguard is still only native-tested, not live-verified.
