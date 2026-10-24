# Isolated encounter implementation queue

User steering 2026-10-24: finish sufficient end-to-end coding before borrowing
the Switch again. Issue dayfine/Arduino-Source#40 is first priority in the
current quota window. Resume this queue after 05:59 PDT, or earlier if #40 is
finished with allowance left. Query actual quota before substantial work.

## Starting state (updated 02:14 PDT)

- Issue #40 completed as fork draft #41; CI code-health and tabs passed. See
  harness/ISSUE40_STATE.md. Its PR branch is handed to Claude; do not push it.
- Code worktree: /Users/difan/Projects/Arduino-Source-isolated
- Branch: prototype/sv-isolated-hunt, pushed head 81172c9f0. Rebased onto current
  fork main 8e973c48f. Old return-mode commit ba454e4a8 is now 3f51f0809.
- Earlier pre-test-first scratch was preserved in
  build-prep/isolated-live-2026-10-24/pre-test-first-scratch.patch, then removed
  before native test-first work. Do not blindly reapply it.
- New pure modules: PokemonSV_IsolatedHuntPlan.{h,cpp} and
  PokemonSV_IsolatedHuntLoop.{h,cpp}, under Programs/ShinyHunting.
- Native tests in PokemonSV_IsolatedHunt*_Tests.{h,cpp}, registered in
  PokemonSV_Tests.cpp and CMake. 11 planning scenarios, 2 location cases and
  16 action-loop cases pass (29 total). Separate C++ test main under build-prep
  calls the SAME registered test functions, no duplicated implementation or
  Python application tests. Binary: build-prep/isolated-plan-tests.
- Full application build/link passed: build-prep/isolated-live-2026-10-24/
  build-plan-loop-tests.log. Rebase needed 129 rebuild steps, latest incremental
  6 steps. Existing Tesseract deployment-target warning only. New modules/tests
  also meet the new code-health hard limits. Sources use CRLF.
- Core supports compatible target/type/location planning, enabled rows,
  ball/limit propagation, all-or-nothing validation; sandwich-before-travel,
  bounded clearing, empty-round refresh/stop, time limit, refusal acquisition
  without despawn on failure, shared battle callback and reanchor after battle.
- IMPORTANT: the pure loop is NOT wired to a console adapter or editable hunt
  table yet. Existing prototype UI still runs the older supervised experiment.
  Do not launch it believing the new end-to-end hunt is implemented.
- Release AZP restored, running PID 96268; observed 114 kills/2 encounters at
  01:56 PDT after restart. Leave it farming during implementation. No actual
  target route, sandwich, respawn movement or shiny acquisition is validated.

## Next coding slices after resumption

1. Read this queue, canonical rules and actual quota. Check code/notes status,
   preserve others' edits, and keep the active release app untouched.
2. Add editable target/type/location/ball/limit/enabled table, using shared
   EditableTableOption and PokemonBallSelectCell; plan through the tested pure
   module. Add native option snapshot/validation tests before implementation.
   Catalog currently implements only Grass/Chespin and Fire/Charcadet, with
   fly anchors and habitat/unlock prerequisites; other research candidates stay
   explicitly unsupported.
3. Add ConsoleIsolatedHuntActions implementing the tested seam, delegating to
   shared sandwich, map/navigation, Let’s Go detectors, process_battle and
   encounter action options. Every action must itself be bounded; now_ms must
   be monotonic. Save before ingredients, use the selected type, validate
   recipe/language before controller inputs. Stop-on-shiny remains the default;
   selected catch/save policy and ball must reach shared encounter handling.
4. Program travel/approach to BOTH actual test sites, including region switching
   if starting from the shared Zero Gate handoff. Central Plaza -> Chargestone
   flight is already shared, but court vs Cave Chespin spawn patch is unverified.
   Charcadet fly/approach needs coding/calibration from the researched video.
   Avoid guessed unbounded map pans. Explicit starting anchors are acceptable,
   but manual travel between sandwich and hunting is not the completed flow.
5. Add native tests for battle during travel/setup (current bool travel seam may
   need a richer event result), arrival failures, refresh/reanchor failures and
   state recovery before implementing that behavior. Reuse shiny/flee/catch
   handling, do not create another battle engine. Continuously watch refusal
   throughout each clearing observation window.
6. Wire status stage/counters/timings and run the program through the new loop.
   Bound sandwich lifetime as well as clearing duration; initial setup/travel
   must not silently exhaust the power. Build/link and native checks before
   borrowing hardware. Only field-test once mostly end-to-end complete.

## Required product flow

1. Editable hunt table: Pokemon/species, encounter type, compatible location,
   enabled flag, ball and ball limit; reuse encounter actions for shiny/flee/
   catch policy. Show only supported locations, reject incompatible combinations
   before controller inputs or ingredients. Chespin/Grass/Chargestone Cave and
   Charcadet/Fire/West Province Area One Central are the first two profiles.
