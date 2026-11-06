# AutoStory sharing: FRLG, BDSP, SV, LGPE and Ruby/Sapphire

Status: **draft architecture review**, based on the revisions listed below.
This updates the sharing assessment in [AutoStory.md](AutoStory.md) and
[AutoStory-Navigation-Stack.md](AutoStory-Navigation-Stack.md). It proposes
incremental work; it does not add a new game or claim complete native/offline parity.

The direction is one story execution contract, explicit checkpoint requirements,
and interchangeable game adapters. AutoStory authors the story objectives and
executes the selected plan on hardware. PokemonSimulation owns map preparation,
offline route planning and evaluation. Its replay must consume the same selected
plan and policies that hardware executes.

Concrete data and executor/ruleset/map interfaces are sketched in
[AutoStory-Interface-Examples.md](AutoStory-Interface-Examples.md).

## 1. What exists, and what remains different

“Shared” below means implemented shared code in the inspected fork stack, not
necessarily merged into Arduino main. FRLG and BDSP refer to the fork AutoStory
heads; SV distinguishes main from the pending extraction stack.

| Concern | FRLG | BDSP | SV |
|---|---|---|---|
| Story orchestration | Shared `StoryEvents`, `StorySteps`, `StorySegments`; game executor | Same shared engine; game executor | Imperative segment/checkpoint classes and action lambdas on main; no adoption of this event runner |
| Selection and stats | Shared utilities in the fork stack | Shared utilities in the fork stack | Main still has local stats; fork `autostory/r3-shared-stats` uses shared selection/index helpers and `Pokemon::AutoStoryStats` |
| Retry/save | Usually save after a successful checkpoint; supplies snapshot; RNG handoffs | Usually save at checkpoint start; save-card location guard; STOP rows | Separate normal/tutorial retry loops; reset, notifications and optional day skip |
| Preconditions | Starting pose and per-row supplies; several live bag/move/save checks | Starting pose, starter choice, expected save-card place | Mostly start/end descriptions plus scattered runtime setup checks; e.g. checkpoint 21 expects a level-100 Gardevoir |
| Maps | Shared `MapData`; FRLG step/warp/elevation rules | Shared `MapData`; joined `StoryWorld`, local/world conversions | Map UI, fly points, minimap markers, camera/heading alignment and visual navigation; no use of this collision-grid contract |
| Route planning/recovery | Shared leg search and wall-anchor search; game movement adapter | Shared ordinary leg search and wall-anchor search; game-specific robust belief-set search | Screen/marker-driven travel and unexpected-battle wrappers; no shared tile-anchor search |
| Battle decisions | Tracks lead, moves/PP, supplies and opponent data; game-specific loop | OCR move scoring and a simpler potion/switching loop | Own battle helpers used by imperative checkpoints; not the FRLG/BDSP policy engine |
| Complete semantic export | Not yet | Not yet | Not yet |

### Existing shared event engine

`Pokemon/AutoStory/` already separates ordering and retry orchestration from game
execution. `StoryStepExecutor` dispatches route, talk, battle, heal, shop and other
step kinds. The lower-level `Event` supports require/expect/skip checks, outcomes,
interrupts and recovery; `Segment` supplies checkpoint attempts and reload hooks.
Game-specific scenes and controller operations belong in their executors.

The data-level `StoryCheckpoint` is less expressive than the lower-level engine:
it primarily contains a starting pose and steps. `build_story_checkpoint()` adds
a `from_save` event that assigns that declared pose; it does not independently
observe the player's tile. `RunState` carries pose and flags, not a complete
party/bag/money ledger. A callable condition's human-readable name is not an
executable, portable condition specification.

There is also branch divergence to resolve in the owning shared PR: FRLG has
`Outcome::changed_nothing`, `StoryStep::once_per_save` and `PreparedSteps`; the
inspected BDSP head does not. These implement reusable preparation/reload
semantics, not an FRLG-only mechanic. Promote them through the shared owner and
merge down, with tests distinguishing a read from an action a reload undoes.

### Map agreement does not imply movement agreement

Both tile games use `Pokemon::MapData`: glyphs, named points, regions, objects,
warps, triggers, flags, connections, elevations and corrections. Both call shared
`plan_legs()` and `find_wall_anchors()` algorithms through game models.

