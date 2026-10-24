# Integrated hunt preflight — 2026-10-24 13:25 PDT

User requested preparation for a test run. Release PID 96268 was already stopped
(Start Program button visible), with Max Lair session counters 25 runs, 21 wins,
93 catches, 1 shiny. Capture showed colour bars; current game state is unknown.
No Switch input or game change was issued. Release app closed normally.

Prototype PID 83439 is open with profile sv-isolated-live-20261024 and sole
ownership of configured capture/serial/audio devices. Controller enumerates;
capture still shows colour bars. Await user restoration of Switch HDMI picture
and confirmation Violet is available before saving, traveling or starting.

Candidate f2347d3d1, binary SHA-256:
a7569a0b0edb628e14f05bf2fc052381e8c1d89876053f539f78d9ef133396a7.
Matches previously built/tested candidate; no code changes or repeated tests.
Profile now uses hunt-from-zero-gate, Traditional Chinese, one enabled
Charcadet/Fire/West Province One Central row, stop on shiny, MAX_ROUNDS=1,
RUN_SECONDS=30. Recipe is shiny with two salty herbs (availability unverified).
Stats file exists. Settings backup/evidence stay private under code worktree
build-prep/isolated-live-2026-10-24/preflight-1325 and /tmp/isolated-prep-*.png.

No actual target test has started. Verify picture, game, healthy lead,
ingredients and saved Zero Gate start before starting. Flight calibration and
return destination remain unverified; follow FIELD_VALIDATION.md. User handback
context must be established from current game rather than assuming AZP.
Quota at 13:25: 100% primary remaining, 61% weekly remaining.

## Pilot and revisions at 13:45 PDT

The user restored Violet at Zero Gate and authorized starting. Corviknight lead
was full health, 334/334, level 100. A manual save was visually confirmed.
First pilot ran 13:28:18–13:29:38, stopped during ingredient selection because
only one Salty herb was available for the old two-Salty recipe. Zero sandwiches,
kills or encounters. Selection was cancelled; the Salty herb visibly returned
to count 1. Picnic packed; no Herba Mystica consumed. A Salty+Bitter configuration
was prepared but NEVER started: user explicitly requested Encounter-only,
ordinary ingredients and no rare herbs by default.

Code c18a234d5 implements ordinary recipes and explicit USE_HERBA=false default.
Fire is Pickle + Olive Oil (Encounter Lv1); Grass is Great Klawf Claw (Lv2).
Reference: https://bulbapedia.bulbagarden.net/wiki/Sandwich_recipe (#20, #105).
Legacy saved shiny mode without opt-in is sanitized on load. Shared sandwich
maker remains responsible for selecting/building ingredients. Seven native
registered groups pass, including both recipes, legacy settings and toggles.

User additionally requested program-driven initial flight. Code 3d222e3c5 adds
reset_start before ingredients, guarded 90s, selecting only the overlapping
flypoint on the map. Current contract remains outdoors at Zero Gate; this is
pose normalization, NOT arbitrary-region navigation or destination-name OCR.
Tests verify startup ordering, failed-start no ingredients, and startup battle
policy. Seven groups pass, incremental lint scans 31 files, zero failures.
Final binary SHA-256:
2ee19e0a1000d4b293b25086899bd882e0517b02caf6617821608d34f10080c5.
Both commits pushed to prototype/sv-isolated-hunt, no PR created.

The manual re-flight to Zero Gate finished at 13:40, before the automated
startup revision. Candidate reopened as PID 88840, sole device owner, profile
sv-isolated-live-20261024. It is NOT running yet: waiting for video frames after
camera initialization. Still one round / 30 seconds, no herbs, stop on shiny.
Do not report a second pilot as started until the Start button is pressed and
live startup logs confirm it. Capture initialization has intermittently shown
0 FPS on reopening; a Reset Video restored frames previously.
