# First integrated isolated-hunt field validation

Code checkpoint and known gaps: IMPLEMENTATION_QUEUE.md. Existing AZ-platform
controls in the 2026-10-24 RESULT.md do not validate either target location.

## Exclusive hardware handoff

The current release process is PID 96268, Max Lair Standard (Sword/Shield), not
AZP. The user has been asked whether to leave it running or switch to SV after
the adventure. Wait for that handoff. Preserve all catches/shinies and save at a
safe boundary. Do not force-close an adventure or overwrite another game's
configuration. Switching to SV may require the user's cartridge/software setup.

When SV is available, inspect party/lead and current game state, save, and use a
programmed/verified flight to Zero Gate. Original return-zerogate mode works
ONLY from inside Area Zero. Do not apply it to arbitrary Paldea/Blueberry or to
Sword/Shield. Verify Zero Gate visually before the integrated hunt starts.

Close the release app normally before assigning hardware to the prototype. Use
profile sv-isolated-live-20261024 after backing it up; its OLD START value is
return-zerogate. Set START=hunt-from-zero-gate, LANGUAGE=chi_tra and a valid HUNTS
row before launch. Do not accidentally start the old travel utility. Check
capture/controller/audio and ensure PA-Stats.txt exists. Grant microphone/camera
permissions if prompted and wait for actual frames/audio before Start.

## Charcadet first

Use one enabled Fire/Charcadet/West Province One Central row. Shiny action should
preserve the target; stop-on-shiny is the default, or select the requested ball
and catch/save when appropriate. Use MAX_ROUNDS=1 initially and RUN_SECONDS=30;
this is a deliberate pilot setting, not a hardcoded one-round limit.

Observe/save these stages:
- Saved at Zero Gate before ingredients; sandwich made with Fire powers.
- Map seek and actual center landing. Initial offset is uncalibrated; identify
  the center/location from the game. Stop if it is wrong rather than pretending
  a successful flight means the right destination.
- Side-of-center approach, visible target/competitor species and terrain.
- One sweep: R behavior, lead reachability, detected kills, refusal handling,
  battle entry/species OCR, and stage duration. Retain screenshots/log excerpt.
- Return candidate and save. Verify Zero Gate visually; do not launch AZP from
  another center. Correct the route calibration when necessary.

If the landing/habitat is correct, repeat a 2–5 minute bounded probe to measure
refly/replenishment and encounters. Record start/end counters, elapsed setup /
travel / hunting / recovery, detected kills per minute, entered species,
refresh count/yield, empty-round time and any failure. Distinguish kill sounds
from target kills, visible Pokemon from unique shiny checks and observations
from inference. Do not invent species purity from generic sound counts.

## Chespin second

Requires Canyon diversity upgrade and visited fly anchors. Start at verified
Zero Gate with the Grass row. Inspect cross-region flight -> Central Plaza ->
Chargestone landing. The existing court may not be Cave habitat; scout the actual
spawn patch and confirm Chespin before a sustained run. Record the camera and
standing anchor and calibrate approach/sweeps/refly. Follow the same short-round
then bounded-probe progression. Do not buy upgrades or expand scope mid-test.

## Failure and final handback

An unresolved refusal or policy stop must not trigger a spawn reset/departure.
Inspect/preserve the Pokemon first. A failed interrupted route must not resume a
timed path from an unknown pose. Capture evidence, then recover deliberately.

Before finishing live work, preserve shinies/catches, save, reach VERIFIED Zero
Gate, close the prototype and reopen the agreed release SV/AZP configuration.
Verify station/platform traversal and clearing, not only a running button. If
live context is still Max Lair, leave it untouched and do not claim an AZP
restoration took place. Always reserve headroom for recovery; start no new
experiment at <=20% remaining quota. Never leave borrowed hardware idle while
analyzing code or logs.