FRLG routes track map-local position, elevation, surfing and facing, with edge
connections, warp entry modes and arrival displacement. Its planner currently
avoids ledge hops; story steps can request those explicitly. BDSP places maps on
a combined grid and plans over uncertain position sets, including open-leg drift.
Its robust planner excludes doors and leaving loaded maps; several transitions
are separate screen-driven story steps. BDSP supports ledge hops in its step model.

Keep those differences explicit. Share map identity, coordinate-frame conversion,
transition descriptions, dynamic-world overlays, search infrastructure and replay
records. Supply collision, transition activation/arrival, uncertainty expansion
and safe-anchor rules through game adapters. A door, person, sign or unloaded
map edge is not automatically a safe wall. Predicted arrival and observed arrival
must remain separate.

Terrain authoring/corrections and immutable map exports belong in
PokemonSimulation. Story state changes should select overlays (object visibility,
trainer state, opened obstacle), rather than editing base terrain to make a route
pass. BDSP data derived from DP needs identified BDSP corrections and provenance.
A shared importer must preserve unverified/unresolved transitions explicitly.

## 2. The common contract worth building next

Share the semantic contract across languages; use one native C++ runner and one
independent Python evaluator. Avoid duplicating game-plan decisions in a second
handwritten simulation script.

```text
story objectives + starting profile + chosen options
  -> selected plan with typed actions, requirements and policy versions
     -> native executor + game/controller/detector adapters
     -> offline replay + game/data/mechanics adapters
  -> comparable semantic traces and complete battle-entry states
  -> evaluation: unsupported state, preparation gaps, reliability and cost
```

### Checkpoint requirements, effects and save identity

A common checkpoint contract should contain:

- Stable plan/checkpoint/step IDs, source revision, game/version and data/map pins.
- Starting requirements and required ending effects: location/pose confidence,
  screen, flags, party composition, moves, level/EXP, HP/status/PP, bag, money,
  badges and traversal capabilities. Games supply namespaced predicates where
  a generic predicate is insufficient.
- Evidence for each relevant value: observed, replay-derived, caller-assumed,
  bounded/ranged, or unknown. A declared full-HP requirement is not a heal.
- Save placement policy, a save-generation identity, reload target and retry
  budgets. Reload restores game state; observations/preparation caches must be
  invalidated or reconciled against that same save generation.
- Effect/result records, including resources actually consumed and uncertainty
  introduced. An unreadable value must not silently become a successful check.

The existing FRLG save-at-end and BDSP/SV save-at-start policies can both fit;
sharing orchestration must not silently change them. A wrong starting state must
fail before overwriting a useful save. Map-name agreement alone does not prove
exact tile, facing, team or inventory agreement.

Preparation caching needs particular care: checking an already-known move can
be reusable after reloading the same save; teaching a new move must run again if
the reload undid it. SV's day skip and re-save also change state, so they need
explicit effects and save identity rather than being treated as harmless retries.

### Resource/preparation planning

Use objectives such as “member with traversal capability X”, “level at least N”,
“required attack has PP remaining”, or “bag contains at least two healing items”.
Resolve them into explicit capture, training, evolution, learning, shopping,
pickup and healing steps. Track stable party-member IDs across evolution.

AutoStory owns the objectives and permitted choices. An offline planner here can
propose an acquisition/preparation route; select and export that plan for both
executors. Validate affordability, stock/unlock availability, consumable TMs,
move replacements, EXP participation, item consumption and traversal prerequisites
with the game adapter. Optional purchases and required preparation must have
different outcomes: skipping an unaffordable optional item cannot satisfy a
mandatory bag requirement.

Illustrative requirement, not an implemented schema:

```json
{
  "checkpoint": "before_required_trainer",
  "requires": [
    {"predicate": "party.knows_move", "member": "support-1", "move": "cut"},
    {"predicate": "bag.at_least", "item": "potion", "quantity": 2}
  ],
  "on_unmet": "plan_preparation_or_stop"
}
```

“Cut” is a move requirement for a plan that needs it; a portable traversal
objective should use a capability, because another game may unlock that action
through a story technique or ride rather than an HM-bearing supporter.

### Battle interface and rulesets

Share battle-state observations, legal semantic actions, player policy
orchestration, inventory effects, battle-entry validation and result reporting.
Use A/B/C as complexity labels; an executable policy still needs a version and
parameters. Keep NPC AI separate from our player's policy. UI reading/menu inputs
remain game/controller adapters.

