# AutoStory review: prove map reuse in a FireRed demo

Status: proposed plan revision, 2026-10-28. This updates implementation order;
it does not record new user approval of a fixed interface or hardware budget.
Read alongside [AutoStory.md](AutoStory.md). Prepared as a local handoff for
fork draft #42; no existing PR branch was changed.

## Evidence and scope

- Existing design: `0443b0332264c79a7b6a8ffa6ade30a92c0587fc`.
- SV extraction draft stack #43–47: `autostory/r5-dev-tests` at `bad944993`.
- SwSh upstream #1207: `32d506c273879244e0ffc10c59ee01da2d30865e`.
- BDSP upstream #1717 baseline: `65094d4cd`; our segment work starts from
  `bdsp/1717-seg00`, with the reviewed map/walk implementation present at
  `bdsp/new-game-intro-v2:6dbcc8e10`. The active branch advanced during the
  review to `a5f91acde`; that is not a frozen validation build.
- Source and recorded lab lessons were inspected. No console execution or
  new build/test run was performed for this documentation review.

## Recommendation

Use the existing shared map/planner and screen modules as candidate
implementations. Build a small FRLG adapter and an observed story slice
before settling a common route executor. A small prerequisite extraction
is justified when the demo calls it immediately. Migrating the Elite Four
farmer, SV registries, or all of BDSP is not a prerequisite.

There are two demos with different purposes:

1. **Startup demo:** fresh FireRed, preset names and explicit starter,
   bedroom → Oak → starter → lab rival resolved → verified first save.
   This tests screen handling, short walks, battle outcomes and the first
   save. Before a verified save exists, recovery must explicitly restart
   the fresh-game flow or stop; it cannot load an assumed checkpoint.
2. **Map demo:** from a prepared, verified post-rival save, leave the lab,
   cross Pallet/Route 1 into Viridian, collect Oak's Parcel, return and
   deliver it, then verify the Pokédex and save. This demonstrates an
   outdoor map connection, building entry/exit, NPC interaction, a grass
   interruption, re-localization and checkpoint recovery. First run it
   independently from that prepared save, then chain it to startup.

The map demo is the more useful architectural experiment. It may be built
first while new-game detectors are still being collected. A single starter
and English are enough for the experiment; broader starter choice remains
an implementation decision. The lab-rival routine must report the observed
outcome: story progress, health and level assumptions depend on it.

## What to reuse from SV

Drafts #43–46 already implement selection/range helpers, shared stats and a
retry loop. The common files total about 195 physical lines, with another
160 lines of SV hooks. Reuse their implementation through an explicit
prerequisite/reviewed extraction, not a second copy. #47 moves an SV-only
developer harness and contributes nothing required by FRLG.

The retry draft preserves SV's legacy behavior: the default limit executes
102 attempts, and a failed initial save can be followed by running the
action after a reset without establishing a new checkpoint save. That is
not the FRLG recovery contract. Keep SV compatibility explicit; FRLG needs
an exact attempt budget and a verified rollback location. A reset failure
must stop, not be counted as a successful recovery.

Keep SV navigation, battle/dialog interpretation, options and registries
in SV. Claim Mystery Gift calls the exported SV segment list; a generic
registry rewrite would affect another existing program.

The current fake tests cover five retry cases and three index cases. They
do not prove the SV adapter, reset-failure propagation, a second game, or
hardware operation. Existing draft implementation is distinct from rollout
approval and cross-game validation.

## What BDSP has already made reusable

Paths below are under `SerialPrograms/Source/`.

| Module | Present implementation | Reuse decision |
|---|---|---|
| `CommonTools/ScreenMachine/` | Screen matching, stable observations, bounded visits, transitions, stall checks and StepTrace | Reuse for FRLG screens; supply FRLG detectors/actions and test actual timeout/terminal behavior |
| `Pokemon/Navigation/Pokemon_TileGrid.*` | Tile parsing, stepping, wall hits, path search, trainer sight and possible-position planning | Already in shared code; reuse the supported subset with FRLG fixtures |
| `Pokemon/Navigation/Pokemon_MapSet.*` | Places adjoining maps on a single grid and translates local/world coordinates | Reuse only for physically adjoining outdoor maps |
| BDSP `AutoStory_RouteWalk.*` | Execution, timing estimates, banners, trainer dialogs, obstacle handling, replanning | Keep the BDSP implementation; derive a smaller shared execution policy after a working FRLG adapter |
| BDSP `AutoStory_Walk.*` and matchers | Screen motion, interruptions, battle-vs-transition checks and landmarks | Keep perception, timings and battle decisions per game |
| BDSP maps, trainer locations and story checkpoints | Hand-authored and console-corrected story data | Keep in BDSP; FRLG supplies its own reviewed map data |

At the reviewed BDSP revision, ScreenMachine/StepTrace total 329 lines;
TileGrid/MapSet total 932. They are already in shared folders: extracting
these again would duplicate work. Generic navigation tests currently live
inside BDSP's AutoStory test registration; separate pure map/planner tests
from BDSP map fixtures when FRLG becomes a consumer.

Recorded evidence is limited: the lessons document reports 00.04 completed
unattended in r79; 00.05 was stepped through to the PC and subsequently
coded. Those are different evidence levels, neither a repeated FRLG run
nor proof that the executor is generic.

## Map structure to test

Use these responsibilities as a working division, not a frozen C++ interface:

