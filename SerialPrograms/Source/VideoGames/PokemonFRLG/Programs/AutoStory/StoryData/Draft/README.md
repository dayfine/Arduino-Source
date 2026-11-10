# FRLG story drafts, 00.30 to the Hall of Fame

**Every file here is an unverified draft.** None has run on a console, and the
story table doesn't load them: `pa_embed_text_files()` embeds only
`StoryData/*.json`, not this folder. The authored format has no status field,
so this README is where they are marked unverified.

Each file is a `pokesim-authored-checkpoint/1` plan in the same format as
`../frlg-parcel-00.05.plan.json`. The order and the item stops follow PokeGuy's
LeafGreen run (PokemonSimulation `data/runs/pokeguy-leafgreen-15934/events.json`
and `external/pokeguy-notes.md`). 00.28 and 00.29 are table rows on
`frlg/stage8-rocktunnel` (#293); 00.29 ends at Lavender's nurse, where 00.30
starts.

| Id | Title | Events |
|---|---|---:|
| 00.30 | Lavender Mart, then west over Route 8 on the Bicycle under a Repel: Gamer Rich, a Pidgey for Fly, the Underground Path to Celadon's Pokemon Center | 14 |
| 00.31 | Celadon Game Corner and the Rocket Hideout down to B4F: the poster guard, the poster switch, the Lift Key Grunt, the LIFT KEY and B3F's Rare Candy | 12 |
| 00.32 | Rocket Hideout: the lift to B4F, the two door guards, two Rare Candies, Giovanni, the SILPH SCOPE, an Escape Rope out | 16 |

The later drafts (Celadon Department Store to the Hall of Fame) are on
`frlg/draft-story-50-51` (#303) under the ids 00.32 to 00.51; with 00.30
restored here they each move up by one when that stack takes this one in.

## Story order after the hideout

As PokeGuy plays it: the Celadon Department Store 5F (6 X Speed, 21 X Special,
5 X Accuracy, 2 Guard Spec), then Pokemon Tower (a Super Repel on 3F to 7F,
the floors with wild Pokemon), Route 16's Snorlax, Fuchsia and Koga, the
Pokemon Mansion and Blaine, Erika, Silph Co., Sabrina, Viridian's Giovanni, and
the Elite Four.

Items from the bag, as PokeGuy uses them: a Repel over Route 8 and the
Underground Path (00.30; 5 bought in 00.26), renewed when "REPEL's effect wore
off" shows (`renew_repel()`); Super Repels in Pokemon Tower; an Escape Rope out
of the hideout after Giovanni (00.32), which puts the player at the Game
Corner's door, CeladonCity (34, 22) (pokefirered `UpdateEscapeWarp()`: the last
outdoor door, a tile south). The Rocket Hideout has no wild Pokemon
(`wild_encounters.json` lists none of its floors), so no Repel is used there.
The BICYCLE (00.26) is ridden on Route 8's open legs (`bike`); it is
`BIKE_TILE_TIME`, an estimate from the decompilation, until measured.

## The maps each draft needs

The story can't run or simulate these drafts until their maps are in the map
resource lock (PokemonSimulation `6c2f5c8`, `data/maps/runtime-manifest.json`).
Status there, in PokemonSimulation's `data/games/frlg/maps/source-candidate`,
or neither:

| Map | Draft | Status |
|---|---|---|
| LavenderTown | 00.30 | published |
| LavenderTown_Mart | 00.30 | candidate |
| Route8 | 00.30 | candidate |
| UndergroundPath_EastEntrance | 00.30 | candidate |
| UndergroundPath_EastWestTunnel | 00.30 | candidate |
| UndergroundPath_WestEntrance | 00.30 | candidate |
| Route7 | 00.30 | candidate |
| CeladonCity | 00.30 to 00.32 | candidate |
| CeladonCity_PokemonCenter_1F | 00.30, 00.31 | missing |
| CeladonCity_GameCorner | 00.31, 00.32 | candidate |
| RocketHideout_B1F | 00.31, 00.32 | candidate |
| RocketHideout_B2F | 00.31, 00.32 | candidate |
| RocketHideout_B3F | 00.31 | candidate |
| RocketHideout_B4F | 00.31, 00.32 | candidate |
| RocketHideout_Elevator | 00.32 | missing |

Beyond publishing (a PokemonSimulation publish and a lock bump, which need the
user's approval), the code added for these drafts reads three things the
candidate maps don't carry yet:

- B2F's and B3F's spinner tiles as regions `spin_north`, `spin_south`,
  `spin_west`, `spin_east` and `spin_stop` (pokefirered `MB_SPIN_*`,
  `MB_STOP_SPINNING`; `spin_slide()` in PokemonFRLG_OverworldMap.h);
- the Game Corner's poster as an object `poster` at (11, 1), and its stairs
  (16..17, 2..3) as objects `poster_stairs`, which the switch removes
  (`SimField::POSTER_SWITCH`);
- the lift's floors: its door is a dynamic warp (`setdynamicwarp`), so the sim
  is told where each floor lets out (`SimScript::lift_floors`: B1F (24, 25),
  B2F (28, 16), B4F (20, 23)) and the planner can't route through it yet.

`STORY_TRAINERS` names the battles these drafts fight: CeladonCity_GameCorner
`rocket_grunt` (GRUNT_7), and on RocketHideout_B4F `lift_key_grunt` (GRUNT_18),
`left_guard` (GRUNT_16), `right_guard` (GRUNT_17) and `giovanni`. When the
drafts become table rows, their battle steps take those names.

## Conventions

- A `battle` event's `id` is the battle's id in PokemonSimulation
  `docs/battles/pokeguy-trainers.json`, e.g. `route8-rich`, `koga`. Its strategy
  belongs to the battle agent, not to these files. Two battles have no entry
  there: `tower-marowak` (an `events.json` scripted fight) and
  `dojo-karate-master` (not in PokeGuy's run).
- Shopping is a `mart ...` `game_action` with the quantities from
  PokemonSimulation `docs/battles/pokeguy-conditions.md` bags and PokeGuy's
  receipts: Lavender (4 Escape Ropes, 7 Super Repels), Celadon 5F (6 X Speed,
  21 X Special, 5 X Accuracy, 2 Guard Spec), Indigo Plateau (12 Full Restores).
- A pickup faces the ball and talks (A). Hidden items (Cycling Road's Rare Candy
  and Max Elixir, the Mansion's Elixir) have no tile in the candidate maps.
- The story's shiny gifts are placeholders: a `party_room free=1` check, then a
  `gift_rng <name>` handoff, for the Eevee (00.32b), the Omanyte from the Helix
  Fossil (00.38b), the Hitmonchan (00.41) and the Lapras (00.42). The authored
  format can't yet say `handoff`/`live_start` as 00.12/00.12b do in C++.
- `game_action` variants the C++ executor now has (no console run yet):
  `poster_switch`, `lift_floor <from> <to>` (PokemonFRLG_AutoStoryHideout.h),
  `use_item <repel|super-repel|max-repel|escape-rope>` and `ride_bike`
  (PokemonFRLG_AutoStoryItemUse.h), and the `bike` route variant. Variants
  that no executor has yet: `party_room`, `gift_rng`, `use_item rare-candy
  slot=N` (an item used on a party member), `fly`, `flee_wild`, `surf`,
  `strength_puzzle`, `reenter_map`. The `mart` variants name Marts the Mart table doesn't list
  yet (Lavender, Celadon 5F, Indigo Plateau).
- HMs and key items come in story order: LIFT KEY, SILPH SCOPE, HM02 Fly, TEA,
  POKE FLUTE, HM03 Surf, GOLD TEETH, HM04 Strength, SECRET KEY, CARD KEY.

## Decisions for the user

- **A Pidgey for Fly.** PokeGuy flies everywhere from 1:03; the story party has
  no flier. 00.30 catches a Pidgey in Route 7's grass (Route 8's grass is
  behind two Cut trees). Without Fly, Cinnabar needs a long walk round to Pallet.
- **TM14 Blizzard over Mega Kick** (00.40) is a one-shot TM: it needs the team
  plan first.
- **Strength over Water Pulse** (00.45) and **Surf over Water Gun** (00.37)
  follow PokeGuy.
- **The Hitmonchan** needs the Dojo's Karate Master beaten, a fight PokeGuy
  skipped.

## Leg check

Every `route` event was planned on PokemonSimulation `6c2f5c8`
(`data/games/frlg/maps/source-candidate`, plus `runtime` for the maps the
candidate set lacks) with `RuntimeWorld` and the shared `shortest_path()`, from
the previous event's pose. 74 legs plan; 36 are skipped, none fails. The
planner ignores trainer sight, Surf, Strength, spinner and quiz tiles, and holds
NPC wander boxes as walls. No map was edited. Where a leg crosses a map edge on
an open tile (Lavender to Route 8, Route 7 to Celadon), the executor should hold
it to a wall, because of the known off-by-one on unheld crossings.

Two candidate maps look wrong; these are map questions for the user, not edits:
the Elite Four rooms' arrival points (e.g. Lorelei's `warp_0` (6, 12)) are
wall tiles, and Indigo Plateau's door glyph is at (11, 5) with its warp at
(11, 6).

| Skipped leg | Why |
|---|---|
| 00.32/b2f_lift | B2F's lift corner is reached over arrow tiles: the planner solves them now, but the candidate map has no `spin_*` regions |
| 00.32/b4f_below_guards | RocketHideout_Elevator map not in source-candidate |
| 00.32/x_item_clerk | CeladonCity_DepartmentStore_Elevator map not in source-candidate |
| 00.32/route16_below_tree | CeladonCity_DepartmentStore_Elevator map not in source-candidate |
| 00.32/celadon_nurse | map not in source-candidate |
| 00.32b/eevee_ball | Condominiums 2F/3F/RoofRoom map not in source-candidate |
| 00.35/cycling_road_rare_candy | Route17 map not in source-candidate |
| 00.35/cycling_road_max_elixir | Route17 map not in source-candidate |
| 00.35/fuchsia_below_center | Route17 map not in source-candidate |
| 00.35/fuchsia_nurse | map not in source-candidate |
| 00.37/fuchsia_nurse | map not in source-candidate |
| 00.38/mansion_b1f | the statue-switch gates and the floor drop are not modeled |
| 00.38/hidden_elixir_spot | hidden item tiles are not in the candidate maps |
| 00.38/below_tm14 | the statue-switch gates are not modeled |
| 00.38/below_secret_key | the statue-switch gates are not modeled |
| 00.38/cinnabar_nurse | map not in source-candidate |
| 00.38b/fossil_scientist | CinnabarIsland_PokemonLab_* map not in source-candidate |
| 00.39/below_blaine | the quiz doors are not modeled |
| 00.40/celadon_nurse | map not in source-candidate |
| 00.41/karate_master | SaffronCity_Dojo map not in source-candidate |
| 00.41/hitmonchan_ball | SaffronCity_Dojo map not in source-candidate |
| 00.41/saffron_center_door | SaffronCity_Dojo map not in source-candidate |
| 00.41/saffron_nurse | map not in source-candidate |
| 00.43/below_sabrina | warp-tile room order not planned: check SaffronCity_Gym warps |
| 00.44/takashi_line | arrow tiles and trainer lines not planned |
| 00.44/warren_line | arrow tiles and trainer lines not planned |
| 00.44/below_gym_giovanni | start unknown |
| 00.45/victory_road_door | Surf legs between the gates are not planned |
| 00.46/indigo_door | Victory Road boulder states are not modeled |
| 00.46/indigo_clerk | the Plateau door (11, 6) does not lead in on the candidate maps (no door glyph under the credits NPCs) |
| 00.47/below_lorelei | E4 arrival: the room's arrival point is a wall tile in the candidate map (map question, not edited) |
| 00.48/below_bruno | E4 arrival: the room's arrival point is a wall tile in the candidate map (map question, not edited) |
| 00.49/below_agatha | E4 arrival: the room's arrival point is a wall tile in the candidate map (map question, not edited) |
| 00.50/below_lance | E4 arrival: the room's arrival point is a wall tile in the candidate map (map question, not edited) |
| 00.51/below_champion | E4 arrival: the room's arrival point is a wall tile in the candidate map (map question, not edited) |

The native codec (`Pokemon::read_authored_checkpoint()`, built alone from the
story-data-tests sources) and PokemonSimulation's `read_authored_checkpoint()`
both read all 25 files.
