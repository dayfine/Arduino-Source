# Scheduled resumption after quota reset

User explicitly requested stopping now and scheduling a fresh code-fix task for
this evening. One-shot local launch agent is registered for 2026-10-24 18:27 PDT,
after the primary reset. No further field work is authorized before this wake.
Fresh CLI task avoids the earlier same-thread active-writer failure.

Read LOCATION_VERIFICATION.md and research/isolated-encounters/2026-10-23/
PROTOTYPE_STATE.md on agents. Code head 6ca85664b on prototype/sv-isolated-hunt.

Confirmed wrong destination: actual flight confirmation 西第１區：北 = North;
Austin John video wP2rClXrn3A at 521–525 seconds says CENTRAL. Use cached video
/tmp/sv-isolation-videos/wP2rClXrn3A.video.mp4, .en-orig.json3 and committed
reference images at 525s/540s. Correct selection and verify the named destination
before confirming, then calibrate the approach. Do not call a guessed offset
verified. Native C++ policy regression tests, build and incremental lint required.

Fire Encounter Lv2 ordinary recipe is coded (6 Rice / 2 Curry Powder / 2 Wasabi)
but not verified in game. Need actual powers confirmation before roaming and
setup budget appropriate to assembly. No herbs by default. Prior North pilots
were not valid Charcadet-only yield measurements.

Last known live state: release open/stopped, AZP zerogate selected; latest feed
colour bars after Switch Home/Violet. Prototype closed. No farm handback complete.
Inspect fresh; never issue blind controller commands. Don't interrupt unrelated
programs. Code first; if safely borrowing hardware later, preserve shinies and
restore/verify agreed farming before finishing. User asked us to stop now, so no
more attempts were made to restore video/start farming in this scheduling turn.

Persistent lab signing is configured. Read lab/MACOS_LOCAL_SIGNING.md. Separate
lab/release IDs confirmed. One live microphone reapproval occurred, followed by
release->lab->release without a further observed prompt; do not claim universal
permission persistence. Keep test helper outside signed bundle and close lab
before builds. No signing-key recreation or privacy-database changes.

Schedule details (local, not an app-native automation):
- ~/Library/LaunchAgents/com.dayfine.codex.isolated-20261024-1827.plist
- ~/.codex/timers/isolated-20261024-1827/{config.json,prompt.txt,timer.py,ARMED,state.json}
- launchctl registration verified: waiting on calendar trigger, runs=0.
- configuration dry-run validated without starting any model task.
- At wake check actual quota first; do not start if unknown or below 25%.
- Uses a new codex exec task, inherits local defaults, writes run.jsonl and
  last-message.txt there. Lock/ARMED guards prevent duplicate model runs.
- Mac must be powered on/logged in; missed calendar triggers may run on wake.
  No promise that sleeping/offline hardware will execute at the exact minute.

No upstream PRs, no subagents, notes only on agents. Commit/push fixes to prototype;
report results and remaining validation limits. Do not repeat the scheduled job.


## Scheduled task executed — evening 2026-10-24

This one-shot request was resumed. Read [RESUME_CODE_FIX.md](RESUME_CODE_FIX.md)
for the result at pushed prototype head `5db7dfe1b`. Build, eight native groups,
North-image OCR replay and incremental lint passed. Central selection/path and
return remain supervised pending real field calibration; actual Fire Lv2 remains
unverified. No live inputs or restoration occurred because a fresh visible game
state was unavailable. Do not repeat or reschedule this task automatically.
Updated notes are committed locally on agents; no agents push was made under the
push-only-prototype instruction.