- **Map data per game:** terrain, directional traversal, connections,
  warp destinations, scripted triggers and NPC/trainer metadata. Maintain
  provenance and a pinned source revision. For FRLG, inspect decomp map,
  layout, collision/behavior and script data together; a picture or event
  coordinate alone is not proof a tile is traversable.
- **Shared spatial model and planning:** positions, supported traversal,
  route search, candidate-position propagation, anchor goals. Keep console,
  OCR, controller and battle dependencies out of this module.
- **Per-game observation and movement adapter:** translate a leg to inputs,
  interpret a stopped camera/battle/dialog, and provide position evidence.
  FRLG frame timing and BDSP analog movement remain separate.
- **Story execution per game:** resolve the interaction or battle, verify
  its postcondition, update story facts, then continue or reset to a known
  save. Share common orchestration only where both adapters exercise it.

A map coordinate needs a map identity. `MapSet` is a joined outdoor canvas;
a Mart interior or upstairs room is not another adjacent rectangle. Treat
warp/door actions as explicit transitions, verify the destination and its
arrival tile, then plan on that map. The current planner deliberately
rejects doors and unknown edges; preserve that protection during the demo.

Keep static terrain separate in meaning from current occupancy and story
facts. A wandering NPC blocking a step is temporary; a beaten trainer may
still block a tile; a door may lead into a cutscene. The first demo needs
only its actual states and transitions, not a universal world simulator.

Limits to resolve before calling the existing map implementation general:

- `TileType::LEDGE` currently supports southward jumps only. Add other
  traversal rules only where the selected FRLG route requires them.
- `at_wall()` treats NPC tiles as anchors. A movable NPC is not a durable
  position reference. Use fixed terrain for demo anchors.
- `TrainerSight` is a straight ray with a fixed facing/range. Do not infer
  an encountered trainer's identity solely from list order when multiple
  sight lines fit. Dynamic facing and defeated flags need game evidence.
- The robust planner proves a result only within its supplied map and
  movement-error model. `planned.after` is a prediction, not a screen read.
  Unknown position must remain unknown until an anchor/landmark resolves it.
- BDSP's current executor estimates distance from elapsed movement, can
  mark a singleton inferred blocker, and picks `watching[0]` for a trainer.
  Those are BDSP policies needing evidence, not contracts to export.
- A bounded failure to find a robust route means no route under this
  search/model, not that the game destination is unreachable.

For the demo, use a small hand-reviewed map set. A complete decomp importer,
all Kanto maps and automatic runtime route optimization are unnecessary.
A planned path may be computed offline or at startup; live position
verification is required in either case.

## Revised work order and gates

1. **Record the map-demo contract.** Pin FireRed/version and source maps;
   step through the route with short commands, capture entry/exit anchors,
   and record the exact post-rival starting save and parcel/Pokédex states.
2. **Build one runnable map slice.** Reuse the needed shared map subset;
   add the FRLG movement/observation adapter, fixed terrain anchors,
   one outdoor connection and a verified building transition. Reuse FRLG
   wild-battle exit. Keep a bounded, FRLG-specific orchestration loop.
3. **Complete the parcel round trip and recovery.** Exercise a wild battle
   mid-leg; re-anchor rather than replay a full open timed leg. Inject a
   recoverable failure after a verified save, reload, verify the anchor,
   and complete the checkpoint. Stop on failed localization/reset.
4. **Complete startup and chain both demos.** Reuse ScreenMachine and
   StepTrace with FRLG observations. StarterRng's rival code supplies
   examples, but its method is coupled to RNG filters, EV accounting and
   level-up reads; it is not a drop-in story battle function.
5. **Compare two real adapters.** Extract only common position bookkeeping,
   recovery decisions or transition handling that BDSP and FRLG now share.
   Keep timings, OCR, battle outcomes and game scripts local. Add native
   tests at that interface and rerun both game slices after extraction.
6. **Expand to Brock.** Add trainer approach/battle cases, progression,
   health/PP and move-learning handling as the route requires. Consider E4
   walker migration as a separate regression-tested change, not a gate
   before the demo. SV adoption remains a separate rollout.

Validation must cover the model and its use: forbidden door/unknown-edge
planning, one-way ledges, possible starts converging at fixed terrain,
map identity across a door, temporary obstruction, battle interruption,
unknown-position abort, failed save, failed reload and exact retry count.
Screen fixtures test game observations; scripted action tests test orchestration;
console runs test their integration. Existing BDSP route tests mostly check
planned endpoints/geometry, not this execution/recovery matrix.

Require three consecutive autonomous completions from the declared start
for each completed demo, plus the deliberate failure/recovery run. Record
outcomes, resets, false arrivals, and Switch time. Step-through success and
one unattended run are milestones, not repeated-validation claims. The
existing Brock gate remains the expansion decision; its proposed three-hour
average budget has not become a new public commitment in this review.

## SwSh comparison

SwSh #1207 contains about 560 lines of new implementation/header and calls
seven segments directly. It has timed routes plus limited visual feedback
for transitions, battle menus and catching. It has no verified checkpoint
runner. Some helpers return normally on timeout or discard catcher failure
outcomes; navigation repeats a whole timed action after interruption.

This reinforces the need to retain action outcomes and verify destination
states. It provides no second adapter validating the proposed shared runner.
No source tests or gameplay-validation report were found; passing build CI
is a different claim. Do not extract these helper contracts into shared code.
