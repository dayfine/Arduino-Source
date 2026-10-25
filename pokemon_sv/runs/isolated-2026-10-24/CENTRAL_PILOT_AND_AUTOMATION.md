# Central pilot, user sweep and next build

## Live observations on prototype 5db7dfe1b

Full hunt started 19:02:16 PDT. Zero Gate reset began 19:02:18; sandwich/save
stage began 19:02:31 and ended 19:05:49 (198 seconds). Used ordinary 6 Rice,
2 Curry Powder, 2 Wasabi; no herbs. Actual powers screenshot confirms Fire
Encounter Lv2. Manual acknowledgment at 19:07:01. The supervised Central map
selection exceeded its 180-second window; program safely stopped 19:10:07 with
travel failed and zero clearing. This did not validate a complete full hunt.

Manual selection subsequently visibly identified 西第1區：中央 and flew there.
Several Charcadet were visible. A manual approach first hit a wall; retreat and
camera recenter reached grass beside the center. That approach is not a reliable
recorded route. The map seek also involved several corrections; no single input
sequence has been live-validated end to end.

Prepared stationary control started 19:13:46, verified existing Fire Lv2, and
cleared from 19:14:33 to 19:16:37: 9 passes, 3 detected defeats, 0 battles,
0 refusals, 0 shinies, 0 refreshes. It stopped on 3 consecutive empty passes,
well before the 300-second cap. About 1.45 detected defeats/minute over 124s;
not sustainable yield or unique shiny checks. A game notification explicitly
identifies one defeated Charcadet. The old prepared-mode UI incorrectly showed
Hunt Seconds=0; fixed in the next build.

## User demonstration

User took control and roamed around Central to inspect the footprint. Their
estimate was approximately 20 spawns while moving; not independently deduplicated
or a respawn-rate measurement. Frames show Charcadet along grass/path, low rock
mounds and changes of height, with battle entries near rocks. This supports a
larger accessible pool than the stationary control, not a calibrated waypoint loop.

Saved six 30-second 1280x720 video-only clips outside Git, manifest and hashes in
`evidence/user-demo-videos.json`. Mostly overlapping, with a short initial gap;
later clips include Switch Home. Three contact sheets are committed. Separate
capture-card audio was started later and saved locally as user-demo-audio.m4a;
it is not a synchronized full demonstration soundtrack. The extra macOS window
recorder did not produce a usable recording; it was stopped. Recording helpers
were stopped when the user said done. No controller inputs during demonstration.

Austin John wP2rClXrn3A, 8:42–9:10, recommends Central and demonstrates riding
on the dirt path and adjoining grass around the low rocky hill. He does not
prescribe a fixed lap, reset boundary or exact controller sequence.
https://www.youtube.com/watch?v=wP2rClXrn3A&t=522s

User prefers move-a-little / Lets Go over continuous road following. Next route
calibration should test short steps from a fixed anchor, bounded cumulative
movement, kill/progress checks and reanchoring. Repeated passes must reach fresh
spawns. Preserve refusal/shiny handling. Long winding-path following would need
visual drift correction; short steps reduce that requirement. No assertion that
20 sightings are 20 new spawns or that all are reachable from one position.

## Prepared code build

Pushed prototype head `3136433380d6c081f3bb34a8a643f7303d7fc054`.
Tree `0ff7b0a20c40246f4172462fc09e0355f2668849`.
Signed binary SHA256 `718f4cd905e1851a52ce511c1ea42df34c8e95282005fa8add51ddc33bb0bdd9`.

- 8ad4c65cb: stage begin/end timings and outcomes, recipe/herb flag, clearing
  cycle duration/defeats/event/empty count, refresh reason, final rounds/rate;
  prepared-mode Hunt Seconds and clearing summary now include actual elapsed time.
- cd12a6e9e: automatic power panel reading replaces the Meal Powers checkbox.
  Read all three row positions; split name/type and level crops. Two complete
  matching reads required. Unsupported languages/levels, wrong/missing/conflicting
  text and panel closure failure stop before clearing. English and Traditional
  Chinese supported; unsupported languages rejected before full-hunt ingredients.
  Same console reader exposed for native screenshot replay. Ordinary level two
  default; level three still requires explicit herb opt-in.
- 313643338: initial Central candidate selected automatically: one zoom out,
  west 950ms then south 650ms, nearest center detection, existing two-name-read
  confirmation gate. Derived from observed manual map traversal, **not a verified
  automatic route**. Wrong/unreadable destination stops without flying.
  Destination selection no longer waits for an operator. Local re-flights retain
  the same name gate.

Signed full build passed. Nine native groups passed (Power, Flight, Plan,
Locations, Loop, ExitPolicy, Options, Sandwich, EncounterOptions). First power
acceptance and English acceptance tests were each observed failing before their
implementation. Power regression cases also cover the earlier Lv1 result, wrong
power/type, missing/conflicting second read, panel open/close failures, unsupported
language/level, Grass Lv2, explicit Lv3 and full-width digits.

Actual screenshot replay initially rejected the correct Lv2 because wide-column
OCR read Fire as y. Fixed by splitting columns; final replay through the real
console reader accepts `遭遇力：火 Lv.2`, rejects earlier `遭遇力：火 Lv. 1`, and
recognizes actual Central name. These are offline tests, not live controller
validation. Incremental lint: 42 changed C++ files, zero failures; one existing
file-length advisory. Signature strict verification passed. No full downloaded
fixture suite claimed. Application tests and screenshot replay are native C++;
Python scripts only build/link/orchestrate them.

## Next field test and current ownership

Lab closed before rebuild. Profile reset to full hunt, 300s/100 rounds/3 empty,
Charcadet Fire Central, no herbs, shiny STOP, path acknowledgment false.
Only the Central path-positioning checkpoint remains supervised. The automatic
candidate seek and power-panel open/read/close need live verification. Full mode
already has bounded roaming; todays measured control was the stationary mode.
Do not claim a calibrated move-a-little route or automatic Central return.

Capture briefly showed colour bars during return attempt, then user demonstrated
movement. Subsequent Home resume and map actions occurred before the user said
not to worry about the Switch and focus on code. No Switch actions after that
instruction. Return/save/farming handback was not completed or claimed; user owns
the Switch now. No app re-open for this build. No PR created or mutated.