Choose mechanics and data by exact game/version/ruleset, not generation alone:
FRLG and Ruby/Sapphire are both Gen 3 without identical data or all identical
behavior; BDSP is not a DP Gen 4 ruleset; LGPE needs its own rules; SV needs its
own rules and story-specific encounters. Doubles, multi-stage/scripted battles,
abilities, held items and special mechanics require explicit support or an
unsupported result. No existing simulator is assumed to support all five games.

The offline interface must emit whole-party battle entries and actual policy
choices, then reconcile EXP, stats, HP/PP, moves, money and bag effects. Independent
per-fight entry tests and a stochastic full-run evaluation are different scopes.
A successful-looking fake battle that updates none of these is not plan validation.
See [the PokemonSimulation replay contract](https://github.com/dayfine/PokemonSimulation/blob/main/docs/battles/autostory-replay-interface.md).

## 3. SV: useful refactors without replacing its navigation

SV already provides useful game-specific pieces: checkpoint/segment selection,
repeated-checkpoint developer runs, save/reset, notifications and stats,
normal/tutorial retry loops, setup checks, camera/marker alignment, fly-point
navigation and unexpected-battle handling. Reuse the working game routines.
The pending [shared selection/index/stats stack](https://github.com/dayfine/Arduino-Source/pull/45) is a small extraction; it is not
proof that SV runs on `StoryEvents`/`StorySegments`. The older retry extraction
[fork PR #46](https://github.com/dayfine/Arduino-Source/pull/46) is closed; assess current code rather than counting that proposal as adopted.

Recommended migration:

1. Characterize existing retry behavior through an injectable save/reset/action/
   notification seam. Preserve tutorial handling, attempt callbacks, post-action
   menu checks, telemetry, day-skip behavior and saved stat keys. Specify actual
   attempt counts before changing them; main's zero-based loop and `i > 100`
   condition permit 102 failed action attempts, not a strict 100-attempt budget.
2. Wrap one existing checkpoint as a shared runner action, retaining its existing
   movement and battle code. Only one layer owns retries. A wrapper is an
   orchestration migration; its opaque body remains unsupported for detailed
   offline state evaluation until annotated or decomposed.
3. Add typed preconditions to a checkpoint such as SV checkpoint 21, whose start
   description expects level-100 Gardevoir at Mesagoza West. Verify available
   observations; retain unknowns and an explicit external-preparation boundary.
   Do not infer that the story caught/trained that team from the description.
4. Expose its major effects and landmarks as semantic events; use the shared
   trace/replay contract for coverage, resource and policy diagnostics.
5. Migrate additional checkpoints only after behavior-preserving tests and a
   small live acceptance run. Retry-limit changes are separate behavior changes.

SV's location adapter should describe regions, landmarks/fly points, heading,
camera and traversal mode with confidence. The shared `Position` abstraction can
carry a game-specific position; the tile-oriented `ROUTE`/`LEGS` vocabulary should
not be forced onto SV. Keep visual navigation as its adapter, with bounded
relocalization and explicit failure. Do not claim a collision-complete SV map or
reuse FRLG's wall clipping as SV recovery without evidence.

The largest SV benefits are structured preparation/resume checks, tested retry
boundaries, explicit handoffs, traceable failures and replayable party/resource
state. Sharing the 2D world representation is a lower priority and may be inappropriate.

## 4. Let's Go Pikachu/Eevee: reuse orchestration, prove capture first

Existing `PokemonLGPE` code includes Joy-Con game-entry/reset helpers, battle
arrow inference and several farming/reset programs. There is no AutoStory module
in the inspected main tree. These are reusable primitives, not an established
walking, catching or story executor.

Reuse the runner, requirements/effects, party/bag ledger, trace format, policy
interface and offline evaluation protocol. Candidate map data can use the common
schema when it accurately represents the game. FRLG's Kanto topology may suggest
scenarios, but its tiles, trainers, scripts and arrival offsets are not LGPE data.
No verified complete LGPE map source is selected by this review.

New adapters/work that determine feasibility:

- **Controller/capture:** existing game-entry APIs take `JoyconContext`, unlike
  the tile games' Pro Controller executors. Prove that our supported controller
  transport can perform the required throw interaction in the selected play mode,
  then detect throw, catch/failure, flee and return to overworld. Official controls
  differ between Joy-Con motion play and handheld aiming; button timing alone is
  not evidence of a supported automated throw.
- **Encounter/progression:** normal wild encounters enter a catching interaction;
  trainer battles and special encounters need their own outcomes. Model capture
  objectives, balls/berries, observed catch results and resulting party/EXP changes.
  Do not reuse “fight wild battles for training” unchanged. Exact catch/EXP formulas
  and any combo/candy/partner-specific effects need separate validation.
- **World/capabilities:** measure motion error, collisions, map transitions and
  arrival position/facing. Record story technique unlocks and entrance requirements
  as capabilities/preconditions, rather than assuming an FRLG HM plan applies.
- **Battle rules/data:** support the chosen partner/version, its move availability
  and LGPE-specific stat/mechanics inputs; do not substitute FRLG or generic Gen 7.

First slice: one saved overworld location, one controlled encounter/catch attempt,
confirmed return/localization and a resource/EXP effect record; then one trainer
battle and a checkpoint replay. Stop expansion at an unsupported controller action
or unknown data/mechanics boundary. No claim that existing reset programs prove it.

The [official LGPE controls](https://pokemonletsgo.pokemon.com/en-us/how-to-play/index.html)
confirm the encounter/throw and controller-mode distinctions. Full mechanics and
map extraction remain research tasks, not established by that overview.

## 5. Ruby/Sapphire: closest structural reuse, separate game rules/data

Existing `PokemonRSE` code includes menu/dialog detectors, starter/gift/legendary
reset programs and navigation helpers. `PokemonRSE_Navigation.h` concerns reset,
menus and encounters; it is not a story map/path engine. There is no AutoStory
module in the inspected main tree.

The [pret/pokeruby source](https://github.com/pret/pokeruby/tree/5784633ce4ef7ade1a7f2d2d0c288e3d5e6cdd7f)
provides a Ruby/Sapphire data-source candidate. Add a pinned importer in
PokemonSimulation, preserving source revision, target edition/language/revision,
map coordinate frames and verified corrections. Reuse parsing/building primitives
where formats match; do not assume the FRLG importer or pokefirered data works
unchanged. Emerald is a separate target, not an implicit fallback for R/S.

Reuse the common map contract, leg/wall search, story runner, requirements and
replay format. A Gen 3 mechanics implementation is also a candidate for reuse
behind an R/S ruleset and edition-specific trainer/move/item/AI data.

Implement and test R/S movement policies from source: collision/elevation,
connections, door/cave/ladder activation and arrival, ledges, water and the travel
modes the selected route needs. Validate bike, current/forced-movement and other
special terrain before enabling routes through them. Measure input timing and
screen detectors on the actual deployment platform; similar graphics and D-pad
movement do not establish FRLG controller timing or detector compatibility.

Party preparation must include traversal moves/supporters, move teaching,
badges/unlocks and resource availability. Validate R/S EXP/EV participation,
learning/evolution and trainer AI with the selected rules/data, including doubles
when the chosen route requires them. The existing `flee_battle()` explicitly
assumes guaranteed escape (Smoke Ball); it cannot be used as a general new-game
wild recovery policy. Shared recovery needs bounded failed escapes and unknown
post-interruption pose handling.

First slice: a small sourced route with one doorway/edge transition and an
injected wild interruption, followed by the first mandatory trainer checkpoint.
Demonstrate native/offline agreement on candidate poses, safe anchor, facing,
route rejoin and battle entry. Add a supporter/traversal requirement before
committing to a full-run plan.

## 6. Sequence and acceptance gates

These are proposed independently reviewable slices, not a commitment to start
all games. Reuse the owning shared PRs rather than cloning shared files into a
game branch; merge parent updates down the chain.

| Slice | Deliverable | Acceptance gate |
|---|---|---|
| A | Shared preparation/reload semantics aligned across FRLG/BDSP | Read-only preparation caching, mutating preparation replay and save-generation invalidation tested |
| B | Portable checkpoint requirements/effects and plan export | One FRLG and one BDSP checkpoint replayed; invalid/unknown hard requirements fail before side effects |
| C | Resource/preparation replay | One capture/training/teaching/shopping dependency chain; no invented items, EXP, PP or successful preparation |
| D | SV runner wrapper | One legacy checkpoint with matching save/reset/action/notification ordering; no nested retry loops |
| E | SV requirements/effect export | Explicit team/location handoff, bounded recovery and honest partial replay coverage |
| F | R/S pilot | Pinned data, movement/transition/recovery parity and one battle-entry contract |
| G | LGPE feasibility pilot | Supported controller throw, capture outcome/resource/progression record, localization and one trainer entry |

Common native tests should be table-driven through game-action seams. Keep
controller/detector tests in Arduino; share versioned semantic scenarios and
expected states with PokemonSimulation. Compare exact map/pose/facing sets,
resource deltas, save restoration, stop reasons and action choices, not only
“completed” or candidate counts. Invalid preconditions must cause no movement,
purchase or save. Interrupted/queued movement cannot assume full displacement.
Repeat interruptions and retry exhaustion must terminate safely.

Start with the existing Forest/sign and BDSP Route 202 recovery evidence. Add
mine/house transitions only after map/arrival differences are reconciled. The
current native exports validate selected cases, not all queue/video/timing
behavior. Each new game needs its own small live acceptance gate after offline
regressions; passing a semantic simulator does not validate a detector or controller.

## 7. Inspected source revisions

Arduino paths below are relative to `SerialPrograms/Source/`. Links are pinned so
this assessment is reviewable even while the AutoStory chain changes.

| Scope | Revision and entry points |
|---|---|
| SV, existing LGPE/RSE | [`9d7341ba77a27db66fec7f7300d76113bbcc1549`](https://github.com/dayfine/Arduino-Source/tree/9d7341ba77a27db66fec7f7300d76113bbcc1549/SerialPrograms/Source/VideoGames): `PokemonSV/Programs/AutoStory/PokemonSV_AutoStoryTools.{h,cpp}`, `PokemonSV_AutoStory_Segment_10.cpp`, `PokemonSV/Programs/PokemonSV_WorldNavigation.h`, `PokemonLGPE/Programs/PokemonLGPE_GameEntry.h`, `PokemonRSE/PokemonRSE_Navigation.h` |
| Shared SV selection/stats candidate | [`cee0b139b102d6f09826f0b46c8654493f4a0a66`](https://github.com/dayfine/Arduino-Source/tree/cee0b139b102d6f09826f0b46c8654493f4a0a66/SerialPrograms/Source/Pokemon/AutoStory): `Pokemon_AutoStorySelect.h`, `Pokemon_AutoStoryStats.h`; SV tools include/alias these |
| Shared event/step owner | [`58aa81d04b1db29db52bd13e801e407f99e35513`](https://github.com/dayfine/Arduino-Source/tree/58aa81d04b1db29db52bd13e801e407f99e35513/SerialPrograms/Source/Pokemon/AutoStory): `Pokemon_StoryEvents`, `Pokemon_StorySteps`, `Pokemon_StorySegments` |
| Shared navigation owner | [`1b769a43ad7c593167c9cddbb676c48da6e199f6`](https://github.com/dayfine/Arduino-Source/tree/1b769a43ad7c593167c9cddbb676c48da6e199f6/SerialPrograms/Source/Pokemon/Navigation): `Pokemon_MapData`, `Pokemon_LegPlanner`, `Pokemon_LegWalker`, `Pokemon_WallRecovery` |
| FRLG candidate | [`7eb6517069c65748be88016f1e558fc3b67504b2`](https://github.com/dayfine/Arduino-Source/tree/7eb6517069c65748be88016f1e558fc3b67504b2/SerialPrograms/Source/VideoGames/PokemonFRLG): `Programs/AutoStory/PokemonFRLG_AutoStoryEvents`, `AutoStoryAbilities`, `AutoStoryMart`, `AutoStoryBattle`; `Overworld/` movement/planning; modified shared story files |
| BDSP candidate | [`8c6acefa34253d8d67a13d1fb51f2f1f2a7433eb`](https://github.com/dayfine/Arduino-Source/tree/8c6acefa34253d8d67a13d1fb51f2f1f2a7433eb/SerialPrograms/Source/PokemonBDSP/Programs/AutoStory): `StoryExecutor`, `StoryStepIO`, `Battle`, `BattleMoves`; `Data/StoryWorld`, `RoutePlanner`, `WallRecovery` (filenames prefixed `PokemonBDSP_AutoStory_`) |

Other references: [current offline integration plan](https://github.com/dayfine/PokemonSimulation/blob/b49fe94/docs/autostory-plan.md),
[FRLG native recovery review](https://github.com/dayfine/PokemonSimulation/blob/b49fe94/data/navigation/native-recovery/FRLG_FOREST.md),
and [BDSP native recovery review](https://github.com/dayfine/PokemonSimulation/blob/b49fe94/data/navigation/native-recovery/README.md).
These are architectural recommendations from source inspection, not new console
validation or an assertion that every candidate PR is green.
