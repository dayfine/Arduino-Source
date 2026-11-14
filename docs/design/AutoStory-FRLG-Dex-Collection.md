# FRLG AutoStory: goal-driven Pokédex collection

Status: **design only**. The actions, readers, catalog and program described
here are not implemented. This design is stacked on the current FRLG AutoStory
design and implementation through the Elite Four. Reviewed 2026-11-14 against
`frlg/stage20-league` (table, actions, capture) and `nav/shared` (failure
classes); decisions that belong to the user are marked **Open question** and
collected in section 12. This work starts after the FRLG story reaches the Hall
of Fame with verified checkpoints; it does not compete with that.

## 1. Goal and meaning of completion

Add a bounded part of FRLG AutoStory that can:

1. read which species the save has registered as caught;
2. distinguish obtainable, locked, unsupported and unavailable missing species;
3. choose a supported acquisition and a place to perform it;
4. prepare supplies and route there through the existing story actions;
5. run the existing capture machinery;
6. verify the registration, save it, and plan the next target; and
7. finish with an exact completion/blocker report rather than loop forever.

"Pokédex collection" means **caught registrations**, not keeping one specimen
of every species. A living-dex mode would need a party/PC ownership scan and
retention constraints in addition to the Pokédex state. It should be a later
mode, not an overloaded meaning of this one.

A FireRed save cannot register every species by itself. Version exclusives,
trade evolutions, mutually exclusive starters and fossils, and event-only
Pokémon remain explicit blockers unless the user enables an acquisition source
that supplies them. "Complete" always names the selected goal and allowed
methods.

The first goal is the **Kanto caught set** (Kanto Dex, 150 entries). The
National Dex is not a postgame given in FRLG: it needs 60 owned species plus
Celio's Ruby/Sapphire quest on the Sevii Islands, which is
[Architecture.md](AutoStory-Architecture.md) milestone 3 and out of this
design's scope. National entries in a goal are `story_locked` until then.

### 1.0 What the story already registers

The story rows own these acquisitions, several as shiny Gift RNG hand-offs the
user asked for: the Magikarp (04.03), the Helix Fossil (04.05; Miguel takes the
Dome), Oddish (05.09) and Pidgey (08.03) captures, Eevee (09.03/09.04), Lapras
(16.03b), and Hitmonchan (15.02/15.03, planned, not authored). The collector
never repeats them: a `gift`, `fossil` or `static` recipe whose story row has
run is `story_owned`, and the mutually exclusive counterparts (Dome Fossil and
Kabuto, Hitmonlee) are `unavailable` on a save that followed the story. Any new
gift or static that the user wants as a shiny stays a story row with its RNG
hand-off, not a collector recipe.

### 1.1 Existing pieces and exact gaps

| Concern | Reuse | Gap this design adds |
|---|---|---|
| Story execution | [`Pokemon_StoryCheckpoint`](../../SerialPrograms/Source/Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h), engine, event logs and retry policy | build one checkpoint at a time from a runtime plan |
| World travel | [`ConnectedMaps`](../../SerialPrograms/Source/Pokemon/Navigation/Pokemon_ConnectedMaps.h), FRLG planner/walker, Fly, Surf and runtime maps | choose a destination and an adjacent wild-tile hunt site from data |
| Wild identity | [`WildEncounterReader`](../../SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_WildEncounterReader.h) | constrain reads to the catalog's species at that site and feed the collection ledger |
| Capture | [`CapturePlan` and `run_capture()`](../../SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCapture.h) | selected balls, wired opponent HP, durable Pokédex verification and more encounter methods |
| New-registration screen | [`PokedexRegisteredDetector`](../../SerialPrograms/Source/VideoGames/PokemonFRLG/Inference/PokemonFRLG_PokedexRegisteredDetector.h) | read the existing Pokédex list before planning and after a catch |
| Supplies | Mart, bag row reader, heal and PC actions | calculate a ball budget and add supply steps to a generated checkpoint |
| Source data | pinned runtime maps from PokemonSimulation | a pinned acquisition catalog: encounters first, other recipe kinds later |

