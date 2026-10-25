# Isolated hunt code resumption — 2026-10-24 evening

Code branch: `prototype/sv-isolated-hunt`.
Starting head: `6ca85664b`.
Final head: `5db7dfe1b46a5c49731a0c7840b5000811fd8487`.
Final source tree: `9c638ef93db1e453056a9a01b596752438e696d0`.
Signed app binary SHA256: `d835a99571957987338e19ff45e35d22ccd95540201e2dfa7bff01ead9f91018`.
Native test helper SHA256: `a8ddfa0a03cfcfaf05987931816ad3dad9d4ec633d20682f463c2a5b98aec6d4`.

## Result

Unsafe Central flight acceptance is guarded at a native C++ policy seam used by
the console implementation. Initial Central selection is supervised: select
West Province (Area One) - Central and choose Fly here, leaving the Yes/No prompt
open. The program requires two complete matching destination-name reads and a
still-visible confirmation before it presses Yes. North, incomplete/unreadable
text, inconsistent reads, missing confirmation and failed landing all fail.
Supported languages are English and Traditional Chinese; other Central language
settings are rejected before starting the full hunt or consuming ingredients.
Local re-flights use the same name gate.

Removed the unverified Zero Gate-to-North seek and the uncalibrated Charcadet
approach movements. Each Central arrival, including refresh and post-battle
reanchoring, requires a fresh supervised path-positioning acknowledgment before
clearing. The reverse offset was also unverified from Central; Central return is
manual and no automatic departure/save or farming handback is claimed.

Meal Power verification is a separate hunt-policy stage after making the meal
and before travel. The game powers panel opens for supervised inspection of the
actual Encounter Power type and level. Check `Meal Powers inspected` only after
seeing Fire/Grass Encounter Lv2 (or Lv3 for explicit herbs). A stale checked box
is reset at every gate; timeout, missing verification, or interrupted verification
cannot lead to travel or clearing. Prepared control modes also require power
inspection before clearing. Verification waits count against the power timer.
Manual controls are available for the supervised stages and should be used only
at those prompts.

Ordinary ingredients remain the default. Fire is still 6 Rice + 2 Curry Powder +
2 Wasabi, **not yet made/verified in game**. The maker/setup deadline is now ten
minutes instead of four to accommodate six-rice placement/OCR work; this budget
still needs timing in an actual six-rice assembly. The power gate has a bounded
110-second acknowledgment window, with the full-hunt battle/deadline guard around
it. No correct power is inferred from sandwich-maker completion.

## Verification

- Built the full signed macOS arm64 app with the existing lab cache and strict
  compiler flags. The cache uses Qt 6.8.3. Exit 0.
- Eight native C++ groups passed through `build-prep/run-isolated-native-tests.py`:
  Flight, Plan, Locations, Loop, ExitPolicy, Options, Sandwich, EncounterOptions.
  Flight tests are registered in `PokemonSV_Tests.cpp`, using scripted game
  actions at the same policy seam as the console.
- Added meal-gate failure/interruption and supervision-time/power-expiry cases.
  The trace checks ensure maker success without verified power cannot roam.
- Linked those native flight regressions against a deliberately unsafe generic
  confirmation implementation in a separate local executable. The observed-North
  regression failed as intended: expected no confirmation, read `select confirm`.
  The application library/source was never replaced by this unsafe implementation.
- Replayed the actual recorded `wrong-anchor-north.png` through native project OCR
  using the new console crop: read `西第1區：北`, accepted=0, confirmed=0.
- Incremental code-health against `origin/main`: zero failures. Existing advisory
  for one file over 500 lines remains. Linter self-tests: 22 passed.
- Preserved source CRLF; whitespace check uses `core.whitespace=cr-at-eol`.
- The native helper and replay/mutation executables are outside the signed bundle.
  Persistent certificate, lab bundle ID and app path were preserved. No signing
  certificate or privacy settings were recreated/reset.

Raw verification logs are beside the existing images under `evidence/resume-*`.
The original video map/path frames at 525/540 seconds and the old powers screenshot
were inspected directly. [Location evidence](LOCATION_VERIFICATION.md) remains the
basis for distinguishing Central from the invalid North pilots. The Traditional
Chinese Central spelling also appears in this [location list](https://forum.gamer.com.tw/G2.php?bsn=1647&lorder=1&parent=14525&sn=14624).

## Live state and remaining work

Fresh read-only inspection found release process PID 1139 at the expected release
path. The PID/path-guarded accessibility helper returned no windows, and desktop
capture failed with `could not create image from display`. This background task
could not establish the current game/program state. No controller commands, app
launch/quit, ingredient spending or live experiment occurred. Farming status and
the earlier AZP handback remain unverified; do not infer stopped/running from PID.

Pending field work requires a visible, known game/start state and no unrelated
active program: validate positive Central OCR/landing, match the 540-second path,
time six-rice assembly, retain actual Fire Encounter Lv2 powers evidence, then
measure species-confirmed yield. Preserve refusals/shinies. Calibrate automatic
selection/approach/return only with actual screenshots and logs. North's 3 sound
kills / 40 seconds are not Charcadet-only yield evidence. Restore AZP and verify
actual clearing after any future borrowed test.

Quota was checked from the account at startup and milestones. No quota snapshot
is stored here. No PR was created or mutated. This scheduled task was not repeated
or rescheduled. Only the prototype branch is pushed; these notes stay committed
locally on `agents` under the task's push-only-prototype instruction.
