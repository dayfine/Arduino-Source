# Isolated-hunt prototype: implementation and field handoff

2026-10-23 PDT. Owner: Codex. This is a supervised acquisition experiment;
continuous spawn resetting and autonomous route recovery remain unimplemented.

## Question and scope

Can a healthy lead clear a prepared target patch, and can a refusal bubble
be converted into a normal battle using the existing ZR action? Refusal is
not shiny evidence. Shared battle handling confirms shininess after entry.

Chespin is the first integration candidate, with Charcadet the next user-priority
target. Both can use prepared-spot mode. The optional Central Plaza mode only
implements the existing Chargestone flight/court approach; the court's Chespin
spawn footprint remains unverified. This experiment does not change the BBQ
roadmap, existing programs or their PR branches.

## Isolation and build

- Code branch: `prototype/sv-isolated-hunt`, based on `origin/main` at
  `4706977d0` (not the notes branch or an existing BBQ PR).
- Worktree: `/Users/difan/Projects/Arduino-Source-isolated`.
- Program: **Pokemon SV → Shiny Hunting → Isolated Hunt Prototype**.
- Source: `SerialPrograms/Source/VideoGames/PokemonSV/Programs/ShinyHunting/PokemonSV_IsolatedHuntPrototype.{h,cpp}`.
- Code revision: `e917f111423c8a8bc7be04fa49eea3bf34652b19`, pushed to the fork.
- Packages revision: `92946a8fb1c789165ad4171b5981a61a47e59304`.
- Build: `cmake --build build-prep/app -j 4` from the worktree.
- Logs: `build-prep/configure.log`, `build-prep/build.log` (initial eight-job
  build, intentionally interrupted to reduce memory contention), and
  `build-prep/build-j4.log` (successful resumed build).
- **Full arm64 application build, link and resource deployment passed.** The
  prototype and panel registration also passed direct native compilation.
- Binary SHA-256: `c4dcdb54b0360568c3246945d1725ab164bc9fdd7a348aca68fee090e2d4845e`.
- Linker warning: Homebrew Tesseract was built for macOS 26.6.2 while the app's
  deployment target is 26.0. The build host runs 26.6.2; deployment to an older
  OS has not been verified.
- The GUI was subsequently opened on 2026-10-23 with profile
  `sv-isolated-prototype`; the correct prototype panel was visually confirmed.
  The prototype program itself was not run. Live hunting validation is pending.
- During that GUI session the user selected the controller already used by the
  other instance, causing a serial conflict. The prototype instance was closed;
  the original logged STOPPED and then a successful controller reconnection.
  Separate profiles isolate settings, not physical devices. A second instance
  must stay disconnected until it has a distinct controller/capture assignment.

Launch the completed build with a separate profile:

```sh
/Users/difan/Projects/Arduino-Source-isolated/build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms --profile sv-isolated-prototype
```

Select the prototype in the app and inspect capture/controller settings before
starting. Launching the app alone does not start the program.
This build stores the console device settings under `ConsoleSetup`, rather
than the older `SwitchSetup` key. Confirm the displayed device assignment;
an empty setting under an obsolete key is not a reliable disconnection.

## Behavior

1. Require a 16:9 capture of at least 720p and detect the overworld.
2. Use the prepared spot, or fly from Central Plaza and approach Chargestone's
   existing BBQ court. Battle detection wraps the approach and clearing actions.
3. Send R and observe the existing refusal detector. Without a refusal, wait
   12 seconds and count kill-sound detections. There is no player movement in
   prepared-spot clearing.
4. On refusal, try ZR for five seconds and wait ten seconds for battle entry.
   If no battle menu appears, stop for inspection without despawning the target.
5. Stop after three consecutive passes without a kill, or after the configured
   clearing limit (default 120 seconds, checked between passes). A pass can
   add about 17 seconds beyond that limit. Approach and battle time are separate.
6. A detected battle uses the shared species/shiny/action pipeline. Default
   policy stops on shiny and runs from non-shiny. Configured capture/save
   actions remain supported. After any handled battle, stop for re-anchoring.

Stats: Kills, Encounters, Shinies, Clearing Passes, Refusals. Logs identify
approach, clearing, acquisition, battle and stop reason. These are instrument
counts; kill count does not establish target-species purity or shiny checks.

## First field session

Use the [starter survey](STARTER_SURVEY.md) for unlock and habitat evidence.
Do not begin with a 1–2 hour unattended run.

1. Coordinate exclusive Switch ownership and inspect the current app/program.
2. Save manually before sandwich setup if appropriate; prepare the correct
   power and a healthy, suitable lead. This prototype neither makes sandwiches
   nor heals, resets the game or refreshes expired powers.
3. Chespin: verify Canyon diversity upgrade and Chargestone fly unlock. Start
   at Central Plaza for the approach trial, or use a photographed Cave-habitat
   anchor with observed Chespin. Other cavern habitats may lack Chespin.
4. Record landing/standing/camera screenshots, approach time, visible targets
   and competitors. Set the game language for species OCR/frequency records;
   without it, entered species cannot be confirmed by the name reader.
5. Run a short clearing probe. Record kill delta, refusal events, battle-entry
   success and stop reason. A refusal timeout needs inspection before retrying.
6. For Charcadet, manually prepare the demonstrated West Province Area One
   Central spot, a Fire power and the same camera/lead checklist. Automatic
   Paldea travel is not part of this prototype.
7. Only after acquisition works, measure one local spawn-refresh movement and
   return-to-anchor pattern. Add that profile, then do repeated short trials
   before sustained validation. Avoid automatic despawning after an unresolved
   refusal or blindly continuing a movement path after a battle.

## Validation record

The [Kitakami video review](KITAKAMI_VIDEO_REVIEW.md) adds the Sentret cave as
an optional stationary-clearing control, using the existing prepared-spot mode.
Before coding a spawn-reset path, check whether clearing itself replenishes
reachable targets at that anchor. Chespin and Charcadet retain their priority;
the Kitakami footage is source evidence, not our prototype's validation result.

This is disposable field code, not a production PR. Native compilation and
full application linking are required; no Python application tests or mirrored
source-extraction tests are used. Unit tests were not added for this prototype.
No route, target purity, shiny acquisition or sustained hunting rate has yet
been verified live. Absorb or delete the prototype after the field question is
answered; retain measured results in the notes branch.

## 2026-10-24 field checkpoint

[Control results](../../../runs/isolated-2026-10-24/RESULT.md): three AZ-platform
controls observed 3 total kills and the bounded empty-pass stop. No target
location or sandwich was validated. Revision ba454e4a8 adds the live-verified
Area Zero → Zero Gate return mode. Release AZP was restored and clearing at
01:42 PDT. Continue target selection, programmed travel and sandwich setup
before another takeover.

## 2026-10-24 tested-core checkpoint

Branch prototype/sv-isolated-hunt is now 81172c9f0, rebased onto fork main
8e973c48f. Pure planning and bounded action-loop modules have 29 passing native
C++ scenarios and a successful full application link. They are not wired into
the existing supervised UI. Read IMPLEMENTATION_QUEUE.md before continuing;
next work is the editable options table, console adapter, both actual travel
routes, sandwich and roaming integration. Leave release AZP farming until the
mostly end-to-end gate is met. Earlier scratch is preserved as a patch in the
raw run folder, not current source.
