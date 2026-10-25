# Bounded uphill corridor — code and signed local build

**Superseded checkpoint:** the Oct 25 pilot stopped during sandwich finish.
Recovery build and separate shared-routine safeguard are recorded in
[the Oct 25 run record](../isolated-2026-10-25/RESULT.md). No live corridor verification yet.

Coding-only task, 2026-10-24 PDT. No controller inputs, live program starts,
Switch commands, live tests, PR mutations or new scheduled jobs. No subagents.
The old lab PID 37887 was absent at fresh inspection. The unrelated installed
SerialPrograms instance was left untouched. The rebuilt app was not launched.

## Code and artifact

- Fork branch: `prototype/sv-isolated-hunt`.
- Final code: `be6f865defcf32feca5ee031ce13ea1c0ce52126`.
- Tree: `3f9518c9248a6922c0367d6494061e165dbc8a75`.
- Signed executable SHA256: `315d7d9eddc4acf7c41066670794e026ea98c270040240e0592bb98e777ef230`.
- App: `/Users/difan/Projects/Arduino-Source-isolated/build-prep/app/SerialPrograms.app`.
- Executable: app's `Contents/MacOS/SerialPrograms`.
- Build: `cmake --build build-prep/app -j4`, existing persistent-signing cache,
  certificate `PokemonAutomation Local Development`. Strict `codesign --verify`
  passes on both app and executable. Native helpers remain outside the bundle.

## Behavior

Full-hunt Central now uses bounded ascent segments instead of the four-leg
square. Initial Central corridor positioning remains supervised: stand on foot
at the lower corridor, set a destination marker with a river buffer, and accept
the path checkpoint. Later flight resets do not require walking downhill or
another supervised positioning acknowledgment.

Before each segment, require the destination marker, use the shared old-marker
map open/close + L realignment, and require a conservative minimap forward-cone
bearing gate. Marker presence/bearing is not a terrain or position guarantee.
The minimal shared `PokemonSV_MarkerNavigation` extraction is used by AutoStory
and ShinyHunting; ShinyHunting never includes AutoStory or its battle-flee wrappers.
The shared forward-and-clear helper is called with signed y=-1. Its legacy
byte/tick header comments were corrected without changing existing defaults.

Default limits: six segments per ascent, 600 ms forward each, 5000 ms clearing
wait. Configurable ranges are 1–30 segments, 100–2000 ms forward, 2000–15000 ms
clearing. These are conservative experiment caps, not a calibrated complete
route. Per-segment observation also waits for a two-second gap in detected
defeats, bounded by ten seconds and the remaining hunt/meal budget; persistent
recent defeat activity stops for inspection without resetting or departing.

Marker arrival or the segment cap triggers a guarded local Central re-flight
using `travel_to_isolated_hunt(..., false)`: closest visible center candidate,
two Central name reads, confirmation still visible, then overworld landing.
No global-nearest-center claim. Wrong/unreadable destination or failed landing
stops. Re-read the marker at the new anchor and realign before another segment.
A handled battle also requires flight reset/reanchor before continuing.

Continuous battle/refusal watchers cover navigation and observation, including
map alignment and flight stages. Refusal acquisition uses ZR and delegates to
the shared battle/shiny policy. An unresolved refusal, policy stop, battle
failure, missing marker, failed reanchor, no progress or clearing timeout leaves
the program stopped for inspection. A pending refusal takes priority over an
expired hunt budget. No reset or departure follows these safety stops.

The no-herbs default, actual automatic Meal Power OCR gate and existing meal
and hunt budgets remain. Deadline clamping is tested against the unlimited
startup sentinel to avoid signed-duration overflow. Accepted end-of-hunt
return stages retain their own bounded stage deadline and pending-event latches;
Central's final return to Zero Gate remains manual and uncalibrated.

Logs include stage begin/end, marker state, heading correction, leg, movement
cap, elapsed time, detected defeats, empty count, reset cause/count and final
rate. Kill progress is an audio inference count, not geometric route progress
or deduplicated spawn throughput.

## Verification

- Native TDD acceptance observed red then green: bounded segments/flight reset,
  zero-duration rejection, active-clearing stop, pending-refusal priority, and
  unlimited-deadline conversion. Logs are under `evidence/corridor-*`.
- Ten registered native groups pass: Power, Flight, Plan, Locations, Loop,
  Corridor, ExitPolicy, Options, Sandwich, EncounterOptions. Corridor cases cover
  marker loss/arrival, failed alignment, failed center/landing outcome, missing
  post-flight marker, no progress, time/meal expiry, refusal acquisition and
  battle transitions, and no reset/departure after safety stops. Flight group
  separately exercises wrong/missing/conflicting names, absent confirmation,
  unsupported language and failed landing through the actual Central gate.
- Incremental lint against `origin/main`: 51 changed C++ files, zero failures,
  one existing file-length advisory. Linter's own 22 tests pass. Changed source
  files retain CRLF and contain no tabs; whitespace check uses `cr-at-eol`.
- Native screenshot replay calls the real shared marker reader and bearing gate:
  `uphill-marker-heading.png` is PRESENT/forward; lower-corridor, Gastly
  interruption and endpoint-named screenshots are MISSING/not-forward.
- No full downloaded fixture suite or live navigation validation is claimed.
  Native helper linking emits an existing Homebrew Tesseract macOS deployment
  target warning; all native executions still exit zero.

## Evidence correction and remaining field work

`evidence/uphill-user-completed-endpoint.png` contains capture colour bars.
Despite its filename it is not visual proof of arrival, and is retained as a
missing-marker negative. The user reported completing the final ascent stretch;
that does not establish autonomous end-to-end traversal. There is no positive
actual arrival-notification screenshot in this replay set; arrival policy is
covered through the scripted native actions seam.

Still unvalidated live: automatic power panel flow and initial Central candidate
seek, map/L heading behavior, minimap bearing assumptions, center landing-to-
corridor geometry, drift/terrain margins, full uphill completion, upper-endpoint
flight reset, repeatable spawn refresh and flight overhead. The user owns the
Switch. Do not convert annotation/manual pulse traces into calibrated geometry,
claim a verified round trip, or start a field run without fresh authorization.
