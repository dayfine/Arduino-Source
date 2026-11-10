# FRLG story drafts, 00.30 to the Hall of Fame

**Every file here is an unverified draft.** None has run on a console, and the
story table doesn't load them: `pa_embed_text_files()` embeds only
`StoryData/*.json`, not this folder. The authored format has no status field,
so this README is where they are marked unverified.

Each file is a `pokesim-authored-checkpoint/1` plan in the same format as
`../frlg-parcel-00.05.plan.json`. The order and the item stops follow PokeGuy's
LeafGreen run (PokemonSimulation `data/runs/pokeguy-leafgreen-15934/events.json`
and `external/pokeguy-notes.md`). 00.28 and 00.29 are table rows on
`frlg/stage8-rocktunnel` (#293), not drafts: 00.29 ends at Lavender's nurse.
The draft that also used the id 00.29 (Lavender Mart, Route 8, the Underground
Path to Celadon's Center) was dropped in favour of #293's row, so nothing here
covers Lavender's nurse to Celadon's yet; its events are in commit 8b656d1a2.

| Id | Title | Events |
|---|---|---:|
| 00.30 | Celadon Game Corner and the Rocket Hideout down to B4F: the poster guard, the Lift Key Grunt, the LIFT KEY and B3F's Rare Candy | 12 |
| 00.31 | Rocket Hideout: the lift to B4F, the two door guards, two Rare Candies, Blastoise, Giovanni and the SILPH SCOPE | 16 |
| 00.32 | Celadon: Department Store 5F battle items, HM02 Fly on Route 16 for the Pidgey, the Tea, then the Center | 20 |
| 00.32b | Celadon Mansion roof room: the Eevee (Gift RNG placeholder) | 4 |
| 00.33 | Fly to Lavender, the Center, Pokemon Tower 2F: the rival | 8 |
| 00.34 | Pokemon Tower 3F to 7F: Tammy, Jennifer, Emilia, the Marowak, three Grunts, Mr. Fuji | 21 |
| 00.35 | The POKE FLUTE, Fly to Celadon, Route 16's Snorlax, Cycling Road's Rare Candy and Max Elixir, Fuchsia | 16 |
| 00.36 | Safari Zone: a Full Restore, the GOLD TEETH, HM03 Surf from the Secret House | 12 |
| 00.37 | Fuchsia: Surf to the lead, Juggler Kirk, Koga, then HM04 Strength from the Warden | 14 |
| 00.38 | Fly to Pallet, Surf down Route 21 to Cinnabar, the Mansion: Elixir, TM14 Blizzard and the SECRET KEY | 15 |
| 00.38b | Cinnabar Lab: the Helix Fossil revived (Gift RNG placeholder) | 6 |
| 00.39 | Cinnabar Gym: the quiz doors, Blaine | 9 |
| 00.40 | Fly to Celadon: two Rare Candies, Blizzard, Celadon Gym: Tamia, Lori, Erika | 21 |
| 00.41 | East to Saffron: the TEA at the gate, the Fighting Dojo's Hitmonchan (Gift RNG placeholder), the Center | 13 |
| 00.42 | Silph Co.: the CARD KEY, the rival on 7F, the Lapras (Gift RNG placeholder) | 10 |
| 00.43 | Silph Co. 11F: the boardroom Grunt, Giovanni; then Sabrina | 13 |
| 00.44 | Fly to Viridian: Viridian Gym: Takashi, Warren, Giovanni | 14 |
| 00.45 | Route 22: the rival; Route 23's badge gates; Strength | 7 |
| 00.46 | Victory Road's boulders, Indigo Plateau: Full Restores, the nurse, the save | 8 |
| 00.47 | Lorelei | 4 |
| 00.48 | Bruno | 3 |
| 00.49 | Agatha | 3 |
| 00.50 | Lance | 3 |
| 00.51 | The Champion and the Hall of Fame | 4 |

## Why 00.30 and 00.31 are not table rows yet

They can't run or be simulated on this branch's data:

- The map resource lock (PokemonSimulation `6c2f5c8`, `runtime-manifest.json`)
  has none of CeladonCity, CeladonCity_GameCorner or RocketHideout_B1F..B4F;
  they exist only in `source-candidate`. CeladonCity_PokemonCenter_1F (00.30's
  start) and RocketHideout_Elevator (00.31's lift) are in neither. Adding them
  to the runtime pack is a PokemonSimulation publish and a lock bump.
- 00.30's start, Celadon's nurse, isn't reached by any row: 00.29 ends in
  Lavender.
- The walker has no spinner (arrow) tiles, which B2F and B3F need, and no
  step for the lift's floor menu or the poster switch.
- `STORY_TRAINERS` names none of the hideout's Grunts or Giovanni yet;
  their parties are in `BattleData/`.

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
- New `game_action` variants that no executor has yet: `party_room`,
  `gift_rng`, `use_item`, `fly`, `flee_wild`, `surf`, `strength_puzzle`,
  `reenter_map`. The `mart` variants name Marts the Mart table doesn't list
  yet (Lavender, Celadon 5F, Indigo Plateau).
- HMs and key items come in story order: LIFT KEY, SILPH SCOPE, HM02 Fly, TEA,
  POKE FLUTE, HM03 Surf, GOLD TEETH, HM04 Strength, SECRET KEY, CARD KEY.

## Decisions for the user

- **A Pidgey for Fly.** PokeGuy flies everywhere from 1:03; the story party has
  no flier. The dropped Lavender-to-Celadon draft caught a Pidgey in Route 7's grass (Route 8's grass is behind
  two Cut trees). Without Fly, Cinnabar needs a long walk round to Pallet.
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
| 00.31/b2f_lift | B2F's lift corner is reached over arrow tiles the planner does not solve |
| 00.31/b4f_below_guards | RocketHideout_Elevator map not in source-candidate |
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
