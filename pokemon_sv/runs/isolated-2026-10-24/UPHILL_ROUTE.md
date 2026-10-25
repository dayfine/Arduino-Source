# Central uphill route — next implementation checkpoint

Coding update: [UPHILL_BUILD_READY.md](UPHILL_BUILD_READY.md) records the implemented
bounded ascent/guarded Central flight/reanchor loop, native tests and signed
build at `be6f865de`. Route geometry and return remain unvalidated live.

User map: evidence/user-uphill-route-arrow.png, copied from Desktop
截圖 2026-10-24 下午7.48.49.png. User chose a candidate grassy corridor northeast
from West Province Area One Central to the south bank of the river bend, west
of the bridge/tributary. On ascent: high rock wall to the left, lower terrain
and drop-offs to the right. Do not cross the river or bridge. Leave a buffer
before the bank and a margin from the wall.

## Updated decision: fly back instead of walking downhill

User proposed replacing the return walk with a flight to the nearest fly point.
Prioritize ascent -> guarded flight back to Central -> re-align -> next ascent.
A downhill route is no longer a prerequisite. Measure flight overhead in the
next live test; do not claim it is faster before measuring.

Existing shared helpers in Programs/PokemonSV_WorldNavigation.cpp:
- fly_to_visible_closest_flypoint_cur_zoom_level: select closest visible icon to
  map center at current zoom, with FlyPoint::POKECENTER or FAST_TRAVEL filter.
- fly_to_closest_pokecenter_on_map: broader center search with zoom/retry logic.
These are map-visible candidates, not a global geographic nearest-point search.
Open/recenter the map on the player and verify Central's name before confirming;
never allow a different nearby center simply because it is closest visible.
The prototype's travel_to_isolated_hunt(..., from_zero_gate=false) already
combines nearest-center selection and the Central two-name-read flight gate.
Reuse it for end-of-ascent reset, retaining battle/refusal/shiny stop policy and
meal expiry checks. Test absent/occluded center, wrong name, failed landing and
arrival/reanchor failure. Flight-back from upper endpoint remains unverified.

## Alternatives discussed

1. Broad roaming (prototype four-leg square already exists, not a calibrated route).
2. Follow the dirt path (bends/drift require feedback).
3. Other movement patterns.
4. Vision-follow individual targets (more detection/steering work).
5. Uphill corridor with short movement + Lets Go pauses. Prioritize ascent
   followed by a verified flight reset to Central, per updated decision above. Heading feedback and
   periodic reanchoring are still needed; inverse timed inputs are not proof of
   returning to the same position.

## Exploration and limits

Lab viewer reopened, current signed prototype 313643338; PID 37887, profile
sv-isolated-live-20261024, program remains stopped. User authorized a route
walkthrough after the earlier coding-only request. Agent used on-foot pulses,
several initial camera adjustments, and later R between segments. Lower/middle
terrain was walkable without mount, jumping or climbing. Heading initially near
northeast, later needed correction beside the curved wall. There is no verified
controller sequence covering the complete route.

Fire meal had expired. Non-target wild battles interrupted movement: Rockruff,
Gastly, Capsakid and Flittle. Visually normal encounters were manually fled; this
was not running the hunter's continuous shiny/battle inference. No automated
shiny acquisition, species isolation, kill rate or spawn-refresh claim for this
traversal. User asked to clear with Lets Go like AZP; R was used in later segments,
but this was not an uninterrupted scripted clearing trial. Wait for actual
clearing progress rather than stepping through a lead still engaging targets.

A destination marker was set on the upper grass with a river buffer. It gives
a useful heading reference, but does not steer around terrain. Capture images
lagged behind short inputs, and some early screenshot calls returned stale files;
the helper was adjusted to wait for a new image. Do not transplant keyboard
pulse timings into C++ as a calibrated route. Input trace also includes map
cursor movements and battle-menu actions, not just forward travel.

User stated they walked the remaining section to the destination. Agent stopped
controller input after that message and saved endpoint evidence. Therefore the
full ascent was completed with user intervention; return remains untested.
Do not report an autonomous traversal or round trip. Offline inspection on the
next coding task found `uphill-user-completed-endpoint.png` contains capture
colour bars, not an endpoint view; it cannot prove arrival. The earlier live
observation of an empty Meal Powers panel is separate from that artifact. User owns live Switch; no
save, departure or farming handback has been performed. Lab stays open/stopped
for viewing, with exclusive device access. Do not send inputs absent a fresh
inspection and current user authorization.

Recordings: bounded observation loop ran 19:49:36–19:59:36, screenshots every
10s and 30s video exports every 20s, then exited. Later clips were saved manually;
there is a coverage gap in the exploration. Video files remain local; manifest
and selected screenshots are committed. No recording loop is left running.

## Reuse discovered — inspect before writing new navigation

- Programs/PokemonSV_WorldNavigation.{h,cpp}:
  walk_forward_while_clear_front_path already alternates R, a clearing delay and
  bounded forward segments. It does not swallow battles itself, so retain the
  hunter's outer battle/refusal watchers around it. Header comments still refer
  to old byte/tick conventions: inspect the actual signed-axis implementation
  and calibrate y=-1 for forward, rather than copying legacy 0/128/255 comments.
- Inference/Map/PokemonSV_DestinationMarkerDetector.{h,cpp}:
  detect_all_yellow reads the minimap marker; standard minimap ROI is
  {0.815, 0.645, 0.180, 0.320}. Replay on these screenshots first.
- Programs/AutoStory/PokemonSV_AutoStoryTools.cpp:
  confirm_marker_present, realign_player REALIGN_NEW_MARKER/REALIGN_OLD_MARKER,
  overworld_navigation with STOP_MARKER and CLEAR_WITH_LETS_GO already exist.
  Old-marker realignment opens/closes map, then L; comments describe the game
  aligning the player toward its marker. Verify this behavior live before
  relying on it. Marker-arrival watcher uses ROI {0.717,0.165,0.03,0.061}.
  Do not make ShinyHunting depend on AutoStory or import its generic battle-flee
  wrappers. Extract only minimal reusable alignment logic into a shared module,
  with includes/definitions and dependency direction consistent with rules.

## Next coding/test sequence

1. Native tests for route policy at an actions seam: bounded movement segments,
   Lets Go waits, marker missing/arrival, no progress, time/meal expiry, refusal
   and battle interruption; no further movement/reset after unresolved refusal.
2. Reuse shared advance-and-clear. Add explicit ascent/flight-reset/reanchor states,
   per-leg caps and drift/reanchor handling; no unlimited blind walking or
   four-direction square for this corridor. Keep fixed start and heading setup.
3. Reuse/extract marker alignment and log marker presence, leg, movement ms,
   detected defeats, no-progress count, correction reason and elapsed rate.
4. Offline replay marker detection on actual route screenshots. Build signed
   app, native tests and incremental lint. Keep helper outside signed bundle.
5. Next live run: verify new automatic power panel gate and Central candidate
   flight, align at lower corridor, test one ascent + guarded flight reset under current Fire
   Lv2, check drift and interruption recovery. Only then repeat loops and measure
   species-confirmed progress/fresh-spawn throughput. Preserve shinies/refusals.

At the exploration checkpoint, code was 313643338. See UPHILL_BUILD_READY.md for the subsequent implemented build. Logging, automatic split-column power
OCR and guarded automatic Central candidate selection are already built/pushed;
nine native groups, screenshot replay and lint passed. Corridor policy was not implemented at that exploration checkpoint. Latest prior notes: CENTRAL_PILOT_AND_AUTOMATION.md.
