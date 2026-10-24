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

Prototype PID 97316 is open/stopped with working video/audio, sole device owner.
Current capture shows Zero Gate; user moved there while signing was being set up.
AZP restoration remains paused; release app is closed. No hunt restarted during
signing validation. Release AZP settings use zerogate mode.

Signing setup is complete on code head `6ca85664b`, which adds no hunt behavior
beyond `da0db2aeb`. Stable certificate and bundle ID survived a changed signed
build/relaunch with working capture and no new permission prompt observed.
Read [MACOS_LOCAL_SIGNING.md](../../../lab/MACOS_LOCAL_SIGNING.md) for setup,
verification and the current signed binary hash.