The current console capture adapter deliberately cannot weaken a target:
`read_foe_hp()` returns `UNKNOWN`, even though the opponent HP reader now
exists. It also finds only `poke-ball` at the top of the Balls pocket. The
first land-wild pilot can catch full-HP, high-catch-rate species, but a general
collector must close those two gaps instead of hiding them in generous bounds.

## 2. Shape: a controller that emits ordinary checkpoints

The collector is goal-driven rather than a static list of 150 story rows:

```text
read Pokédex
    -> missing = goal - caught
    -> classify acquisition recipes
    -> choose one feasible recipe and hunt site
    -> build one acquisition checkpoint
    -> run it through the existing story engine
    -> verify, return to an anchor and save
    -> update the snapshot and repeat
```

Do not implement this as one `collect_dex` GAME action. Such an action would
hide routes, shopping, captures, checks and saves inside one event, bypassing
the useful per-step logs, simulator and retry boundary.

Instead, `DexCollectionController` calls the read-only scan and pure planner,
then `DexCheckpointFactory` builds one normal `Pokemon::StoryCheckpoint`. Its
steps use the same action registry as authored FRLG rows. A typical first-slice
checkpoint is:

```text
from_save (known Pokémon Center anchor)
    -> mart, when the ball budget is not already present
    -> heal_to_full (center_enter / center_exit variants)
    -> field-move step, fly variant, or route steps (run / bike variants)
       to the selected map
    -> route to a generated land hunt site
    -> capture the selected species
    -> dex_entry requires caught
    -> fly / route back to a known Pokémon Center anchor
    -> heal_to_full
    -> save (the engine's step; the checkpoint's commit)
```

These are the table's real words. The FRLG GAME actions registered today are
`capture`, `mart`, `pc`, `teach`, `fossil`, `grind`, `grind_check`, `level`,
`bag_check`, `check_full_hp`, `heal_to_full` and `restore_pp`; movement is the
shared `route` step with `run`/`bike` variants
(`PokemonFRLG_AutoStoryPoses.h`), Fly and Surf are field-move step variants,
and saving is the engine's `save`. There are no `run`, `fly`, `surf` or `heal`
actions, and the factory must not invent them.

The dynamic checkpoint id should be stable and diagnostic, for example
`dex-025-pikachu-viridian-forest`. The run record then identifies the species,
recipe, destination, budgets and event that failed.

## 3. Two front ends, one collection routine

### 3.1 Standalone program first

Add `PokemonFRLG_DexCollector` under `Programs/DexCollection/` and register it
in the FRLG General panel. It owns only UI options and console setup, then calls
the reusable collector.

First-slice setup:

- FireRed, English;
- the game has reached the collection prerequisite selected by the program;
- a save exists at a supported Pokémon Center anchor selected in the UI;
- Fly and its named party user are available when the plan permits Fly; and
- the program loads that save before scanning.

The required anchor selection avoids pretending that a place name on the save
card proves an arbitrary tile. The first slice may support one anchor, such as
Celadon or Indigo Plateau, before exposing a database of checked Center anchors.

Suggested options:

| Option | First slice | Later |
|---|---|---|
| Goal | custom species list; Kanto caught set | named goal resources |
| Allowed sources | land wild encounters | Surf, fishing, Safari, static, gift, evolution, trade |
| Version | FireRed | LeafGreen |
| Start/commit anchor | one checked Center | supported anchor database |
| Route policy | safe/default checks | speed/cost policy |
| Capture bounds | encounters and balls per target | derived reliability target and global budget |
| On blocked species | report and continue | stop/report/skip policy |

The standalone program is the first console pilot because it can be rerun from
one postgame save without coupling development to a full story run.

### 3.2 AutoStory integration

The same `run_dex_collection()` routine can be called by AutoStory after the
main-story range has completed. Add an option such as **Continue with Pokédex
collection** only after the standalone controller is proven.

The story does not end at a Center. 20.05 ends in the Hall of Fame (5, 4), the
Hall of Fame saves the game itself, the credits roll, and the game next loads
in the player's bedroom in Pallet Town. The continuation therefore begins from
a declared post-credits start (bedroom, save card PALLET TOWN) and its first
generated checkpoint routes to a Center anchor (Viridian) and saves there
before any catch is attempted. The same start is the standalone program's
"after the Champion" prerequisite profile.

