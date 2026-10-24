# Isolated encounter experiment — active handoff state

2026-10-24 PDT. User explicitly authorized borrowing the Switch from the
release Area Zero Platform program, running bounded isolated-encounter tests,
then restoring AZP whenever testing finishes or quota becomes low. No upstream
PR actions are authorized.

## Restoration obligation

**RESTORE AZP BEFORE ENDING LIVE WORK.** The user asked that low quota trigger
an early handback. Start no new experiment at 20% or less remaining in either
quota window; reserve enough allowance for travel and verifying AZP restarted.
Check `tools/read_codex_quota.py` at milestones. Do not spend the remaining
allowance analyzing logs while the user's farming program is stopped.

- Release app: `/Applications/SerialPrograms/SerialPrograms.app`, v0.70.10.
- Release binary SHA-256: `2cac9c13d81fcbcb30a09f07228d738371d8ae1427ba1202a9079c69694281da`.
- No named release profile; settings/log are under
  `~/Library/Application Support/SerialPrograms/`.
- Program: Shiny Hunt - Area Zero Platform. Actual UI and persisted mode:
  `zerogate` (Start at Zero Gate fly spot), language `chi_tra`.
- Original PID 74800 was stopped normally at 01:11:15 and closed normally at
  01:13. Confirmed counters: 79 kills, 4 encounters, 0 shinies in that run.
- Switch save was visually confirmed before closing; healthy level-100
  Corviknight lead had 286/334 HP. Game is Violet, Traditional Chinese.
- A separate app/profile must not attach to the controller while another
  instance has it open. Close the owning app before transferring hardware.

## Current experiment

Prototype app: `/Users/difan/Projects/Arduino-Source-isolated/build-prep/app/SerialPrograms.app`.
Profile: `sv-isolated-live-20261024`. Revision `e917f1114`; binary SHA-256
`c4dcdb54b0360568c3246945d1725ab164bc9fdd7a348aca68fee090e2d4845e`.
First probe: prepared-spot stationary clearing on the current AZ platform,
120-second bound, stop after 3 empty passes, Chinese Traditional OCR.
This is a control for instrumentation/acquisition, not verification of a
Chespin or Charcadet isolation spot. Further trials depend on the first result.

Raw screenshots, UI helper, private settings backups and structured handoff
state live in:
`/Users/difan/Projects/Arduino-Source-isolated/build-prep/isolated-live-2026-10-24/`.
Do not commit private app settings. The latest phase is in `handoff-state.json`.
PID-targeted `control` helper verifies the executable path before UI actions.

## Restore procedure

1. Stop the prototype and preserve any shiny/encounter before leaving. Save
   if progress needs retaining.
2. Return to Zero Gate fly spot. From inside Area Zero the existing routine
   opens Y/map, nudges the cursor up-left, then flies to Zero Gate. Inspect
   each manual screen; do not blindly confirm an unknown map destination.
3. Verify the Zero Gate landing. Close the prototype, releasing hardware.
4. Reopen the release app. Its actual settings were saved on normal close;
   backup: `release-settings-after-close.json` in the raw run directory.
5. Verify selected AZP program, `zerogate` mode and the correct capture/audio/
   controller. Start it and observe navigation/traversal before calling the
   handback complete. Update this record with evidence and final status.

Current status: **AZP restored and farming**. Release PID 96268 restarted at
01:40:56 PDT, traversed the research station and healed, reached the platform,
and recorded its first kill at 01:42:42. Prototype closed normally; no other
instance owns the hardware. Saved at Zero Gate before reopening release.

Prototype revision `ba454e4a8` adds a live-verified return-to-Zero-Gate mode.
The three stationary trials were controls on the AZ platform, not target-hunt
validation. See [RESULT.md](RESULT.md). Next work is explicit target selection,
programmed approach and sandwich setup; leave AZP running during code work.
