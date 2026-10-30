# FRLG demo work plan

Draft, 2026-10-28. Planning and source inspection only; no new gameplay code,
console run, or build validation. Start with English FireRed and one declared
starter; do not imply LeafGreen or other language coverage from shared code.

This plan refines [AutoStory.md](AutoStory.md) and the
[map-reuse review](AutoStory-Map-Reuse-Review.md). The demo slices below replace
the older fixed PR sequence; the broader pilot to Brock remains the goal.
Interfaces are proposals to validate with the two demos.

## Deliverables and order

Build two independently runnable slices, then chain them:

- **Startup:** new game → player name → bedroom → Oak's lab → starter →
  first rival resolved → verified post-rival save. Provide optional SID Helper
  and Starter RNG handoffs along this path.
- **Map demo:** verified post-rival save → Pallet → Route 1 → Viridian Mart →
  Oak's Parcel → return to Oak → verified Pokédex and save.

Begin map implementation from a prepared save while collecting startup screen
fixtures. The parcel trip exercises connected outdoor maps, interiors, grass,
NPC interaction, interruptions, and return travel. It is the experiment that
should determine a shared route executor. No Elite Four migration, generic
registry, full Kanto conversion, or broad SV extraction is a prerequisite.

## Startup compatible with SID Helper

Compatibility means explicit handoffs to the existing programs first. It does
not require embedding their UI programs or rewriting their RNG calculations.

| Stage | Entry and action | Verified exit / recovery boundary |
|---|---|---|
| Settings bootstrap | Check whether a settings-bearing save already exists; establish Fast text, Help button mode, frame 1, and the selected sound setting | Settings survive reload; a blank game needs a temporary save before the SID attempt |
| Prepare SID Helper | Start New Game, select the supported character, enter the requested player name | Naming screen with complete text, **before confirming OK**; drain controller requests and stop |
| External SID Helper | User runs existing program with matching language and valid target advances | Candidate SIDs and TID recorded; actual output may be trainer card or HOME |
| Resume after SID Helper | Resume the running game if on HOME, recognize trainer card, record/check TID, close it and recognize bedroom | Complete and verify the new identity's first save before permitting resets |
| Prepare Starter RNG | Complete Oak's introduction; move to the selected ball from the south side of the table | Save facing that ball, verify completion, and stop before interacting with it |
| External SID confirmation / starter hunt | User chooses a target using candidate SID, runs Starter RNG, and records confirmation | Candidate status remains distinct from confirmed SID; preserve the pre-starter save while testing candidates |
| Resume story | Explicitly choose to keep the result; identify actual screen, starter, rival state and health | Preserve the chosen starter in a verified save before reset-based recovery; finish the rival only if still pending |

The first demo may require manual settings bootstrap, SID target selection,
and confirmation recording. These are documented handoffs, not an unattended
end-to-end RNG feature. Ordinary startup bypasses both RNG pauses. Full blank-save
bootstrap can be implemented after the shorter naming/return path works.