AutoStory supplies facts it already knows: version, completed story row,
available map flags, expected party roles, start menu, and the exact final
anchor. The collector still scans the Pokédex; it must not infer caught species
from the intended story route.

This is not initially another static start/end checkpoint range. It is a
goal-driven continuation after the selected linear range. If shared AutoStory
later gains story tracks with goal-driven tracks, this continuation can become
a `dex` track without changing the collector.

AutoStory must refuse the continuation when its selected end is not a declared
collection entry point. A run ending midway through the story can report what
is currently feasible, but it must not silently assume postgame capabilities.

### 3.3 Proposed code layout and integration points

Names are provisional, but ownership should stay separated as follows:

```text
Inference/Menus/
    PokemonFRLG_PokedexListDetector.{h,cpp}
    PokemonFRLG_PokedexListReader.{h,cpp}

Programs/DexCollection/
    PokemonFRLG_DexCatalog.{h,cpp}          resource model and parser
    PokemonFRLG_DexState.{h,cpp}            goal, snapshot, capabilities, report
    PokemonFRLG_DexReader.{h,cpp}           menu navigation and full/target reads
    PokemonFRLG_DexPlanner.{h,cpp}          pure classification and next plan
    PokemonFRLG_DexHuntSites.{h,cpp}        map-region hunt-site candidates
    PokemonFRLG_DexCheckpoint.{h,cpp}       ordinary StoryCheckpoint factory
    PokemonFRLG_DexController.{h,cpp}       scan -> plan -> checkpoint -> repeat
    PokemonFRLG_DexCollector.{h,cpp}        standalone descriptor/options/program

Programs/AutoStory/
    PokemonFRLG_AutoStoryDexEntry.{h,cpp}   read-only dex_entry GAME action
```

The action spec is registered with the other FRLG game-action specs and
dispatched by `StoryExecutor`. Its simulator adapter reads the simulated caught
set. Tests register through `PokemonFRLG_Tests.cpp`; the standalone descriptor
is added to `PokemonFRLG_Panels.cpp`; new sources/resources are added to CMake
in their owning slice.

`build_checkpoint()` can supply the action dispatch, start check and save, but
its save hook has two static-table lookups: `keep_next_start_frame()` finds the
next id's start to keep the post-save frame, and `checkpoint_end_map(id)`
(`PokemonFRLG_AutoStorySaveChecks.cpp`) finds the next row's start map for the
save-card place check. A generated row is in neither: `story_checkpoints()` is
loaded from the embedded `StoryData/frlg-<id>.plan.json` files and
`load_frlg_story()` rejects an id outside `STORY_ORDER`. So the factory passes
an explicit end map and a stable next-start key such as `dex-anchor-celadon`
(the next generated row checks that key even though its diagnostic id names a
different species), and builds its `Pokemon::Segment` with one `CheckpointSpec`
directly rather than through `make_story_segment()`. Static story rows keep
their current lookups unchanged.

A generated row also has no authored `StoryCheckpointMetadata.phase`, which is
what decides whether a run may save (TESTING never, VERIFICATION after 2-3
passes, REPRODUCTION always). The collector's commit is a save, so the program
carries the save mode itself (section 8) and stamps each generated row with it.

For AutoStory integration, add the continuation option to `AutoStory` only in
the integration slice. After `run_story_plan()` reports the declared main-story
end, construct `DexCollectionEnvironment` from the existing `StoryEnv`, final
pose and capabilities, then call the same controller as the standalone
program. The controller does not include or call the `AutoStory` program class,
so the dependency stays one way.

The standalone loader can reuse `StoryPreflight`, `soft_reset()`, save-card
place checks and the existing save routine, but needs its own small session
adapter: the current `story_game()` helper is private to `AutoStory.cpp`. Do not
copy that whole program shell. Extract only a reusable checked-save/session
helper if both callers need the same sequence.

## 4. State and contracts

### 4.1 Pokédex state

```cpp
enum class DexRegistration{
    UNSEEN,
    SEEN,
    CAUGHT,
    UNKNOWN,
};

struct DexSnapshot{
    std::map<std::string, DexRegistration> species;
    DexMode mode;                  // Kanto or National
    size_t caught_count;
};
```

`UNKNOWN` is not missing. Any goal entry that cannot be read is a scan failure
or an explicit unknown result; it must never cause a blind capture trip.

