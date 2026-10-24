# Isolated encounter implementation queue

User steering 2026-10-24: finish sufficient end-to-end coding before borrowing
the Switch again. Issue dayfine/Arduino-Source#40 is first priority in the
current quota window. Resume this queue after 05:59 PDT, or earlier if #40 is
finished with allowance left. Query actual quota before substantial work.

## Starting state

- Code worktree: /Users/difan/Projects/Arduino-Source-isolated
- Branch: prototype/sv-isolated-hunt; committed ba454e4a8 adds verified return
  from Area Zero to Zero Gate. Two dirty prototype files add target selection
  and a staged sandwich preparation mode; they compile/link, but were written
  before the user's test-first direction. Treat these as scratch, not a tested
  solution; preserve the diff and rework through native test-first slices.
- Release AZP is restored, running PID 96268, first resumed kill 01:42:42 PDT.
  Leave it farming during implementation. See the 2026-10-24 run STATE/RESULT.
- No target route, sandwich, respawn movement or shiny acquisition is validated.

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