The official [SID Helper instructions](https://pokemonautomation.github.io/Programs/PokemonFRLG/SidHelper.html)
require stopping before name confirmation and specify a default rival name.
They also require a temporary settings save when starting without a save.
Use Help button mode for compatibility with the broader RNG workflow, and keep
the selected Mono/Stereo setting consistent. Validate the documented capture,
console and profile setup at handoff. Never silently change these settings
between preparing and running an RNG program.

The [RNG guide](https://pokemonautomation.github.io/Programs/PokemonFRLG/RngManipulationGuide.html#finding-your-secret-id)
distinguishes candidate SIDs from confirmation using a shiny target. Store
TID, candidate SID/advance pairs, selected candidate, confirmation status,
game/language, sound setting and evidence reference together. TID is a useful
mismatch check, not a globally unique save identifier. A new-game restart
invalidates the association; never carry an old candidate list forward.
For the first manual handoff, a user-maintained record is sufficient; automatic
transfer is a later integration, not a promise that one program can read
another program's display.

The [Starter RNG instructions](https://pokemonautomation.github.io/Programs/PokemonFRLG/StarterRng.html)
require saving on the south side of the table facing the desired starter.
This becomes a named startup milestone, not just a pause near Oak's lab.

### Source constraints for integration

Inspected FRLG code at `c69b5d095`, under
`SerialPrograms/Source/VideoGames/PokemonFRLG/`:

- `Programs/RngManipulation/PokemonFRLG_SidHelper.cpp` has private helpers
  for timed name confirmation, intro completion, trainer-card navigation and
  TID reading. The timing sequence chooses the default rival name. Keep its
  input schedule unchanged; do not insert AutoStory observation waits into
  that interval. Observe before and after it.
- SID Helper does not save. It reads the trainer card and may go HOME. An
  AutoStory retry must not reset this unsaved new identity back to the
  temporary bootstrap save. A failed first save stops for recovery in place.
- Starter RNG can stop early on a shiny, or use rival battles and further
  leveling during calibration. Its exit is not universally "post-rival".
  Inspect screen/state before resuming and do not treat program completion
  as proof of a saved chosen starter. Resume from arbitrary calibration
  interruption is outside the first demo; report an unsupported state.
- Preserve the pre-starter save until the user commits to keeping the result.
  SID confirmation may use a disposable starter before a second target hunt.
- If direct integration is later worthwhile, extract a small callable routine
  with an explicit result from SID Helper. Keep the existing standalone
  program behavior and test its timing contract. Do not instantiate one
  top-level program from another or duplicate the timed script.

Handoff success must not trigger an automatic A press, game close, or reset.
The pre-OK boundary is a deliberate terminal result. Failure before a verified
save is different from failure at a replayable checkpoint.

## Map storage and typed model

**Use versioned JSON resources, loaded into typed C++ objects.** Keep large
map contents out of C++ string literals. Small synthetic grids remain useful
in native unit tests. Candidate resource path:
`RESOURCE_PATH()/PokemonFRLG/AutoStory/Maps/pallet-town.json`.
Integrate delivery with the project's Resources/Packages convention; do not
assume generated files placed in a local Packages directory are distributed.

Three separate kinds of state:

1. Immutable map definition: terrain, edges, exits, source objects and anchors.
2. Live navigation estimate: current map, possible tiles, facing, occupancy,
   most recent observation and uncertainty. A planned endpoint is not evidence.
3. Story state/checkpoints: observed milestones, available transitions, current
   starter/health policy and the last verified recoverable save.

A JSON schema is worth defining now, but only for the small demo subset. First
normalize two connected maps and one interior, then stabilize schema version 1
with the C++ loader. Proposed fields/types:

| Field | Representation / meaning |
|---|---|
| `schema_version`, `id`, `game` | Integer version; stable lowercase hyphenated map slug; explicit supported game |
| `source` | Repository, pinned revision, source map/layout IDs and input hashes |
| `width`, `height`, `terrain_rows`, `terrain_palette` | Rectangular grid of compact symbols with typed meanings; JSON strings per row are readable and diffable |
| `traversal_edges` | Sparse directed rules for ledges or other special movement; unknown rules reject traversal |
| `connections` | Outdoor edge, offset, destination map and corresponding boundary; local-to-world conversion is derived |
| `warps` | Local tile, source warp ID, destination map/warp ID and activation condition; explicit transition action |
| `objects` | Stable object ID, initial tile, fixed/wandering classification, movement/script/visibility references |
| `triggers` | Source script/flag references and supported story gates; not an executable game-script interpreter |
| `anchors` | Named location/region plus reference to FRLG observation evidence; a moving NPC is not a fixed anchor |

Coordinates are zero-based local layout tiles, x east and y south. Do not leak
engine border offsets into route definitions. Resolve destination warp IDs
explicitly; prove the arrival tile and facing on console, since the exit
animation can move the player away from the destination event tile.

Keep collision/elevation/metatile provenance available during conversion.
The runtime schema can collapse these to supported traversal only after the
conversion proves equivalence on the selected maps. A collision-free tile is
not sufficient to infer grass, ledges, warps or story availability.
Unsupported tiles/transitions are unavailable, not implicitly walkable.

Out-of-slice exits (e.g. Pallet to Route 21 or the rival's house) must be
explicitly excluded from routing. The importer can preserve their provenance;
the runtime loader must distinguish an intentionally unavailable edge from a
broken reference. Only enabled transitions require loaded destinations.

Keep static terrain separate from transient blockers. Object positions and
story flags from the decompilation describe possible behavior, not current
screen observations. Replanning changes live occupancy, not the packaged map.
Do not serialize BDSP's ASCII tile codes as the permanent cross-game schema.
Convert supported FRLG terrain to the existing shared TileGrid model; handle
warp actions above its path planner, which currently rejects door tiles.

## Route-planning memory and time budgets

The eight demo layouts contain 4,029 tiles; raw map storage is small. Search
state growth is the larger risk. The reviewed ordinary planner stores five
states per tile. The robust planner searches sets of possible positions and
stores those sets in its node table, parent records, planned legs and priority
queue. Its default `max_spread = 5` bounds each resulting position set, not the
number of sets explored. The current implementation has no total state,
queue-size or planning-time limit. A limit on replans does not bound one search.
These are source findings, not measurements of peak memory or a reproduced
out-of-memory failure.

Before the first observed walk, require:

- Explicit per-search limits on stored states, queued entries and elapsed time,
  plus cancellation checks within expansion work. Check capacity before growing
  containers, including stale queue entries and temporary successor sets.
  Bound input position sets and validate search parameters so one expansion
  cannot evade the resource limits. Never discard possible positions merely
  to fit a budget; obtain better observations or return a bounded failure.
- Distinct results for a route found, search exhausted without a route,
  budget exceeded (with the limiting resource), and cancellation. Budget
  exhaustion is not proof that no route exists. Re-localize, select a nearer
  verified landmark, or stop within the existing recovery budget; do not
  automatically retry the same search with larger limits.
- Per-search diagnostics: map/region dimensions, starting uncertainty,
  expanded states, peak stored states, peak queue entries, elapsed time and
  termination reason. During validation, measure peak memory and identify
  whether it is planner allocation or whole-process memory; video buffers and
  allocator retention must not be attributed to the planner without evidence.
- Short routes between observable anchors or exits. Use map connections/warps
  for travel between regions and tile search within a bounded region. MapSet
  builds a dense bounding rectangle, including gaps: validate dimensions and
  checked size arithmetic before allocation, and do not assemble all Kanto
  into one tile canvas. The JSON loader also limits resource size and cell count.

Choose and document initial finite limits before execution, then tune them from
measured demo searches. A byte estimate based only on node count is not a hard
memory cap unless vector sizes, queue growth and allocation overhead are also
bounded. Keep search-local data scoped to one search so replans release it.

Native acceptance tests must exercise tight state/queue budgets, deadline and
cancellation paths, oversized map/input rejection, and distinct no-route versus
budget-exceeded results. Use an injectable clock for deterministic deadline
checks. Include unreachable goals, open areas with ambiguous starts and repeated
replanning; verify cleanup and that budget failure issues no movement command.
Profile normal and adverse searches on the demo maps, record peak memory/time,
and confirm repeated replans do not accumulate retained search data. If the
parcel route exceeds the documented limits, narrow the search or improve
localization before expanding scope. This gate precedes sharing the executor.

## Map sources and first importer

The initial source subset consists of eight maps and their dependencies from
[pret/pokefirered](https://github.com/pret/pokefirered/tree/ce6406d8c2eaf46d543c8a2606de0c3cb1951982),
pinned to `ce6406d8c2eaf46d543c8a2606de0c3cb1951982`:

| Map | Width × height | Purpose |
|---|---|---|
| PalletTown | 24 × 20 | Outdoor hub and Oak trigger |
| PalletTown_PlayersHouse_1F | 13 × 10 | Startup interior |
| PalletTown_PlayersHouse_2F | 12 × 9 | Initial bedroom / SID resume |
| PalletTown_ProfessorOaksLab | 13 × 14 | Starter, rival and parcel delivery |
| Route1 | 24 × 40 | Outdoor connection, grass and ledges |
| ViridianCity | 48 × 40 | Town connection and Mart approach |
| ViridianCity_Mart | 11 × 9 | Parcel interaction and door transition |
| ViridianCity_PokemonCenter_1F | 15 × 10 | Optional healing fallback |

The source subset includes map JSON/scripts, layout binaries, relevant
metatile attributes, layout and tileset indexes, fieldmap definitions, naming
code and constants. The map table describes decompilation inputs, not a
production map pack or console-validated navigation dataset. The converter
must record source revision and input hashes with its output.

`map.json` alone is insufficient: it contains events/connections and a layout
reference, while tile data is in `map.bin`. The pinned fieldmap definitions
encode metatile index in bits 0–9, collision in 10–11, elevation in 12–15.
Resolve primary/secondary tileset attributes to identify relevant behavior.
Do not download rendered screenshots and treat them as authoritative collision.

First importer deliverable: deterministic offline conversion of Pallet,
Route 1 and Oak's lab, plus a readable grid/edge report. Add the remaining demo
maps once the encoding and warp semantics are checked. Preserve source revision
and hashes, validate dimensions and references, and compare the converted
route against console observations. No runtime network dependency or moving
"latest" upstream input. An offline conversion script may be Python; production
loader/planner tests remain registered native C++ tests.

## Startup and traversal inventory

| Capability | Existing pieces | Demo work still required |
|---|---|---|
| New Game / intro | Dialog, selection and fade detectors | Fresh-save/title path, character/name transitions, cutscene locks; existing LoadMenuDetector assumes a save and says new-game behavior is untested |
| Name entry | No FRLG name-entry driver found | FRLG in-game keyboard, not Switch system keyboard; observe page/cursor, type/backspace, validate text, separate entry from confirmation |
| Naming scope | Decomp defines 7-character player/rival limit and upper/lower/symbol pages | Initially support declared ASCII letters, reject unsupported input before pressing anything; include both keyboard pages in fixtures; custom rival only outside SID mode |
| Menu context | Start-menu helpers and NO_DEX support | Distinguish no Pokémon, Pokémon/no Dex, and post-Dex layouts; do not hard-code one row index throughout startup |
| Saving / reload | `save_game_to_overworld`, reset/load helpers | Verify first-save and overwrite prompts, actual save completion and observed reload location; current save helper can assume first-save completion after a missing second prompt |
| Battle decisions | `spam_first_move`, BattleResult, battle/PP/faint detectors | Bounded early-rival policy, explicit win/loss results, health and PP handling, return to observed overworld; story progress must not assume victory |
| Rival reference | Starter RNG `auto_battle_rival` | Coupled to stat inference, filters and EV updates; use as source evidence, not a drop-in story battle driver |
| Wild battles | `flee_battle`, tested `WildBattleExitActions` | Validate failed escape/opponent turn/faint at starter levels; existing flee contract assumes escape (Smoke Ball), though implementation retries; battle-exit logic only handles an already-won battle |
| Movement | Shared BDSP TileGrid/MapSet; FRLG timed walkers | FRLG observations, uncertainty, small movement batches, facing, dynamic NPCs, interrupted movement and verified arrival |
| Story interactions | Dialog and menu primitives | Oak escort/locks, ball selection, rival trigger, Mart parcel receipt, Oak delivery and actual Dex proof |
| Recovery / diagnostics | ScreenMachine, StepTrace, SV retry draft | Exact attempt budget, verified saved state, no reset before preservation, timeout screenshots, trace of observed vs predicted position |

Before ordinary wild-encounter travel, establish a health policy. Start simple:
verify usable starter and PP, flee where safe, and stop/recover on unsupported
battle states. Add healing only when the observed route needs it. Do not allow
a loss to silently invalidate the next checkpoint's assumed party state.

## Implementation slices and acceptance evidence

| Slice | Deliverable | Acceptance gate |
|---|---|---|
| 0. Fixtures and contracts | Recorded fresh intro, keyboard, trainer card, save prompts, starter-facing save and post-rival state | Named start/end states, supported settings, recovery rules and screenshots; no inferred save success |
| 1. Maps and loader | Small deterministic normalized map pack; typed JSON loader | Native tests reject malformed dimensions, unsupported schema/terrain, broken enabled transitions; test outdoor offsets, directional ledges and warp resolution; enforce resource/cell limits and checked allocation sizes |
| 2. One observed walk | Bounded planner, prepared save, Pallet/Route 1 movement and one interior transition | Pass search-budget/cancellation tests and record memory/time measurements; verify destination/facing; forced missed movement and NPC obstruction lead to re-localization or bounded stop, not an invented exact coordinate |
| 3. Parcel trip | Full route from post-rival save to saved Dex | Wild interruption resumes from observed state; parcel/Dex evidence; deliberate failure reloads verified checkpoint and completes replay; repeated replans remain within documented resource limits |
| 4. Naming / SID handoff | Enter player name and stop before OK; resume after existing helper | No extra queued input; default rival retained; TID/candidate record associated correctly; first new-identity save verified before any reset |
| 5. Starter handoff / startup | Verified ball-facing save; ordinary starter/rival path; supported resume after chosen RNG result | No accidental starter acceptance at handoff; no reset of unsaved chosen result; actual rival outcome and save verified |
| 6. Chain and extract | Startup → parcel demo; compare FRLG and BDSP execution | Repeat unattended ordinary run and supported manual RNG handoff run; extract only demonstrated common policy with both adapters covered |

For ordinary startup and the parcel demo, require three consecutive unattended
completions from the declared start, plus the deliberate failure/recovery run.
For RNG compatibility, validate each automated leg independently and record
manual handoffs separately; do not call the combined workflow unattended.

Slices 1–3 can precede 4–5 using prepared saves. Slice 0 starts immediately for
both. Validation records must distinguish manual stepping, unattended completion,
repeat runs and synthetic tests; record console time rather than promise a
hardware budget before measurements exist.

## Reuse decisions to defer until the demo

- Reuse BDSP's already-shared `CommonTools/ScreenMachine` and
  `Pokemon/Navigation/Pokemon_TileGrid` / `Pokemon_MapSet` via explicit
  prerequisite commits. MapSet is for adjacent outdoor maps, not interiors
  superimposed on a world canvas. Move pure planner tests out of BDSP-specific
  registration when adding the second consumer.
- Keep FRLG input timings, screenshots, battle/dialog interpretation, save
  verification and story milestones local. BDSP RouteWalk is useful evidence,
  but combines all of these and should not simply move wholesale to Common.
- SV #43–46 provide small selection/stats/retry candidates; use only what the
  demo immediately consumes. Its legacy retry default executes 102 attempts
  and does not guarantee the new-save recovery contract. Do not silently
  change SV behavior while adopting a stricter FRLG policy. #47's SV developer
  harness is not a prerequisite.
- SwSh's seven-segment prototype does not supply a proven checkpoint/retry
  implementation. Its timed movement and save assumptions are not the FRLG
  acceptance standard.
- After both games run, compare a concrete navigation result type (arrived,
  interrupted, uncertain, failed), the replan/attempt policy, and checkpoint
  orchestration. Share these only where both adapters agree on semantics.

## Immediate next implementation task

Collect startup/handoff fixtures and implement the three-map conversion plus
loader as the first narrow code change. Also verify the prepared post-rival save and the
first observed Pallet/Route 1 walk. Naming is the first startup-specific driver;
it should expose "enter text" separately from "confirm" from day one.