Scan the full goal at program start and after loading a save on restart. During
one uninterrupted run, a targeted check after each catch updates the in-memory
snapshot. A periodic full scan at an anchor catches drift without paying for a
150-row scan after every species.

When the goal contains national entries but the save exposes only the Kanto
Dex, classify those entries as locked on the National Dex prerequisite. Do not
turn absent rows into `UNSEEN` or `UNKNOWN` captures. National number to slug
comes from the existing `Pokemon/Pokedex/Pokedex-National.json` resource
(`load_pokemon_slug_json_list()`), not a new table.

### 4.2 Acquisition catalog

PokemonSimulation should generate and publish a pinned resource from
`pret/pokefirered`, rather than hand-authoring encounter facts in C++:

```json
{
  "format": "pokesim-frlg-acquisition-catalog/1",
  "source": {"repository": "pret/pokefirered", "revision": "..."},
  "versions": ["firered", "leafgreen"],
  "species": [
    {
      "slug": "pikachu",
      "national_number": 25,
      "catch_rate": 190,
      "recipes": [
        {
          "id": "wild-firered-viridian-forest-land",
          "kind": "wild",
          "version": "firered",
          "map": "ViridianForest",
          "method": "land",
          "encounter_rate": 10,
          "slots": [{"weight": 5, "min_level": 3, "max_level": 5}],
          "requires": []
        }
      ]
    }
  ]
}
```

Use the exact runtime-map names. Preserve raw slot weights and encounter rate;
the planner can aggregate multiple slots for the same species without losing
source information. Every recipe carries version and capability requirements.

Later recipe kinds use the same species index:

- `wild`: land, Surf, fishing, Rock Smash or Safari;
- `static`: an overworld interaction and battle;
- `gift` and `fossil`: a story interaction, possibly mutually exclusive;
- `evolve`: source species, method and level/item/trade requirement;
- `in_game_trade`: requested owned species and NPC interaction; and
- `external_trade` or `event`: disabled unless the user explicitly enables an
  external handoff.

The catalog says where a species can come from. It does not claim that the
current map pack can route there or that the console implements that method.

### 4.3 Runtime capabilities

```cpp
struct DexCapabilities{
    GameVersion version;
    std::set<std::string> story_flags;
    std::set<std::string> field_moves;
    std::set<std::string> key_items;
    std::set<std::string> supported_methods;
    std::set<std::string> loaded_maps;
};
```

An AutoStory continuation derives this from its known completed checkpoint and
verified inventory/party checks. A standalone run starts from a named
prerequisite profile, then verifies the facts it can read. Unknown capability
state blocks dependent recipes.

Do not use the caught Pokédex flag as proof that an evolution or trade source is
still owned. Supporting those recipes requires an owned-party/PC snapshot or a
session ledger rooted in a verified scan.

## 5. New inference and menu routines

### 5.1 Pokédex list inference

Add under `Inference/Menus/`:

1. `PokemonFRLG_PokedexListDetector`: recognizes the numerical Pokédex list
   and rejects the entry/detail page and other menu screens.
2. `PokemonFRLG_PokedexListReader`: reads the list mode, visible national
   numbers, cursor/scroll position, and each row's seen/caught marker.
3. `PokemonFRLG_PokedexEntryDetector`: recognizes the selected species entry
   when a targeted verification needs stronger evidence than the list marker.

The reader should identify species by national-number row, not OCR the English
name as its primary identity. Names come from `Pokedex-National.json`. It must
handle caught, seen-only, unseen and unread rows separately. The list facts
this relies on (the caught marker is a Poké Ball beside the number, unseen rows
show dashes, numerical order is the default and the SELECT search menu may not
be available before the National Dex) are from memory of the game, not yet
checked on console: slice 2 starts by capturing the real screens.

Add under `Programs/DexCollection/` or a reusable FRLG menu module:

- `open_pokedex_list()`: from the overworld through the existing START-menu
  cursor navigation, force numerical order and verify the list;
- `read_pokedex_snapshot(goal)`: traverse enough rows to cover the goal,
  waiting for each stable list screen, and return to the overworld;
- `read_pokedex_entry(species)`: navigate to one national number, verify its
  caught marker, and return to the overworld.