2. A location catalog groups candidate routes by type, with fly anchor, unlocks,
   habitat, approach, navigation confidence and refresh strategy. Do not pretend
   all 24 starters or research candidates have implemented routes.
3. Make the chosen type sandwich using shared SandwichMakerOption and sandwich
   routines. Save first. Validate language, recipe/type and required setup.
   Existing non-shiny preset is Normal only; do not treat it as Fire/Grass.
4. Programmed flight and approach to BOTH test locations. Explicit initial
   anchor is acceptable, but another manual flight after setup is not the final
   flow. Reuse map/flight inference, detect arrival, bound retries and stop on
   unexpected state. Central Plaza -> Chargestone already exists. Paldea travel,
   region switching and Charcadet local anchor still need verified routes.
5. Bounded hunt rounds: roam/circle or move across a spawn boundary as supported
   by the profile, with return-to-anchor. For stationary Let's Go, continuously
   monitor refusal across the whole observation window, not just R startup.
   Observe or kill actual spawns, count kills/encounters, log stage timings and
   stop on no progress. Do not despawn an unresolved refused target.
6. Delegate battle detection/species/shiny/flee/catch/save to shared battle and
   encounter machinery. Never resume an invalid timed route after a battle:
   re-anchor or stop. Ball table must actually drive shared catch handling.
7. Status: selected profile, elapsed hunt/setup time, kills, encounters, refusals,
   refreshes, empty rounds, kill/encounter rates and stop reason. Counters do not
   prove target purity or distinct shiny checks. Add logged entered species.

## Implementation and tests

Use native C++ tests registered in PokemonSV_Tests.cpp, no Python app tests.
One failing behavior -> minimal implementation -> passing test, then next slice.
Keep game I/O behind a small action interface and a scripted fake. Coordinator
uses shared lower-level navigation/sandwich/battle helpers; no dependency from
shared helpers back to top-level programs. Keep test code in test source files.

Test priorities: valid/invalid target-type-location combinations; location
selection; per-target ball propagation; correct type sandwich before travel;
arrival failure; battle during travel; no-progress bound; refusal prevents
refresh/despawn; refresh returns to anchor; timer expiry; re-anchor after battle;
catch/save/flee handoff; finish/recovery states. Use data-driven cases with named
fields, clear expected/read failures, CRLF source and real header includes.

## Live gate

Only borrow hardware when the two configured routes and setup/hunt/encounter
flow compile/link and native behavior tests pass. Inspect release state, stop
at a safe point, save, close it, then launch the prototype with exclusive
controller/capture ownership. Handle camera/microphone permission and wait for
working video before Start. Run short bounded trials first at both actual
sites, then longer only if results justify it. Record target/competitor evidence,
setup/travel times, kills and encounters per minute, refresh yield and failures.

Preserve shinies, save, close prototype and return to Zero Gate; reopen release
AZP in zerogate mode and verify clearing before any handback is complete. Always
restore AZP on low quota or when live work finishes; reserve at least 20% before
starting another experiment. Do not leave the Switch idle for code analysis.

## One-time continuation

Armed for 2026-10-24 06:01 PDT (after the 05:59 reset). Detached timer PID
7576 was verified alive at 02:16. Local state/log/prompt are under
/Users/difan/.codex/timers/isolated-20261024-0601/.
It resumes the same Codex session once, first checking actual quota; it will
not start a model turn if quota is unknown or any window has less than 25%.
The resumed prompt requires honoring any newer pause/cancellation/scope change.
Delete its ARMED file to cancel before it fires. This is a one-time continuation,
not a permanent monitoring service. Read its state.json for actual execution
status; a scheduled timer is not evidence the later code or field run finished.

At 02:16 PDT release AZP PID 96268 was still running: 313 kills, 5 encounters,
0 shinies. No prototype instance owns hardware. Code branch is clean/pushed.

## 08:21 continuation correction

The timer fired at 06:01, but same-thread Codex resume failed because this
conversation still had an active writer. It exited 1, with no overnight coding.
No timer remains armed. User resumed at 08:21 and requested incremental linting
and generated test inputs for #41; that revision is now the immediate task.
Isolated code remains at 81172c9f0 until that task is finished.

## 08:29 lint adoption

#41 revised at user request: incremental changed-file checks only, no baseline
or stored fixture files; 22 tool tests and CI pass. Running its CLI against the
entire isolated diff initially found two violations. Prototype approach,
clearing-pass/session and encounter handling were split into private helpers;
shiny-panel registration was extracted to avoid worsening the existing factory.
All 12 changed C++ files now pass. Existing 29 native scenarios and full app
build/link pass; raw log build-lint-refactor.log. No live test or UI/route/sandwich
integration is claimed. Use the harness from its worktree via PYTHONPATH and
--root until the fork PR lands; do not copy prerequisite code.
