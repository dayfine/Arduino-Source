# Isolated hunt prototype — current checkpoint

Code: `prototype/sv-isolated-hunt` at `da0db2aeb`. Build, seven native C++ test
groups and incremental lint pass. No PR created. Notes stay on agents.

Read [latest field record](../../../runs/isolated-2026-10-24/PREFLIGHT_1325.md)
first; it supersedes the older build/no-live-test status in IMPLEMENTATION_QUEUE.

User requires ordinary Encounter Power Lv2 by default; Herba Mystica is explicit
opt-in only. Fire recipe now 6 Rice + 2 Curry Powder + 2 Wasabi, not yet verified
in game. Actual previous Pickle recipe was confirmed Fire Encounter Lv1 on screen.
Automated starting-anchor re-flight is implemented before ingredients, with
failure/battle tests; start still requires outdoors at Zero Gate, not anywhere.

Two target pilots: 1 kill / 15 seconds, then 3 kills / 40 seconds with one
unresolved refusal. Counts are sound detections, not identified target kills.
Numel was seen at current patch; Charcadet-only isolation is NOT established.
Player hit a wall during sweeps. Correct/verify habitat and approach before
longer yield tests. Keep nearby Pokemon intact until refusal inspection finishes.

Prototype PID 93949 is open/stopped at the field location, sole device owner.
AZP restoration was paused at user's request for longer testing; release app is
closed and AZP has NOT restarted. Release AZP settings now use zerogate mode.

Capture permissions recur because lab build uses ad-hoc hash-based signing.
No signing identity installed; stable certificate signing is proposed, not set up.
