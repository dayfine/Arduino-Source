# AutoStory unification plan (2026-11-08)

Plan for the next sessions, agreed with the user on 2026-11-08. It is the
single handoff for a clean session: read this, then
`CodingAgentContext/AgentProgramFramework.md` (fork PR #253, merged into fork
`main`), then start at [Stage 1](#stages-and-parallelism).

Goals, in the user's order:

1. Make the BDSP demo robust and finish the current run (Oreburgh Mine → Roark
   → Floaroma, rows 01.03–01.11 of `PokemonBDSP_AutoStory_StoryTable.cpp`).
2. Generate the story events for the rest of FRLG as data, not code, with the
   maps from PokemonSimulation #71 loaded into Arduino-Source.
3. Run through FRLG smoothly.

PLZA is out of scope for now (user: "don't worry about PLZA just yet").

## Re-prioritized 2026-11-08 evening (read this first)

The night's console runs showed that the order below was wrong: we kept
fixing one step's screen check at a time, finishing checkpoints by hand when
they failed, and moving on. Every 01.04 bug found today (night brightness, a
hand-counted tap total, the Potion menu under Roark's badge speech) was
already in the 2026-10-31 log. New order, highest first:

### P0 (process, now): no advance without a clean pass

A failed checkpoint is rerun from its own save until the program passes it
with no hand input; only then does the next one start (its save overwrites
this one's start). Never "fixed but not verified". Skill rule: #265
(`console-iteration` §6 step 7, §7 Save state). Memory:
`no-advance-without-clean-pass`.

- **01.04 is open, not passed.** Its start save was overwritten at 15:29 when
  01.05 started after a hand-closed Bag. It needs a save from before Roark
  (a replay from an earlier checkpoint, or a new game with the user's OK).

### P1 (shared foundation, both games): event-driven perception

Today each step polls one detector in its own window. Observed on console B
(2026-11-08):

- a banner that shows during the previous step is lost (the Gate, both ways:
  an open leg slid onto the warp and the banner came and went unwatched);
- a dim destination reads as a fade (the Gate's floor, brightness 46, under
  `FADE_DARKNESS` 75);
- one missed fade fails a real arrival (the Monte Carlo drops 01.05 to 19/20
  on a single injected miss at a door);
- a screen threshold measured by day fails at night (the gym check, street
  88 against 90).

Design: detectors (fade to black, fade to white, banner text, dialog,
battle, still screen) run continuously while moving and post timestamped
events; steps judge from the events since they started. Arrival through a
warp = fade **or** the far map's banner **or** a position that fits the far
map. Shared in `Pokemon/` with BDSP and FRLG bindings; the seeded
disturbance sim (#262, B6) is the gate: a row must survive any single injected
miss. Design note first (short), then implementation by a code agent.

### P2 (shared foundation): move fast where the end is certain

- A leg that ends at a wall, door or warp runs (held B + direction) and stops
  on the wall or the fade; counted D-pad taps only for legs that stop on an
  open tile. Out of the Oreburgh Gym took 19 taps, about 32 s; a run is about
  5 s. The mine hall and the Gate's row 22 are the same case.
- Door steps settle short (overworld back, still) unless the row marks an
  arrival scene (Mom's walk-in, Dawn's tour). Today every door pays the 5 s
  quiet rule: about 10 s after entering the Gate.
- Don't localize what the next move doesn't need: the Gate's route spent six
  wall clips pinning a column before a straight run west.

### P3 (in progress, agent): no open leg beside a warp

Planner/table rule with a red test: an open (stick) leg must not stop one
tile short of a warp while walking toward it; it drifts onto the warp. Plus:
the banner step must not take a dim cave floor for a fade, and must notice
when the player is already through. 01.05 entry fixed in #226 `95b2960c5`;
01.05's exit and 01.02's Gate entry have the same shape.

### P4: BDSP 01.05 → 01.11, each checkpoint a clean pass from its own save

Then the rest of the plan below in its old order (B1-B4, C1-C3). #263 (01.03
as JSON) needs rework first: pilot JSON on a stable row, not the one being
iterated on the console (review on #263).

### Status at 2026-11-08 16:05

| Row | State |
|---|---|
| 01.03 | **Passed** on console B 14:17-14:23, one program run (north route, mine doors black/white). Sim 76% (hiker "no plan", missed cave fades). |
| 01.04 | Fixed: night brightness check removed; 21 taps from the map's doors (red test); settle after a battle before the Potion; no Potion left ends the doses. Program beat Roark (third try). **Open**: start save gone. |
| 01.05 | Gate entry fixed (door run; passed on console 16:03). Gate exit: same bug; a one-run fix is local only, because it drops 01.05's Monte Carlo to 19/20 until a door counts fade or banner (P1). |
| 01.06-01.11 | Not run tonight. |

Open PRs from tonight: #226 (BDSP WIP), #262 (Monte Carlo), #263 (JSON,
needs rework), #265 (skill rule), the P3 agent's PR.

## Where things are

| Thing | Where | Notes |
|---|---|---|
| BDSP WIP | fork PR #226 `bdsp/autostory-mine-floaroma-wip` → `bdsp/sim` | Rows 01.02–01.03 fail in the mine (see [Diagnosis](#diagnosis-why-bdsp-0103-fails)). |
| Lab BDSP tree | `~/Projects/wt/dev-bdsp`, branch `lab/dev-bdsp`, tip `ebdbf45` | = #226 tip + `lab/device-ownership` + `lab/main`. See [A0](#a0-what-the-local-labdev-bdsp-commits-are). |
| FRLG stack | #213 stage1-core → #214 → #215 → #216 → #219 → #222 → #224 `frlg/stage7-celadon` (00.26) | Next checkpoints to author: 00.27–00.30. |
| Shared story engine | `SerialPrograms/Source/Pokemon/AutoStory/` (`Pokemon_StorySteps.h`, `Pokemon_StorySegments.h`, `Pokemon_StoryEvents.h`, `Pokemon_StoryStepJson.h`), owned by #122 `nav/story-steps` | Fork branches only, not upstream main. |
| Shared navigation | `SerialPrograms/Source/Pokemon/Navigation/` (Grid, MapData, RuntimeMap, TilePose, LegPlanner, LegWalker, WallRecovery, RouteLog), owned by #114 `nav/shared` | Change shared files only in their owning PR, with tests from both games. |
| FRLG checkpoint export | #251 `StoryCheckpointExport` (merged into stage1-core) | Produces PokemonSimulation's native format `pokesim-native-checkpoint-export/1`. |
| FRLG maps | #238 (merged): maps built from pinned Packages resources, embedded at build time via `overworld_map_json()` | Loading more maps = promote in PokemonSimulation + bump the resource pin. |
| PokemonSimulation | dayfine/PokemonSimulation `main`: #69 native capture format + `docs/story/CHECKPOINT_CONTRACT.md`, #70 `EXECUTION_PLAN.md`, #71 144 FRLG source-candidate maps (`data/games/frlg/maps/source-candidate/`, `pokeguy-map-scope.json`, not yet promoted) | Milestones: (1) export FRLG 00.05 + BDSP 01.03, prove parity; (2) strict parsing/binding around `StoryStepExecutor`, author both as data, delete duplicate C++; (3) complete FRLG plan as data; (4) bind incrementally. |
| Framework doc | `CodingAgentContext/AgentProgramFramework.md` (#253) | Concerns 2.1–2.8; FRLG/BDSP alignment in §4; what to take / not take from the external design note in §6. |
| Lab consoles | A: `lab-A` pid 6479 = the user's Max Lair fix run (slot-A deployment `5670ae2a86c0-88b1b40c`). **Do not touch.** B: lab console for BDSP (then FRLG). | Launch B with `python3 ~/Projects/Arduino-Source-agents/lab/scripts/start_dev.py B`; kill the old lab-B instance first. Logs: `~/Library/Application Support/SerialPrograms/Profiles/lab-B/SerialPrograms.log`. No lab-B instance was running at 2026-11-08 evening. |

## Diagnosis: why BDSP 01.03 fails

From the lab-B log (2026-11-07):

- 12:24 Onix used Bind → 9 Run presses → 60 s timeout →
  `escape_wild_battle: Exit transition and late main-menu fallback could not confirm escape.`
  at event 14 `below_the_coal_rooms_warp`. `escape_wild_battle` has no
  "can't escape" branch; it only presses Run until the timeout.
- 22:32 `below_roark: short_leg: south 3 moved 2`; 22:59
  `below_barry: short_leg: south 1 moved 0`; 23:06 `roark: nobody answered`.
  Open legs drift sub-tile; the `one_short_ok` hack on `lab/dev-bdsp` papers over it.
- The 01.03 start save (`after_the_welcome()`, outside the Center) is unhealed:
  lead paralysed, water move at 0 PP. Every reload replays the weakened party.
- `GridSim` cannot reproduce any of this: `walk_to_wall` always reports
  `wild_battles = 0`, battles are stubbed, there are no screens.

Root causes, in order of leverage: (1) checkpoint preconditions are not stated
or checked (healed? at a known anchor?); (2) the wild-battle policy is per game
and BDSP's is the weaker one; (3) the simulator cannot express encounters on
wall legs or battle outcomes, so fixes are proven on the console only.

## FRLG vs BDSP today (what to unify)

| Concern | FRLG | BDSP | Decision |
|---|---|---|---|
| Shared engine files | superset: `once_per_save`/`PreparedSteps`, `Outcome::changed_nothing`, `LegSpec::stalled_steps`, `LegResult.tiles` from interrupt timing (+225 lines) | merge base 64540276b | **B1**: land FRLG's additions in #122/#114; both chains merge down. CRLF→LF noise must be normalised back to CRLF. |
| Walking | shared `LegWalker`, timed holds, BattleFlash/BattleScreen/WhiteDialog watchers | own `Utils/Walk.cpp`: 400 ms chunks, screenshot diff, `WalkResult` origin CURRENT_LEG / MAY_PRECEDE_LEG | Keep both for now; the "every legal prefix" candidate idea is good and belongs in the shared walker later (B4 follow-up). |
| Wild battle | RUN_TRIES=4 then fight; statused → run then heal; low PP → nurse | Run until 60 s then throw | **B2**: shared `WildBattlePolicy` with FRLG's decisions; BDSP first consumer. |
| Re-anchoring | walls only (`OverworldReanchor`, `ReanchorPolicy`) | walls only (`Data/WallRecovery`, `WallPolicy`) | **B4** design: one `StopLocalization` module. |
| Start preconditions | save-card place check | `saves_in` | **B3**: real `Checkpoint::start` conditions. |
| Simulator | `OverworldSim` (screens, `SimTiming`, encounters, missed detectors; duplicate `OverworldSimWalker`) | `GridSim` (sub-tile drift, doors, wanderers; real walker via `RouteIO`/`GridSimRouteIO`; no screens, no battles) | Long term one grid-sim core with screens + encounters + drift (B4 follow-up). Tonight: give `GridSim` scripted encounters on wall legs so A2 has a red test. |
| Unused engine features (both) | `SegmentHooks::recover`, per-event `Recovery`, `Checkpoint::start` conditions, interrupts | same | B3 uses `Checkpoint::start`; the rest stays unused until a program needs it. |

## Decisions already made (do not re-ask)

- A1–A4, B1, B2, C1 approved as written below.
- **B3**: a checkpoint does not have to heal. If a Pokémon Center is close or
  along the way, use it; default on, off in a speed-run mode.
- **B4**: `StopLocalization` is deferred to a design PR, but the design is wanted.
- **C2**: promote maps in chunks as needed. Also investigate whether Packages
  resources can be loaded and unloaded at run time instead of being baked at
  build time (FRLG embeds the JSON at build time today).
- **C3**: author 00.27–00.30 as data story events, ideally.
- Saving over the current BDSP start save at the Oreburgh Center doorstep: **yes**.
- **No Repel**: Oreburgh does not sell it, and it would not solve the problem
  (later in the game Repel is the default anyway).
- Console B may switch to FRLG once BDSP reaches Floaroma.
- Parallelism under 3 agents; staged tasks that finish quickly per stage.
- **No CI polling.** Check CI status once when it should be done (+10/15 min).
- `DoorTransitionTester` dev program: **drop**, do not fold into #226.

## A0: what the local `lab/dev-bdsp` commits are

`lab/dev-bdsp` vs `origin/bdsp/autostory-mine-floaroma-wip`:

| Group | Commits | In #226? | Action |
|---|---|---|---|
| Walk fixes (wandering NPCs, soft hazard cost, encounter origin, chunk origin) | adc8b9e97, 7e19883dc, 4dcbfd89b, b36ae1256 | yes, identical content | nothing |
| Lab tooling (`lab/device-ownership` merge, Developer Tools in profile builds a3160e496, source groups a5b1431b8) | — | no, lab-only | nothing |
| Center door fix: `pokemon_center_inside()` check before `enter_lit_door` (`StoryExecutor.cpp`); `run_into_door` `BlackScreenWatcher(... FinderType::PRESENT, 50ms)` + a log line per outcome (`StoryStepIO.cpp`); `below_oreburghs_center` = `at(OREBURGH, "pokemon_center_doorstep", NORTH)` instead of (47,21) (`StoryTable.cpp`) | 1572e1749 | no | **fold into #226** |
| `DoorTransitionTester` program + Panels/CMake registration | 1572e1749, 3e855, d5e99 | no | **drop** (user) |
| `one_short_ok` variant on `below_roark` | ebdbf45 | no | **drop**; A3 replaces it |

After A0 the lab tree must equal the #226 tip (plus lab/main), so console runs
test the PR and nothing else.

## Track A: BDSP — finish the run

- **A0** Fold the Center door fix into #226 (without the tester), rebuild
  `lab/dev-bdsp` from the new #226 tip + `lab/main`. Drop `one_short_ok`.
- **A1** New row **01.02b "Oreburgh Center heal"**: start `after_the_welcome()`
  → walk to `pokemon_center_doorstep` → `run_into_door` → heal → exit → save on
  `out_of_oreburgh_center()` (doorstep, facing south). This overwrites the
  current start save (approved). Before saving, confirm the start pose with a
  screenshot against the map; a failed step means an unknown spot.
- **A2** Can't-escape branch in `escape_wild_battle`
  (`PokemonBDSP_AutoStory_WildBattle.cpp`): after N Run presses with the battle
  menu back, switch to Fight with a move that still has PP until the exit
  transition or the foe faints; reuse the `battle_1_pokemon` code. Red
  `GridSim` test first: a wall leg with a scripted trap encounter. Once B2
  exists, this becomes the BDSP binding of the shared `WildBattlePolicy`.
- **A3** `below_roark` must end with `to_walls` against a rock beside Roark
  (check the mine map) so the last leg is clipped, not counted. Then drop
  `one_short_ok`.
- **A4** Console B: kill any lab-B instance, `start_dev.py B`, run 01.02b →
  01.03 → … → 01.11 (Floaroma). On failure: stop the program, keep game and app
  open, look at the failure frame, fix once, rerun (skill `console-iteration`;
  hard stops: 2 same-step fails, 3 builds, 45 min). Notes in a file, not
  narration. When Floaroma is reached, console B may switch to FRLG for 00.27.
- Every console failure becomes a sim regression test (red first).

## Track B: shared engine

- **B1** Land FRLG's shared `Pokemon/AutoStory` and `Pokemon_LegWalker`
  changes in the owning PRs (#122 story steps, #114 navigation), CRLF
  preserved, with tests from both games; merge down both chains (BDSP #167/#226
  and FRLG #213…#224). Merge, never rebase.
- **B2** `WildBattlePolicy` in `Pokemon/AutoStory`: data (run tries, fight
  fallback, status → run then heal, low PP → nurse) + the decision function;
  the game supplies the battle actions. FRLG's current behaviour becomes the
  default policy; BDSP binds it in A2.
- **B3** Real `Checkpoint::start` conditions, built by `build_story_checkpoint`
  and rechecked on reload: save place (map + anchor point), at-anchor pose
  (door, PC, story location after a cutscene, or map border clipped to a wall),
  and `healed` only when the row says so. Heal when a PC is close or on the
  way (default on; off in speed-run mode). Codec fields in
  `Pokemon_StoryStepJson.h` so the export (#251 / PokemonSimulation
  `CHECKPOINT_CONTRACT.md` entry requirements) carries them.
- **B4** Design PR (not implementation): one `StopLocalization` module =
  OverworldReanchor + WallRecovery + trainer-sight + `slide_to_wall`, with a
  `RouteIO`-style orchestrator so the same walker runs in `GridSim` and on the
  console, and one grid-sim core (screens + encounters + sub-tile drift) that
  both games share. Also note BDSP's "every legal prefix" candidate idea and
  FRLG's missed-detector model as inputs.
- **B5** (user, 2026-11-08 12:30; implementation, not design) Recovery
  moves toward the goal, both games. A re-plan after an interruption (wild
  battle, NPC block) targets the end of the step, not the end of the
  interrupted leg, and skips the step's remaining legs. A counted step that
  can be interrupted (encounter tiles, wanderers) must end on a wall-bounded
  anchor (corner, wall, door, sign), so the re-plan ends in clips. Shared
  walker (`Pokemon_LegWalker`, #114) and story steps (#122) with tests from
  both games, plus a table lint for both games: interruptible counted steps
  ending on an open tile fail. BDSP's own `tap_legs` gets the same rule first
  (#226, Roark's corner (20, 28); lab-B 01.03 11:55: leg-end recovery ended
  on "south 5 (open)", overshot a row, walked under Roark's rocks).
- **B6** (user, same; implementation) One seeded disturbance sim for both
  games: pinned seeds per row; per seed wild battles on encounter tiles,
  sub-tile drift on holds/open legs, wanderer seed/start, one missed
  fade/banner; pass rate per row and failing steps with seeds. Shared runner
  and disturbance model in `Pokemon/`; bindings over `GridSim` (BDSP, first)
  and `OverworldSim` (FRLG, has encounters + misses already). Default seeds in
  the unit suite; heavy mode (200-1000) on demand. Gate: a console run of a
  row only after its seeds pass. Folds into B4's one grid-sim core.

## Track C: story as data (PokemonSimulation #69/#70/#71)

- **C1** JSON loader: `read_story_checkpoint` → FRLG `build_story_checkpoint`.
  Round-trip FRLG 00.05 (`data/story/native/frlg-parcel-00.05.json`) and prove
  FakeGame trace parity between the C++ row and the loaded data. Then a BDSP
  01.03 export adapter (PokemonSimulation milestone 1 needs both).
- **C2** Promote the maps 00.27–00.30 need in PokemonSimulation (Route 9/10,
  Rock Tunnel, Lavender, Route 8, Underground Path, Celadon), bump the resource
  pin in Arduino-Source. Chunks as needed. Investigate run-time
  loading/unloading of Packages resources (today `overworld_map_json()` embeds
  at build time); report the options before changing the loader.
- **C3** Author 00.27–00.30 as data story events against the #69 schema,
  sim-passing, authored-vs-executable coverage stated as #70 asks.

## Stages and parallelism

Each stage is a short, bounded task per agent (≤3 agents in parallel); agents
push, report the PR number and tip SHA, and stop. The main session owns console
B and does the lab-tree edits itself.

| Stage | Main session (console B) | Agent 1 | Agent 2 | Agent 3 |
|---|---|---|---|---|
| 1 | A0 fold + A1 row in the lab tree; prepare the Center-doorstep save (screenshot first) | B1: FRLG shared changes → #122/#114, CRLF, both-game tests | A2 red GridSim test (scripted trap encounter on a wall leg) + can't-escape branch in `escape_wild_battle` | C1 loader + 00.05 round-trip/parity test |
| 2 | A3 `below_roark` to_walls; A4 run 01.02b → 01.03 | B2 `WildBattlePolicy` (after B1 lands), BDSP binding | B3 `Checkpoint::start` conditions + codec | C2 map promotion chunk 1 + pin bump; run-time resource loading findings |
| 3 | A4 onward to 01.11 | B4 design PR | C1 BDSP 01.03 export adapter | C3 00.27–00.30 as data |
| Morning | Handoff note, run record, PR tree | | | |

CI: do not poll. Note the push time, check once at +10/15 min.

## Rules that bind this work

- Fork drafts only: `~/.local/bin/gh --repo dayfine/Arduino-Source --draft`.
  Never open, reopen or mark ready a PR on `PokemonAutomation/Arduino-Source`.
- At the start of each session: `git fetch origin agents`, read
  `origin/agents:AGENTS.md` and `origin/agents:pokemon_sv/PR_WORKFLOW.md`.
- Shared `Pokemon/AutoStory` and `Pokemon/Navigation` files change only in
  their owning PR (#122, #114) with unit tests from both games.
- Keep CRLF in CRLF files. No `std::thread`/`std::mutex`/`std::async`.
  Control-flow braces per `AGENTS.md`.
- Review rounds → new commits on top; merge the base in, never rebase.
- Console B: one train at a time; never stop a program to probe; a checkpoint
  run saves at its start, so confirm the start pose first. Close lab error
  dialogs by the lab pid only. `sips -Z 640` screenshots before reading them.
- Notes and run records go on the `agents` notes branch (this file's home),
  never into code branches.

## Open questions for the user

1. B3 "speed-run mode": a program option on `StoryRunOptions`, or a per-row
   flag? (Default proposal: one option, default off.)
2. C2: if Packages resources cannot be loaded at run time without new infra,
   do we accept build-time embedding for the rest of FRLG, or invest in a
   loader now?
3. Once BDSP reaches Floaroma, the user deposits Mew/Jirachi to HOME by hand
   before console B switches to FRLG — confirm when.

## Morning deliverables

- `~/BDSPLab/notes/2026-11-09-next-session.md` (state + do next; the session
  log goes in its own file) and a run record under `autostory/runs/`.
- PR tree with "Depends on" banners (B1 changes the base of several PRs).
- This plan updated: what landed, what moved, what is still red.
