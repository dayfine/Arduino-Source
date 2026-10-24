# Isolated hunt — current state and next run

Updated 2026-10-24 09:12 PDT. Code is built/pushed and ready for bounded field
calibration. The live Switch is currently running Sword/Shield Max Lair, not
SV/AZP. Do not interrupt it without the user's handoff. An asynchronous question
is pending: leave Max Lair running (code only), or switch to SV after the current
adventure. Silence is not authorization. No Switch was borrowed in this session.

## Code/build checkpoint

- Worktree: /Users/difan/Projects/Arduino-Source-isolated
- Branch: prototype/sv-isolated-hunt
- Head: f2347d3d1d9a17614b0ff5746daccb615d7bbdd5 (pushed, clean).
- Includes merged fork #41 (67ed6141d), providing incremental lint directly.
- Build: build-prep/app/SerialPrograms.app, v0.71.3-user.
- Executable SHA-256: a7569a0b0edb628e14f05bf2fc052381e8c1d89876053f539f78d9ef133396a7.
- Local final build/link passed. Native Plan, Locations, Loop, ExitPolicy,
  Options and EncounterOptions groups all passed. Same functions are registered
  in PokemonSV_Tests.cpp and run through a local native QApplication bootstrap
  reusing the app's compiler/link flags. The Python helper only orchestrates the
  C++ build; it is not an application test implementation.
- Incremental lint: 29 changed C++ files, 0 failures. One advisory is the existing
  MaterialFarmerTools file, reduced by moving regional flight into shared code.
- Raw logs: build-prep/isolated-live-2026-10-24/build-final-candidate.log and
  native-final-candidate.log. Existing Tesseract deployment-target warning only.

## Implemented flow

The new default start mode, hunt-from-zero-gate, expects Zero Gate as if just
flown there, on foot, with a healthy suitable lead. Legacy supervised controls
and the Area Zero -> Zero Gate travel utility remain available.

1. Editable rows: enabled, Pokemon, encounter type, location, shiny action,
   ball and ball limit. Initial catalog is Charcadet/Fire/West Province One
   Central and Chespin/Grass/Chargestone Cave. Other research targets unsupported.
   All enabled rows validate before controller commands; incompatible rows
   reject the whole plan. Default is one Charcadet row, stop on shiny.
2. Save before spending ingredients. Make the row's Fire/Grass sandwich through
   the shared SandwichMakerOption/routines at Zero Gate. Requires game language
   and Sparkling/Title/Encounter recipe. Type comes from the row automatically.
3. Programmed flight and approach. Chespin reuses Paldea -> Blueberry entrance,
   Central Plaza and Chargestone helpers. Regional flight was extracted from
   MaterialFarmerTools into PokemonSV_RegionTravel, preserving its command and
   adjustment order. Charcadet uses an experimental relative map seek, icon
   inference and an initial side-of-center approach.
4. Bounded four-leg clearing sweeps with R and full-window refusal/battle
   watching. Charcadet uses 1400ms legs; Chespin 700ms. Empty rounds refly the
   nearby anchor and reapproach. A refusal attempts ZR acquisition and stops
   without refreshing/despawning if battle entry is not confirmed.
5. Shared process_battle handles species/shiny detection, flee/catch/save and
   notifications. Per-row shiny rule is appended last to a COPY of the base
   encounter settings; selected ball/limit reach the actual shared handler.
   Normal battle continuation requires reanchoring. Battles interrupting setup,
   travel or refresh are handled, then stop the unsafe interrupted route.
6. Normal time/round/power limits can trigger return flight and save. Policy
   stop, unresolved refusal, no progress and failures leave the current scene
   for inspection. Return flight is experimental and must be visually verified
   before restarting AZP; never infer Zero Gate solely from an overworld frame.
7. Stats: kills, encounters, shinies, clearing passes, refusals, spawn refreshes,
   sandwiches, hunt seconds. Logs show stages, stop reason and per-target detected
   kills/minute. These do not prove target purity or distinct shiny checks.

Defaults: 120 hunt seconds, 100 rounds, 3 consecutive empty rounds; 29-minute
power budget starts after sandwich setup and includes travel. A finite clearing
round can extend past the time limit. Setup/travel guards are 240s, local refresh
90s, return 180s; guards monitor battle/deadline inference while shared routines
run. User stop/cancellation remains available.

## Validation and known gaps

Native tests cover 11 planning cases, 2 location cases, 23 scripted loop cases,
11 departure-policy states, option defaults/save/restore, and 3 shared shiny
policy mappings. Cases include interrupted setup/travel/refresh, power exhausted
by travel, no wasted refresh at the final round, refusal preservation, catch
settings and reanchor failure. Tests do not prove real movement or spawn purity.

A UI-only profile sv-isolated-ui-20261024 was opened with controller/video/audio
all None, inspected, then closed normally. No Switch input or capture/controller
handoff occurred. It displayed the new start mode/options/stats. Rebuilt binary
requested camera/microphone permission, which was granted. Permission may be
requested again after a subsequent rebuild.

The following remain unverified and require SV field access:
- Charcadet: initial Zero Gate -> center seek (-1,-0.47,850ms at one zoom-out),
  actual destination identity and side-of-center approach.
- Chespin: cross-region sequence and actual Cave habitat. Existing court route
  is not evidence Chespin spawns there. Canyon diversity upgrade and fly unlock
  must already exist; do not spend BP to unlock anything without instruction.
- Both local refly anchors, square sweeps and target-species purity.
- Both return-to-Zero-Gate seeks; Chespin region switching is especially unverified.
- Real sandwich ingredients/placement and battle acquisition/catch behavior in
  this integrated program. Those are shared implementations, not live proof.

## Next actions

1. Honor the user's response about Max Lair. While it runs, code/build work can
   continue; do not open a device-assigned prototype profile concurrently.
2. Follow FIELD_VALIDATION.md once the Switch is available in SV. Begin with
   one short Charcadet round, then calibrate Chespin. Observe actual landing and
   habitat; do not report a kill rate as a target benchmark before purity checks.
3. Fix measured route failures, rebuild, repeat native groups/lint as appropriate.
4. Whenever live work finishes or quota is low, preserve shinies, save, return to
   verified Zero Gate, close prototype and restore release AZP only when SV is the
   agreed farming context. Verify clearing resumes. Reserve >=20% before another
   experiment. Current Max Lair is a different context and must remain untouched
   until explicitly handed over.

No isolated-hunt PR has been opened. Prototype remains Codex-owned. No upstream
PR creation/reopening/mark-ready is allowed. Notes belong on agents only.

## Scheduling

The earlier 06:01 timer fired but same-thread Codex resume failed due to an active
writer lock. It exited 1; no overnight code ran. No timer remains armed. Do not
claim background monitoring or automatic future continuation.