Each press waits for the screen it needs. There are no fixed-delay page walks.

Image tests need first/middle/last pages, list scrolling, Kanto/National mode,
caught/seen/unseen rows, an unread marker, and at least two capture-card
profiles. The full-scan control routine gets scripted-screen tests for a missed
press, a repeated page, a mode mismatch and a list that stops advancing.

### 5.2 Existing inference to reuse

- `SelectionArrowDetector` and `move_cursor_to_position()` already reach
  POKÉDEX from the full START menu.
- `PokedexRegisteredDetector` already recognizes the new-registration screen
  during a catch. The AutoStory capture loop does not currently consume it; add
  it when recording registration evidence. It is immediate evidence, but the
  target list row remains the durable postcondition.
- `WildEncounterReader` already returns the encountered species slug.
- `PokemonFRLG_BattleHpBarReader` already reads the opponent HP fill. The
  AutoStory capture adapter currently returns `FoeHp::UNKNOWN`; wire this reader
  into it instead of creating another HP detector.
- `ItemListReader` already reads bag rows. Extend ball selection through it
  instead of assuming the top row is `poke-ball`.

## 6. Planner and hunt-site selection

The pure planner takes a goal, snapshot, catalog, capabilities, current anchor,
maps and policy. It produces either one `DexAcquisitionPlan` or a completion
report.

```cpp
struct DexAcquisitionPlan{
    std::string species;
    std::string recipe_id;
    Pokemon::MapPlace hunt_site;
    HuntMethod method;
    CapturePlan capture;
    DexAnchor commit_anchor;
    DexPlanCost cost;
};
```

For each missing species, recipes are classified as:

| State | Meaning |
|---|---|
| `feasible_now` | version, capability, method and map are supported |
| `story_owned` | a story row acquires it (section 1.0); never planned here |
| `story_locked` | a badge, item, field move, area or scene is not available yet |
| `unsupported_method` | source exists, but its console action is not implemented |
| `unrouteable_map` | source exists, but the published map/connection graph cannot reach it |
| `unavailable` | wrong version, trade/event excluded, or mutually exclusive choice is gone |
| `unknown_state` | a required observed fact could not be read |

For a land recipe, enumerate adjacent pairs of tiles in the map's `wild`
region (`WILD_REGION` in `PokemonFRLG_OverworldWildTiles.h`). A valid
first-slice hunt site:

- lets the player step from one encounter tile to the other and back;
- is reachable through the loaded map graph;
- is not a warp, ledge, gate or one-way tile;
- avoids undefeated trainer sight when the story state says one remains (the
  capture loop already fights a trainer who sees the player while pacing,
  `PatchSteps::fight_trainer()`; after the Champion only optional trainers
  remain); and
- has a route back to the commit anchor.

The current `CapturePlan::pace` then comes from the chosen pair rather than an
authored row.

Choose deterministically by a cost such as:

```text
route to site + route back
    + expected encounters / aggregate slot weight
    + expected throws from catch rate, level, HP and selected ball
    + shopping/healing cost
    + policy risk penalties
```

The first slice can use conservative fixed encounter/ball bounds and greedy
one-target choice. Later the same contract can batch a location: choose a site
covering several missing species and catch any wanted species encountered there.
Do not start with a world-tour/TSP optimizer.

## 7. Story actions and capture changes

### 7.1 New read-only action

Add one small FRLG GAME action for generated checkpoints:

```text
dex_entry species=<slug> registration=caught
```

It calls the targeted Pokédex reader and returns `outcome_unchanged()` on the
required state. A missing, unread or wrong state is a distinct failed outcome.
This gives the checkpoint an explicit postcondition and lets simulation fake it.

The full `dex_scan` is a controller routine, not a story action: its result is a
large `DexSnapshot`, which does not belong in `Outcome` flags.

### 7.2 Keep existing actions where they fit

Generated checkpoints reuse:

- the `route` step (`run`/`bike` variants) and the connected-map route planner;
- the field-move step's `fly` and `surf` variants;
- `mart` for balls and healing items;
- `heal_to_full` with the Center variants, and the engine's `save` step;
- `pc` when a later recipe requires party arrangement; and
- `capture` for the first land-wild slice.

No separate `return_to_anchor`, `buy_balls` or `save_dex_progress` action is
needed. They are ordinary routes, Mart lines and checkpoint save policy.

### 7.3 Extend capture in reviewable pieces

The existing capture routine already provides encounter bounds, species reads,
non-target fleeing, shiny hard-stop, party/PC destination handling and catch
bounds. Extend it without changing today's authored capture defaults:

1. **Ball selection:** add optional `ball=<slug>` to `CapturePlan` and the
   `capture` action. The console searches the Balls pocket for that row and
   selects USE. Omitted keeps today's `poke-ball` behavior.
2. **HP-aware weakening:** map `read_opponent_hp_fill()` to `FoeHp` and enable
   the already-designed weakening turns. The plan still names a move and a
   maximum; safe-move selection from damage ranges is a separate planner.
   **Open question:** the PokeGuy team has no move that cannot knock out a
   low-level wild, so weakening means a TM (False Swipe) or a caught helper,
   which is a TM/team decision for the user. Recommendation: slices 5-7 use no
   weakening; they buy Great/Ultra Balls instead and let the planner's
   catch-rate filter decide which species are feasible without it.
3. **Registration evidence:** have the catch result record whether the
   registration page appeared. The generated `dex_entry` action remains the
   durable check.
4. **Separate encounter finding from a target battle:** extract the current
   paced-land finder from the battle/catch actions. This lets fishing, Surf and
   static methods reuse species reading, shiny policy, throws and catch exit.
5. **Location batches:** after the single-target slice, allow a set of wanted
   species. A non-primary missing species can be caught instead of fled from,
   then the controller replans from the updated snapshot.

Method-specific actions come later, each with its own explicit contract rather
than conditionally valid arguments on `capture`:

- `fish_capture`: selected rod, facing-water site, hook/battle detection;
- `surf_capture`: a water pacing pair and surfing movement;
- `safari_capture`: Safari steps/balls, no ordinary weakening, and a required
  exit before saving;
- `static_capture`: face/interact at a declared object, with no encounter loop;
- `gift_acquire`, `evolve` and `in_game_trade`: non-wild recipes whose effects
  are still checked by `dex_entry`.

## 8. Save, retry and restart contract

The first reliable version starts and ends every acquisition at a known anchor.
The capture itself is not committed until the checkpoint returns there,
verifies the target and saves.

- Failure before the save reloads the previous anchor save. If a target was
  caught and the return failed, the reset loses that catch and the next attempt
  catches it again; the in-memory snapshot must not claim it was committed.
- Success updates the snapshot only after the checkpoint save succeeds.
- A process crash after the save but before the update is recovered by the full
  Pokédex scan on restart.
- A shiny preserves today's rule: stop in the battle without pressing anything
  and do not reload.
- Safari checkpoints return outside the zone before saving, because its START
  menu has no SAVE.

Returning to an anchor costs time but fits the current save-place/start-frame
contract. Area batching and saves at generated hunt sites can come later only
with a persistent resume record and an independently verified tile pose.

Do not add a sidecar progress file in the first slice. The game save plus a
fresh Pokédex scan is the source of truth.

**Save mode.** The program has the same save modes as AutoStory: a testing run
never saves (it catches, checks `dex_entry`, then reloads, so the console pilot
proves the loop without committing), a verification run saves once after 2-3
passes of the same target, and a reproduction run saves at every anchor.
**Open question:** FRLG has one save slot, so every collector save overwrites
the post-credits save that the 20.x retries also load. Recommendation: run the
collector only once 20.01-20.05 are verified and the user has released that
save; until then testing mode only.

**Failure names.** Generated checkpoints report through the shared
`StoryFailureClass` list (`Pokemon_StoryFailureClasses.h`): a failed
`dex_entry` is `STATE_CHECK_FAILED`, a ball or encounter bound reached is
`OPERATION_FAILED` with the bound in the message, pacing interrupted by a wild
battle it could not flee is `WILD_BATTLES`. The planner's recipe states
(section 6) are not failure classes; they are a report, not a retry decision.

## 9. Completion and blocker report

The collector ends when the selected goal has no feasible recipe, not when the
missing set is necessarily empty. Report every remaining species once, grouped
by classification, for example:

```text
Kanto caught goal: 123 / 150 registered
  8 feasible, global collection budget exhausted
  6 LeafGreen/version-exclusive
  4 trade evolutions, external trades disabled
  3 supported by data but not by a console method
  6 locked by the selected story prerequisite
```

The on-screen status and notifications should show the current target, recipe,
map, encounter/ball budgets, completed count and blocker count. Stats should
separate wild encounters, targets seen, catches committed, balls thrown,
targets skipped and planner failures.

## 10. Tests and console pilot

### Catalog and parser

- Source pin and resource hash are present.
- Every species/recipe id is unique and every map name is canonical.
- Slot levels and weights are valid; aggregate weights never exceed the source
  table.
- Known FireRed/LeafGreen exclusives classify differently.
- Unknown recipe kinds or requirements fail at load.

### Pure planner

- Already-caught species are omitted.
- `UNKNOWN` entries are not targeted.
- Version, capability, supported-method and loaded-map filters produce the
  right blocker.
- The lower total-cost recipe wins with deterministic ties.
- Hunt sites are adjacent wild tiles, reachable both ways and outside a named
  active sight line.
- No feasible species produces a complete blocker report, not a loop.

### Controller and dynamic checkpoint

Use injected scan/planner/story-run actions:

- scan -> catch one -> save -> replan -> complete;
- wrong species are fled and do not change the snapshot;
- a catch whose `dex_entry` check fails is not committed;
- out of balls can select a supply checkpoint or report its bound;
- a failed return reloads and repeats the uncommitted target;
- restart after a successful save recovers from the scanned Dex;
- a shiny stops without a reload; and
- the generated checkpoint passes the normal story-action lint and simulation.

### Console pilot

Use a custom three-species goal from one checked postgame save:

1. one already caught species, which is skipped from the scan;
2. one supported land encounter, which is routed to, caught, verified and
   saved; and
3. one version-exclusive or unsupported species, which appears in the final
   blocker report.

Then restart the program from the saved anchor and prove it does not target the
committed species again. Expand to three land species across two maps before
adding another encounter method.

## 11. Implementation slices

Keep the design's boundaries as separate reviewable changes:

1. **Acquisition catalog in PokemonSimulation:** schema, pinned generator,
   validation and published Packages resource; no console code.
2. **Pokédex list inference:** detectors/readers and image tests; no presses.
3. **Pokédex navigation:** full and targeted reads through injected screen
   actions; no collection planner.
4. **Catalog reader and pure planner:** classification, hunt-site selection and
   deterministic costs with table tests.
5. **Capture ball/HP extensions:** defaults preserve existing authored rows.
6. **Dynamic checkpoint factory and controller:** fake story runner and
   simulation first.
7. **Standalone land-wild collector:** one anchor, custom goal, console pilot.
8. **AutoStory continuation:** call the proven controller from a declared final
   story anchor.
9. **One acquisition method per follow-up:** fishing, Surf, Safari, static,
   gifts/evolution/trades, each with its own detectors, actions and console
   evidence.
10. **Optimization after correctness:** location batches, opportunistic missing
    catches and additional anchors.

The first usable result is slice 7: it reads actual caught state, chooses among
published FireRed land encounters, routes, catches, verifies, saves and moves to
the next target. It makes no claim about methods added by later slices.

## 12. Open questions for the user

Each has a recommendation; none is decided here.

1. **Weakening move / TM spend** (7.3): teach False Swipe or catch a helper, or
   none. Recommendation: none in slices 5-7; better balls and a catch-rate
   filter instead.
2. **The one save slot** (8): may the collector overwrite the post-credits
   save? Recommendation: only after 20.01-20.05 are verified and released;
   testing mode (no save) until then.
3. **Goal and version**: Kanto caught set on FireRed only; National Dex and
   Sevii are out of scope. Recommendation: accept; LeafGreen and National Dex
   are later goals, not options in slice 7.
4. **Pilot anchor**: Viridian (first Center after the credits) or Celadon
   (department store, central Fly target). Recommendation: Viridian for the
   first checkpoint, Celadon as the working anchor once Fly is confirmed.
5. **External trades and events**: disabled by default. Recommendation: keep
   disabled; the blocker report names them.
