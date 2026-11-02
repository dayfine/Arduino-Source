# FRLG AutoStory: the full checkpoint list

Status: **design data only**. Rows after `00.11` are not registered in
`story_checkpoints()` and are not executable or console-validated.

This is the checkpoint list for FireRed/LeafGreen AutoStory from a new game to
the end of the Sevii Islands story, plus the optional extras. It is built so
that every line of the 52poke story flow lands somewhere:

- **52poke** [附录:宝可梦 火红／叶绿流程](https://wiki.52poke.com/wiki/%E9%99%84%E5%BD%95:%E5%AF%B6%E5%8F%AF%E5%A4%A2_%E7%81%AB%E7%B4%85%EF%BC%8F%E8%91%89%E7%B6%A0%E6%B5%81%E7%A8%8B)
  (revision of 2026-10-07) is the line list. Each line has an id such as
  `开局12` (section + line number, nested lines counted, place-header lines in
  终局 counted too). Lines in *italics* there are optional story; here they are
  **named options, off by default**. The coverage table at the end maps every
  line to a checkpoint.
- **Segments** are the 20 parts of the page's English version,
  [Bulbapedia's walkthrough](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen).
  Segment 00 keeps today's numbering and covers Parts 1–3 plus the Running
  Shoes; Parts 4–20 become segments 01–17.
- **Exact triggers, tiles and prompts** come from
  [pret/pokefirered](https://github.com/pret/pokefirered) `data/maps/<Map>/map.json`
  and `scripts.inc`. Where a row cites a script or tile it was read there;
  every other start or end is a symbolic anchor (`@Map.anchor`), not a
  coordinate. Resolve anchors against the decomp, then on the console.
- **What a fast route skips** comes from pokeguy's LeafGreen 1:59:34 notes
  (`autostory/FRLG_WR_VIDEO_NOTES.md` on `notes/frlg-wr-route`), quoted as
  "WR" below.

## Reading the rows

Each checkpoint starts from the previous checkpoint's end save, unless its
Start says otherwise, and ends by saving at its End. A row's postcondition (the
badge, item or gate it exists for) must be observed before that save; a
visible destination or a closed dialog is not enough.

Columns: **ID**, **Title**, **Start** (pose or anchor), **Does** (ordered
steps), **End / save**, **52poke** (lines covered; italic lines in *italics*).
`[opt: name]` marks a step or row that runs only when that option is on.

Current table helpers are `route`, `legs`, `step_into`, `talk`, `answer`,
`scene`, `battle`, `ready`, `heal` and `make`. Their limits still hold:

- `route` and `legs` need mapped geometry; an `@anchor` is not a map point.
- `battle` is the MVP battle; each named fight needs its own readiness and
  move/party policy. All progression fights must be won; only the tutorial
  fight has `may_lose=true`.
- `ready` covers HP/status, not PP, level, matchup, roster or inventory.
  `SHOP` is the fixed Viridian purchase, not a general restock.
- `scene` is the MASH step. Item pickup, scripted travel, puzzles, gifts,
  field moves and special battle modes must not be blind mashing.

Capitalized steps are **new requirements**, not existing `StoryStepKind`s:
`VERIFY`, `ITEM`, `TEACH`, `FIELD`, `PARTY`, `PREP`, `PUZZLE`, `TRAVEL`,
`CAPTURE`, `CHOICE` and `STOP_AT`. `VERIFY` names something observable on
screen; labels like `ticket_received` are planning names, not RAM reads.

The inventory at future starts is unknown until measured; `{2, 1}` from Brock
must not be copied forward. Split long route and dungeon rows into named route
and battle events once their maps are surveyed; pick a path and inspect sight
lines before naming the trainers a row must fight.

## Run modes, sections and options

Run modes (planned, `autostory/DEMO_PLAN.md` on the notes branch): SPEEDRUN,
DEFAULT and SAFE choose which battles to take, whether to flee wild Pokemon,
the heal threshold and what to buy. Rows describe DEFAULT. SPEEDRUN would
mostly turn on travel options the WR uses (`bike_voucher`, `fly`,
`south_road=WEST`) and could drop non-italic steps the WR skips, such as
Silph's Master Ball.

**Sections** follow the story-sections rule (`docs/design/AutoStory-StoryTracks.md`
on `docs/story-tracks`): a section ends where the user must step in.

| Section | Segments | Ends at | Why the break |
|---|---|---|---|
| Main story | 00–14 | `14.07`, first Hall of Fame | The National Pokedex (`15.01`) needs 60 caught species; collecting them is the user's job (or a separate program) |
| Postgame | (11 if deferred), 15–16, optional 17 | `16.03`, network machine complete | — |

Postgame rows assume the first Hall of Fame. `15.01` checks its precondition
(post-HoF save, 60 caught) at the start and stops with a clear message before
saving anything.

### Options

All default off. Each one names what it adds. Choices with two values have a
default value instead.

| Option | Adds | Where | 52poke |
|---|---|---|---|
| `player_name`, `sid_helper_handoff`, `starter_rng_handoff` | Reserved for the segment-00 work in progress (player-name setting; stop at the naming screen before OK for SID Helper and resume after it; split `00.04` to save under the ball for Starter RNG). Not designed here. | 00 | — |
| `town_map` | Daisy's Town Map in the rival's house | 00.06 | *开局10* |
| `route22_rival_early` | Optional first Route 22 rival battle | 00.07 | *开局12* |
| `magikarp` | Buy the Magikarp at the Route 4 Center | 01.01 | *开局17* |
| `vs_seeker` | Vs. Seeker from the girl in Vermilion Center | 03.02 | *中局4* |
| `bike_voucher` | Fan Club voucher; exchange for the Bicycle in Cerulean | 03.02, 04.02 | *中局5* |
| `old_rod` | Old Rod from the Fishing Guru's house | 03.02 | *中局6* |
| `flash` | Diglett's Cave → Route 2 aide → HM05 Flash (10 caught) | 04.01, 05.01 | *中局12* |
| `old_amber` | Pewter Museum side door (Cut) → Old Amber | 04.01 | not on 52poke |
| `early_tower` | Pokemon Tower before Celadon: 2F rival, ghosts on 3F | 05.02 | *中局15*, *中局16* |
| `coin_case` | Coin Case from the man in the Celadon restaurant | 06.01 | *中局19* |
| `eevee` | Eevee from the Celadon Mansion rooftop room | 06.01 | *中局23* |
| `fly` | HM02 Fly from the Route 16 house; enables Fly travel | 07.01 | *中局37* |
| `south_road` = **EAST** \| WEST | Which Snorlax road reaches Fuchsia. One is required. | 07.02–07.04 | *中局31*, *中局36* |
| `super_rod` | Super Rod from the Route 12 fishing brothers | 07.03 | *中局32* |
| `itemfinder` | Itemfinder from the Route 11 gate aide (30 caught) | 07.03 | *中局33* |
| `exp_share` | Exp. Share from the Route 15 gate aide (50 caught) | 07.03 | *中局35* |
| `amulet_coin` | Amulet Coin from the Route 16 gate aide (40 caught) | 07.04 | *中局38* |
| `good_rod` | Good Rod from the Fishing Guru's brother in Fuchsia | 07.07 | *中局43* |
| `dojo_gift` | Fighting Dojo: Karate King, then Hitmonlee or Hitmonchan | 08.02 | *终局7* |
| `lapras_gift` | Lapras from the Silph 7F employee after the rival | 08.04 | *终局9* (italic half) |
| `cinnabar_approach` = **ROUTE21** \| SEAFOAM | Reach Cinnabar by Pallet + Route 21, or by Routes 19/20 and Seafoam | 09.01 / 09.02 | *中局44* |
| `articuno` | Stop and save before Articuno; capture only if asked (needs SEAFOAM) | 09.02 | *终局4*, *终局5* |
| `fossil_revival` | Revive the Mt. Moon fossil (and Old Amber) at the Cinnabar Lab | 10.01 | *终局13* |
| `sevii_before_league` | Accept Bill on Cinnabar and play segment 11 before Giovanni. Off: decline; segment 11 runs at the start of the postgame. | 10.03 | *终局16* |
| `zapdos` | Power Plant: stop and save before Zapdos; capture only if asked (needs `fly`) | 12.02 | *终局1*–*终局3* |
| `rock_smash` | HM06 from the old man at Ember Spa | 15.03 | *七之岛5* |
| `moltres` | Mt. Ember summit: stop and save before Moltres | 15.03 | *七之岛6* |
| `four_island_daycare` | Leave Pokemon at the Four Island Day Care | 15.05 | *七之岛10* |
| `water_labyrinth_egg` | Egg from the old man at Water Labyrinth | 16.01 | *七之岛16* |
| `selphy` | Lost Cave: rescue Selphy; show her the asked-for Pokemon | 16.01 | *七之岛17* |
| `tanoby_unown` | Seven Island: Sevault Canyon, Tanoby Key, unlock the Unown chambers | 17.04 | *剧情后1* |
| `mewtwo` | Cerulean Cave: stop and save before Mewtwo | 17.01 | *剧情后2* |
| `roaming_beast` | Bounded search for the starter-dependent beast (Squirtle: Raikou) | 17.03 | *剧情后3* |
| `league_rematch` | Elite Four round 2 | 17.02 | not on 52poke |
| `navel_rock`, `birth_island` | **Unsupported**: need event tickets (MysticTicket, AuroraTicket) | — | *剧情后4*, *剧情后5* |
| `diploma` | Diploma from Game Freak, Celadon Mansion 3F; needs a completed Pokedex. **Unsupported** until a dex-completion program exists. | — | *剧情后6* |

Italic lines that sit on the default path anyway: one of *中局31*/*中局36* (a
Snorlax must be passed), one half of *中局44* (some route to Cinnabar), and
*终局34* (Route 21 north; italic because Fly replaces it).

## Main story

### Segment 00 — New game to the Running Shoes (Parts 1–3)

Implemented in #120 (`frlg/autostory-mvp`,
`PokemonFRLG_AutoStoryTable.cpp`); poses below are that table's. The reserved
rows from the segment-00 work in progress (SID Helper resume point, Starter
RNG split of `00.04`) get their ids in that PR.

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 00.01 | New game to the bedroom | title screen | Intro, player and rival names `[opt: player_name, sid_helper_handoff]` | bedroom (6,6) N | 开局1 |
| 00.02 | Bedroom to Pallet Town | BEDROOM (6,6) N | Downstairs, out of the house | PALLET (6,8) S | 开局2 |
| 00.03 | Oak stops the player | PALLET (6,8) S | North-exit trigger; Oak escorts to the lab | LAB (6,4) N | 开局2 |
| 00.04 | Squirtle, then the rival's battle | LAB (6,4) N | Pick the starter; rival picks the counter; tutorial battle (`may_lose`) `[opt: starter_rng_handoff]` | LAB (7,8) S | 开局3, 开局4, 开局5 |
| 00.05 | Route 1 to Viridian Mart | LAB (7,8) S | Route 1 north; Mart clerk gives Oak's Parcel | VIRIDIAN_MART (4,3) W | 开局6, 开局7 |
| 00.06 | Parcel to the Pokedex | VIRIDIAN_MART (4,3) W | Back to Pallet; deliver Parcel; Pokedex and balls `[opt: town_map — rival's house, Daisy]` | LAB (6,4) S | 开局8, 开局9, *开局10* |
| 00.07 | Viridian shopping, into the forest | LAB (6,4) S | Route 1 north; shop; old man; Route 2 west into Viridian Forest `[opt: route22_rival_early — Route 22 battle before Route 2]` | FOREST (29,62) N | 开局11, *开局12*, 开局13 |
| 00.08 | Forest to Pewter Center | FOREST (29,62) N | Forest trainers in path; Pewter Center heal | PEWTER_CENTER (7,4) N | 开局13 |
| 00.09 | Camper Liam, then Brock | PEWTER_CENTER (7,4) N | Gym; Liam; Brock → Boulder Badge | GYM (6,6) N | 开局14 |
| 00.10 | The Running Shoes | GYM (6,6) N | Route 3 entrance trigger; Oak's aide gives Running Shoes | PEWTER (46,21) N | 开局15 |
| 00.11 | A run to a wall | PEWTER (46,21) N | Timing run for running calibration (lab row) | — | — |

### Segment 01 — Route 3, Mt. Moon, Route 4 (Part 4)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 01.01 | Route 3 to the Mt. Moon Center | PEWTER (46,21) N | `ready`; Route 3 trainers on the chosen path; Route 4 Center `heal` `[opt: magikarp]` | `@Route4.center_counter` | 开局16, *开局17* |
| 01.02 | Mt. Moon to the fossils | `@Route4.center_counter` | `route` 1F→B1F→B2F; Rockets on the path; `PREP(Miguel)`; `battle(super_nerd_miguel)`; `CHOICE(fossil)` (Dome \| Helix, setting); `ITEM` | `@MtMoonB2F.fossil_room` | 开局18 |
| 01.03 | Exit Mt. Moon to Cerulean | `@MtMoonB2F.fossil_room` | Exit east; Route 4 one-way ledges; Cerulean Center `heal`. Never plan a walk back uphill. | `@Cerulean.center_counter` | 开局19 |

### Segment 02 — Cerulean, Routes 24/25, Route 5, Underground Path (Part 5)

Order follows 52poke (Misty first). Swapping Misty after Bill for experience is
a schedule change, not an option.

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 02.01 | Cascade Badge | `@Cerulean.center_counter` | Gym trainers on the path; `PREP(Misty)`; `battle(Misty)`; Cascade Badge, TM03; back to the Center | `@Cerulean.center_counter` | 开局20 |
| 02.02 | Rival at the bridge | `@Cerulean.center_counter` | North to the Route 24 trigger; `battle(rival_cerulean)`; Fame Checker | `@Route24.bridge_south` | 开局21 |
| 02.03 | Nugget Bridge | `@Route24.bridge_south` | Five bridge trainers, each named; Nugget; `battle(rocket_recruiter)` | `@Route24.bridge_north` | 开局22 |
| 02.04 | Route 25 to Sea Cottage | `@Route24.bridge_north` | Route 24 grass and Route 25 trainers on the chosen path; enter the cottage | `@SeaCottage.door` | 开局23 |
| 02.05 | Restore Bill | `@SeaCottage.door` | `talk(Bill)`; `answer(help, YES)`; PC; separation `scene`; `talk(Bill)`; `VERIFY(SS_Ticket)` | `@SeaCottage.door` | 开局24 |
| 02.06 | Back to Cerulean | `@SeaCottage.door` | Routes 25 and 24 south; Center `heal` | `@Cerulean.center_counter` | 开局25 |
| 02.07 | The burgled house | `@Cerulean.center_counter` | Officer has moved (after the ticket); through the house to the back yard; `battle(rocket_thief)`; TM28 | `@Cerulean.house_backyard` | 中局1 |
| 02.08 | Route 5 and the Underground Path | `@Cerulean.house_backyard` | Route 5 south (the Saffron gate guard turns the player back; the route does not touch it); Underground Path to Route 6 | `@Route6.underground_exit` | 中局2, 中局3 |

### Segment 03 — Route 6, Vermilion, S.S. Anne, Vermilion Gym (Part 6)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 03.01 | Route 6 to Vermilion | `@Route6.underground_exit` | Route 6 trainers on the path; Vermilion Center `heal` | `@Vermilion.center_counter` | 中局3 |
| 03.02 | To the harbor | `@Vermilion.center_counter` | `[opt: vs_seeker]` girl in the Center; `[opt: bike_voucher]` Fan Club chairman; `[opt: old_rod]` Fishing Guru; harbor ticket check; board | `@SSAnne.1F_entry` | *中局4*, *中局5*, *中局6*, 中局7, 中局8 |
| 03.03 | Ship rival | `@SSAnne.1F_entry` | Route to the captain's deck; `PREP(rival_ship)`; `battle(rival_ss_anne)` | `@SSAnne.captain_hall` | 中局9 |
| 03.04 | Captain's Cut | `@SSAnne.captain_hall` | `talk(captain)`; back-rub `scene`; HM01; leave; ship departs; `PARTY(cut_user)`; `TEACH(Cut, slot)`. Squirtle can't learn Cut: needs a caught Cut user (WR: Rattata from Route 1). | `@Vermilion.center_counter` | 中局10 |
| 03.05 | Surge's switches | `@Vermilion.center_counter` | `FIELD(Cut, gym_tree)`; gym trainers on the path; `PUZZLE(two_trash_switches)` read from the screen, bounded attempts | `@VermilionGym.surge_front` | 中局11 |
| 03.06 | Thunder Badge | `@VermilionGym.surge_front` | `PREP(Surge)` (needs a Ground or other validated plan); `battle(Surge)`; Thunder Badge, TM34; Center `heal` | `@Vermilion.center_counter` | 中局11 |

### Segment 04 — Route 11, Diglett's Cave, Route 2 east, Museum, Routes 9/10 (Part 7)

Default skips Flash and walks Rock Tunnel dark, as the WR does.

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 04.01 | `[opt: flash]` Diglett's Cave to HM05 | `@Vermilion.center_counter` | Route 11; Diglett's Cave; Route 2 east gate aide (needs 10 caught: `CAPTURE` until met); `TEACH(Flash)`; `[opt: old_amber]` museum side door via Cut; back through the cave | `@Vermilion.center_counter` | *中局12* |
| 04.02 | Back to Cerulean | `@Vermilion.center_counter` | Route 6; Underground Path; Route 5; `[opt: bike_voucher]` Bike Shop exchange; Center `heal` | `@Cerulean.center_counter` | 中局13 |
| 04.03 | Routes 9 and 10 | `@Cerulean.center_counter` | Cut the east tree; Route 9 trainers on the path; Route 10 Center `heal` | `@Route10.center_counter` | 中局13 |

### Segment 05 — Rock Tunnel, Route 10 south, Lavender, Route 8, Underground Path, Route 7 (Part 8)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 05.01 | Rock Tunnel to Lavender | `@Route10.center_counter` | Rock Tunnel floors and ladders, dark (or `FIELD(Flash)` with `[opt: flash]`); path trainers; Route 10 south; Lavender Center `heal`. Hears Fuji is missing; does not enter the Tower. | `@Lavender.center_counter` | 中局13, 中局14 |
| 05.02 | `[opt: early_tower]` Tower 2F rival | `@Lavender.center_counter` | Enter the Tower; 2F rival trigger (decomp `PokemonTower_2F` coord events (17,5), (16,6)); `battle(rival_tower)`; 3F ghosts can't be fought without the Scope: flee | `@Lavender.center_counter` | 中局14, *中局15*, *中局16* |
| 05.03 | West to Celadon | `@Lavender.center_counter` | Route 8; Underground Path (7–8); Route 7; Celadon Center. No Saffron gate before the Tea. | `@Celadon.center_counter` | 中局17 |

### Segment 06 — Celadon, Celadon Gym, Rocket Hideout, Pokemon Tower (Part 9)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 06.01 | Celadon services | `@Celadon.center_counter` | Mansion 1F old lady: Tea; Dept. Store by budget `[opt: coin_case]` `[opt: eevee]` | `@Celadon.center_counter` | *中局19*, *中局23*, 中局24 |
| 06.02 | Rainbow Badge | `@Celadon.center_counter` | Cut the gym tree; gym trainers on the path; `PREP(Erika)`; `battle(Erika)`; Rainbow Badge, TM19 | `@Celadon.center_counter` | 中局18 |
| 06.03 | Hideout entrance | `@Celadon.center_counter` | Game Corner; `battle(poster_grunt)`; press the poster switch; stairs | `@RocketHideoutB1F.stairs` | 中局20 |
| 06.04 | Lift Key | `@RocketHideoutB1F.stairs` | Spinner floors to B4F northwest room; `battle(key_grunt)`; **talk again**; pick up the Lift Key | `@RocketHideoutB4F.key_room` | 中局21 |
| 06.05 | Hideout Giovanni | `@RocketHideoutB4F.key_room` | Lift to B4F; door guards; `PREP(giovanni_hideout)`; `battle(giovanni_hideout)`; Silph Scope | `@Celadon.center_counter` | 中局22 |
| 06.06 | Tea and back to Lavender | `@Celadon.center_counter` | Route 7 gate: guard takes the Tea; through Saffron; Route 8 gate; Lavender Center | `@Lavender.center_counter` | 中局25, 中局26 |
| 06.07 | Tower rival and ascent | `@Lavender.center_counter` | Tower; 2F rival (skipped if `early_tower` won it); 3F–5F with the healing zone | `@PokemonTower5F.stairs_up` | 中局15 (if not early) |
| 06.08 | Marowak | `@PokemonTower5F.stairs_up` | 6F: Scope reveals Marowak; `battle(marowak_ghost)` — can't be caught | `@PokemonTower6F.stairs_up` | 中局27 |
| 06.09 | Fuji's rescue | `@PokemonTower6F.stairs_up` | 7F: three Rockets; Fuji; escort home | `@FujiHouse.fuji_front` | 中局28 |
| 06.10 | Poke Flute | `@FujiHouse.fuji_front` | `talk(Fuji)`; Poke Flute; Center `heal` | `@Lavender.center_counter` | 中局29 |

### Segment 07 — Routes 12–18, Fuchsia, Fuchsia Gym, Safari Zone (Part 10)

`south_road` picks one Snorlax; the other road is never run.

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 07.01 | `[opt: fly]` HM02 Fly | `@Lavender.center_counter` | Through Saffron to Celadon; Route 16 north via Cut (no Snorlax); house: HM02; `TEACH(Fly)`; `FIELD(Fly, Lavender)` | `@Lavender.center_counter` | *中局37* |
| 07.02 | EAST: Route 12 Snorlax | `@Lavender.center_counter` | Route 12; flute; `battle(snorlax)` by policy (flee \| defeat \| capture) | `@Route12.snorlax_tile` | 中局30, *中局31* |
| 07.03 | EAST: Routes 12–15 to Fuchsia | `@Route12.snorlax_tile` | Routes 12, 13, 14, 15 trainers on the path `[opt: super_rod]` `[opt: itemfinder]` `[opt: exp_share]`; Fuchsia Center | `@Fuchsia.center_counter` | *中局32*, *中局33*, 中局34, *中局35* |
| 07.04 | WEST: Cycling Road to Fuchsia | `@Lavender.center_counter` | Needs `bike_voucher`. To Celadon; Route 16 flute and Snorlax; `[opt: amulet_coin]`; Cycling Road (17) downhill; Route 18; Fuchsia Center | `@Fuchsia.center_counter` | *中局36*, *中局38*, 中局39 |
| 07.05 | Soul Badge | `@Fuchsia.center_counter` | Invisible-wall gym path; trainers on it; `PREP(Koga)`; `battle(Koga)`; Soul Badge, TM06 | `@Fuchsia.center_counter` | 中局40 |
| 07.06 | Safari: Gold Teeth and Surf | `@Fuchsia.center_counter` | Pay; Safari encounter mode (never the normal battle handler); Areas 1→3; Gold Teeth; Secret House: HM03; leave by a known exit | `@Fuchsia.center_counter` | 中局41, 中局42 |
| 07.07 | Strength, party | `@Fuchsia.center_counter` | Warden: teeth → HM04; `TEACH(Strength)`, `TEACH(Surf)` to chosen members `[opt: good_rod]` | `@Fuchsia.center_counter` | 中局41, *中局43* |

### Segment 08 — Saffron, Silph Co., Fighting Dojo, Saffron Gym (Part 11)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 08.01 | To Saffron | `@Fuchsia.center_counter` | Back the way `south_road` came (or `FIELD(Fly)`); Saffron Center | `@Saffron.center_counter` | 终局6 |
| 08.02 | `[opt: dojo_gift]` Fighting Dojo | `@Saffron.center_counter` | Dojo trainers; Karate King; `CHOICE(hitmonlee \| hitmonchan)` | `@Saffron.center_counter` | *终局7* |
| 08.03 | Card Key | `@Saffron.center_counter` | Silph to 5F; key corridor; Card Key | `@Silph5F.card_key` | 终局8 |
| 08.04 | Silph rival | `@Silph5F.card_key` | 3F middle room door; warp to 7F; `PREP(rival_silph)`; `battle(rival_silph)` `[opt: lapras_gift]` | `@Silph7F.rival_room` | 终局9 |
| 08.05 | Silph Giovanni | `@Silph7F.rival_room` | Warp to 11F; `PREP(giovanni_silph)`; `battle(giovanni_silph)`; President: Master Ball (WR skips; SPEEDRUN may too) | `@Saffron.center_counter` | 终局10 |
| 08.06 | Marsh Badge | `@Saffron.center_counter` | Gym warp sequence; `PREP(Sabrina)`; `battle(Sabrina)`; Marsh Badge, TM04 | `@Saffron.center_counter` | 终局11 |

### Segment 09 — Routes 19/20, Seafoam Islands (Part 12)

`cinnabar_approach` picks one row. ROUTE21 is the default: no current or
boulder puzzle, and it is the WR's road.

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 09.01 | ROUTE21: Pallet and Route 21 | `@Saffron.center_counter` | To Pallet (walk via Vermilion, Diglett's Cave and Route 2, or `FIELD(Fly)`); `FIELD(Surf)` south on Route 21; Cinnabar Center | `@Cinnabar.center_counter` | *中局44* (second half), 终局12 |
| 09.02 | SEAFOAM: Routes 19/20 and Seafoam | `@Saffron.center_counter` | To Fuchsia; Surf Routes 19, 20 east; Seafoam floors with Strength boulders and currents `[opt: articuno — STOP_AT before Articuno]`; Route 20 west; Cinnabar Center | `@Cinnabar.center_counter` | *中局44* (first half), *终局4*, *终局5*, 终局12 |

### Segment 10 — Cinnabar Island, Pokemon Lab, Mansion, Cinnabar Gym (Part 13)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 10.01 | Secret Key | `@Cinnabar.center_counter` | `[opt: fossil_revival]` hand the fossil to the Lab first; Mansion stairs and statue switches; drop to B1F; Secret Key | `@Cinnabar.center_counter` | *终局13*, 终局14 |
| 10.02 | Volcano Badge | `@Cinnabar.center_counter` | Unlock the gym; each quiz answered from the question read, or its trainer fought; `PREP(Blaine)`; `battle(Blaine)`; Volcano Badge, TM38 | `@CinnabarGym.door` | 终局15 |
| 10.03 | Bill's invitation | `@CinnabarGym.door` | Bill's scene fires outside (`CinnabarIsland_EventScript_BillScene`, YES/NO `ComeWithMeToOneIsland`). `answer(NO)` by default → Bill waits in the Center; `answer(YES)` with `[opt: sevii_before_league]` → segment 11 | `@Cinnabar.center_counter` (NO) / `@OneIsland.harbor` (YES) | *终局16* |

### Segment 11 — Sevii Islands I: One, Two, Three Island (Part 14)

Runs here only with `sevii_before_league`; otherwise the same rows run at the
start of the postgame (its first start is Bill in the Cinnabar Center, who
takes the player when spoken to). Required before `15.02` either way.

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 11.01 | One Island | `@OneIsland.harbor` (or `@Cinnabar.center_counter`, talk to Bill) | Ferry; Center scene (`OneIsland_PokemonCenter_1F`): Meteorite, Tri-Pass, then the Town Map if the rival's-house scene var is below 2 (else an extra map page), Sevii map. PC storage is off from here (`FLAG_SYS_PC_STORAGE_DISABLED`). | `@OneIsland.center_counter` | 终局17, 终局18, 终局19, 终局21, 终局22 |
| 11.02 | Two Island request | `@OneIsland.center_counter` | `TRAVEL(ferry, TwoIsland)`; Game Corner owner: Lostelle is missing | `@TwoIsland.center_counter` | 终局20, 终局23, 终局24 |
| 11.03 | Berry Forest rescue | `@TwoIsland.center_counter` | Ferry to Three Island; biker fights, each named; Bond Bridge; Berry Forest; `battle(hypno)` by policy; Lostelle; automatic return to Two Island | `@TwoIsland.game_corner` | 终局25, 终局26, 终局27 |
| 11.04 | Meteorite, back to Cinnabar | `@TwoIsland.game_corner` | `talk(father)`: Meteorite → Moon Stone; ferry to One Island; `talk(Bill)` → Cinnabar | `@Cinnabar.center_counter` | 终局28–终局32 |

### Segment 12 — Route 21, Power Plant, Viridian Gym (Part 15)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 12.01 | North to Viridian | `@Cinnabar.center_counter` | `FIELD(Surf)` north on Route 21 (or `FIELD(Fly)`); Pallet; Route 1; Viridian Center | `@Viridian.center_counter` | 终局33, *终局34*, 终局35 |
| 12.02 | `[opt: zapdos]` Power Plant | `@Viridian.center_counter` | Needs `fly`. Fly to Route 10 Center; Surf to the Plant; item balls vs Voltorb/Electrode; `STOP_AT(Zapdos)`; capture only if asked; Fly back | `@Viridian.center_counter` | *终局1*, *终局2*, *终局3* |
| 12.03 | Earth Badge | `@Viridian.center_counter` | Gym (seven badges); spinner path; trainers on it; `PREP(giovanni_gym)`; `battle(Giovanni)`; Earth Badge, TM26 | `@Viridian.center_counter` | 终局36 |

### Segment 13 — Route 22, Route 23, Victory Road (Part 16)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 13.01 | Route 22 rival | `@Viridian.center_counter` | West to the trigger; `PREP(rival_route22)`; `battle(rival_route22)` | `@Route22.gate_door` | 终局37 |
| 13.02 | Badge gates | `@Route22.gate_door` | League gate; Route 23 badge checks in order; Surf the water section | `@VictoryRoad1F.entry` | 终局38 |
| 13.03 | Victory Road 1F | `@VictoryRoad1F.entry` | `FIELD(Strength)`; first boulder onto its switch; `VERIFY(barrier_gone)` | `@VictoryRoad2F.entry` | 终局38 |
| 13.04 | Victory Road 2F–3F | `@VictoryRoad2F.entry` | 2F and 3F switches; drop boulder and follow it; last switch; trainers on the path; exit; Route 23 north | `@IndigoPlateau.center_counter` | 终局38, 终局39 |

### Segment 14 — Indigo Plateau (Part 17)

League rooms are event boundaries, **not automatically safe reload points**.
Choose a save policy first: replay from `14.01`'s save, or save in a room with
its exact remaining resources recorded.

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 14.01 | League staging | `@IndigoPlateau.center_counter` | `heal`; shop; `PREP(full_league)`: team, levels, PP, healing and revive budget, coverage | `@IndigoPlateau.center_counter` | 终局39 |
| 14.02 | Lorelei | `@IndigoPlateau.center_counter` | Enter; `battle(Lorelei)` with carried items only | `@Lorelei.exit_door` | 终局40 |
| 14.03 | Bruno | `@Lorelei.exit_door` | Restore with carried items; `battle(Bruno)` | `@Bruno.exit_door` | 终局40 |
| 14.04 | Agatha | `@Bruno.exit_door` | `battle(Agatha)` with a status/confusion policy | `@Agatha.exit_door` | 终局40 |
| 14.05 | Lance | `@Agatha.exit_door` | `battle(Lance)`; last preparation before the Champion trigger | `@Lance.exit_door` | 终局40 |
| 14.06 | Champion | `@Lance.exit_door` | Automatic trigger; `battle(champion_rival, starter_profile)`; Oak and rival aftermath | Hall of Fame room | 终局41 |
| 14.07 | Hall of Fame | Hall of Fame room | Register the team; credits; continue from the game's own save; observe the pose | post-HoF pose, saved | 终局41 |

**End of the main-story section.**

## Postgame (after the first Hall of Fame)

Starts after a break: the user brings the save to 60 caught species. If
`sevii_before_league` was off, segment 11 runs first.

### Segment 15 — Sevii Islands II: Ruby, Four Island, Six Island (Part 18)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 15.01 | National Pokedex | post-HoF pose | Check: post-HoF save, 60 caught (seen is not caught), segment 11 done; else stop before saving. To Pallet; Oak's upgrade scene | `@Pallet.lab_door` | 七之岛1 |
| 15.02 | Ferry to Celio | `@Pallet.lab_door` | To Vermilion (or Fly); ferry with the Tri-Pass; `talk(Celio)`: Ruby request | `@OneIsland.center_counter` | 七之岛2, 七之岛3 |
| 15.03 | Mt. Ember and the Ruby | `@OneIsland.center_counter` | Kindle Road `[opt: rock_smash — Ember Spa old man]`; Mt. Ember: watch the Rocket password scene; two grunts; Ruby Path floors; Ruby `[opt: moltres — summit, STOP_AT]` | `@OneIsland.center_counter` | 七之岛4, *七之岛5*, *七之岛6*, 七之岛7 |
| 15.04 | Rainbow Pass | `@OneIsland.center_counter` | Give the Ruby; Rainbow Pass; Sapphire request | `@OneIsland.center_counter` | 七之岛8 |
| 15.05 | Icefall Cave, Waterfall | `@OneIsland.center_counter` | Ferry to Four Island `[opt: four_island_daycare]`; Icefall Cave ice and cracked floor; HM07; `TEACH(Waterfall)` | `@FourIsland.center_counter` | 七之岛9, *七之岛10*, 七之岛11, 七之岛12 |
| 15.06 | Lorelei and the Rockets | `@FourIsland.center_counter` | Icefall Cave; `FIELD(Waterfall)`; Back Cave; Lorelei; `battle(rocket)` alongside her | `@FourIsland.center_counter` | 七之岛13 |
| 15.07 | Five Island, warehouse shut | `@FourIsland.center_counter` | Ferry to Five Island; Five Isle Meadow; warehouse door refuses (no password). Kept because 52poke has it; drop if the decomp shows the visit sets nothing the Dotted Hole needs. | `@FiveIsland.center_counter` | 七之岛14, 七之岛15 |
| 15.08 | Dotted Hole | `@FiveIsland.center_counter` | Ferry to Six Island; Water Path; Ruin Valley; Cut the braille door (`FLAG_USED_CUT_ON_RUIN_VALLEY_BRAILLE`); Dotted Hole floors; Gideon takes the Sapphire; second password | `@SixIsland.center_counter` | 七之岛18, 七之岛19, 七之岛20 |

### Segment 16 — Sevii Islands III: Five Island, Seven Island (Part 19)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 16.01 | Into the warehouse | `@SixIsland.center_counter` | Ferry to Five Island `[opt: water_labyrinth_egg]` `[opt: selphy]`; Five Isle Meadow; both passwords; enter | `@RocketWarehouse.entry` | 七之岛21, *七之岛16*, *七之岛17* |
| 16.02 | Sapphire | `@RocketWarehouse.entry` | Spinner maze; grunts; both Admins; `PREP(Gideon)`; `battle(Gideon)`; Sapphire | `@FiveIsland.center_counter` | 七之岛22 |
| 16.03 | Network complete | `@FiveIsland.center_counter` | Ferry to One Island; give Celio the Sapphire; machine scene. No link trade. | `@OneIsland.center_counter` | 七之岛23, 七之岛24 |

**End of the postgame section.** Segment 17 is all options.

### Segment 17 — Cerulean Cave, League round 2 (Part 20) and other extras

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 17.01 | `[opt: mewtwo]` Cerulean Cave | `@OneIsland.center_counter` | To Cerulean; Surf from Route 24; guard gone; 1F/2F/B1F path; `STOP_AT(Mewtwo)` — the save is before pressing A; capture only if asked | `@CeruleanCaveB1F.mewtwo_front` | *剧情后2* |
| 17.02 | `[opt: league_rematch]` League round 2 | `@IndigoPlateau.center_counter` | Rematch teams; one event per opponent; second Hall of Fame | post-HoF pose | — |
| 17.03 | `[opt: roaming_beast]` Roaming beast | any Center | Bounded search for the starter's beast; keep its one-time state | same Center | *剧情后3* |
| 17.04 | `[opt: tanoby_unown]` Tanoby | `@SevenIsland.center_counter` | Canyon Entrance; Sevault Canyon; Tanoby Key boulders; chambers open | `@SevenIsland.center_counter` | *剧情后1* |

## Dependencies

| Capability or state | Made by | Needed by |
|---|---|---|
| S.S. Ticket | 02.05 | 02.07 (officer moves), 03.02 |
| A Cut user + HM01 | caught Pokemon + 03.04 | 03.05, 04.03, 06.02, 07.01, 15.08 |
| Flash (optional) | 04.01, 10 caught | 05.01 lit; default walks dark |
| Silph Scope | 06.05 | 06.08 |
| Tea | 06.01 | 06.06 (Saffron gates) |
| Poke Flute | 06.10 | 07.02 or 07.04 |
| Bicycle | `bike_voucher` | 07.04 (WEST) |
| Surf + Soul Badge | 07.06 + 07.05 | 09.01/09.02, 12.01, 13.02 |
| Strength + Rainbow Badge | 07.07 + 06.02 | 09.02, 13.03–13.04 |
| Fly + Thunder Badge | 07.01 | 12.02, optional shortcuts |
| Seven badges / eight badges | earlier gyms / 12.03 | 12.03 / 13.02 |
| Tri-Pass, first Sevii visit | segment 11 | 15.02 |
| National Pokedex, 60 caught | user + 15.01 | rest of the postgame |
| Rainbow Pass | 15.04 | Four–Seven Island |
| Waterfall + Volcano Badge | 15.05 + 10.02 | 15.06 |

## Capabilities the list needs

| Capability | First use | What it must do |
|---|---|---|
| Broader battle/party policy | 02.01 Misty; 03.06 Surge | HP/status/PP, matchups, switching, fainted members, level/evolution/move prompts, trainer vs wild |
| Capture and caught-species count | 03.04 Cut user; 04.01; 15.01 | Bounded balls/encounters, party/PC destination, caught (not seen) count |
| Item and shop interaction | 01.02 fossil; key items | Item identity/quantity, bag space, budget, gifts that may repeat |
| HM/TM learning and field moves | 03.04 Cut | Explicit member and slot; confirm the move and the badge |
| Story-aware map state | Bill, gyms, Rocket occupation | Current objects, triggers and switches, not "remove all post-HoF objects" |
| Forced movement / puzzles | 06.04 spinners; boulders | Spinners, door state, pushed boulders, floor drops, currents, ice |
| Observed puzzle search | 03.05 Surge | Read switch/reset outcomes; bounded attempts |
| Special modes | Safari, ferry, gym teleports | Safari encounters never use the normal battle handler |
| Arrival verification | every dungeon and escort | Map, pose and facing after automatic movement |
| Save/resume manifests | every row; League above all | Story prerequisites, party, inventory, puzzle state |
| Section start checks | 15.01 | Refuse to run on a save that doesn't meet the precondition |

## First slice to implement

`01.01`–`01.03`: Route 3, Mt. Moon with Miguel and the fossil, and the descent
to Cerulean. It tests a new map set, an item choice, a required dungeon fight
and a one-way descent without committing to the Misty-onward battle policy.

For each later promotion: resolve the anchors against `map.json` and
`scripts.inc`; name the route's trainers; choose party, move and supply policy;
test table dependencies and map replay; check the real trigger and scene end on
the console; then record the save boundary.

## Coverage: every 52poke line

Line text below is an English gloss, not a quote. Ids count the page's list
lines in order, nested lines included; 终局's place-name header lines are
counted because they are list lines there. *Italic* = optional on 52poke.

### 开局 (opening)

| Line | Gloss | Checkpoint |
|---|---|---|
| 开局1 | Name the player and the rival | 00.01 |
| 开局2 | Leave home; Oak takes the player to the lab | 00.02, 00.03 |
| 开局3 | Choose the starter | 00.04 |
| 开局4 | Rival picks the counter | 00.04 |
| 开局5 | Battle the rival | 00.04 |
| 开局6 | Route 1 to Viridian | 00.05 |
| 开局7 | Mart clerk gives the Parcel | 00.05 |
| 开局8 | Back to Pallet | 00.06 |
| 开局9 | Oak gives the Pokedex | 00.06 |
| *开局10* | Daisy gives the Town Map | 00.06 `town_map` |
| 开局11 | Route 1 back to Viridian | 00.07 |
| *开局12* | Route 22 rival battle | 00.07 `route22_rival_early` |
| 开局13 | Route 2 and Viridian Forest to Pewter | 00.07, 00.08 |
| 开局14 | Brock: Boulder Badge | 00.09 |
| 开局15 | Oak's aide: Running Shoes | 00.10 |
| 开局16 | Route 3 to Mt. Moon | 01.01 |
| *开局17* | Magikarp salesman at the Center | 01.01 `magikarp` |
| 开局18 | Miguel; Dome or Helix Fossil | 01.02 |
| 开局19 | Route 4 to Cerulean | 01.03 |
| 开局20 | Misty: Cascade Badge | 02.01 |
| 开局21 | Rival before Route 24; Fame Checker | 02.02 |
| 开局22 | Route 24 trainers; Rocket's Nugget and battle | 02.03 |
| 开局23 | Route 25 to the cape | 02.04 |
| 开局24 | Restore Bill; S.S. Ticket | 02.05 |
| 开局25 | Routes 25 and 24 back to Cerulean | 02.06 |

### 中局 (middle)

| Line | Gloss | Checkpoint |
|---|---|---|
| 中局1 | Rocket in the burgled house | 02.07 |
| 中局2 | Route 5; Saffron guard turns the player back | 02.08 |
| 中局3 | Underground Path to Route 6, south to Vermilion | 02.08, 03.01 |
| *中局4* | Vs. Seeker | 03.02 `vs_seeker` |
| *中局5* | Bike Voucher; Bicycle in Cerulean | 03.02, 04.02 `bike_voucher` |
| *中局6* | Old Rod | 03.02 `old_rod` |
| 中局7 | To Vermilion Harbor | 03.02 |
| 中局8 | Board the S.S. Anne | 03.02 |
| 中局9 | Rival before the captain's room | 03.03 |
| 中局10 | Captain: HM01 Cut | 03.04 |
| 中局11 | Surge: Thunder Badge | 03.05, 03.06 |
| *中局12* | Diglett's Cave, Route 2 gate: HM05 Flash (10 caught) | 04.01 `flash` |
| 中局13 | From Cerulean: Route 9, Rock Tunnel, Route 10 to Lavender | 04.02, 04.03, 05.01 |
| 中局14 | Fuji is missing; to the Tower | 05.01 (news), 05.02 / 06.07 (Tower) |
| *中局15* | Tower 2F rival | 05.02 `early_tower`; else 06.07 |
| *中局16* | 3F ghosts can't be attacked | 05.02 `early_tower` |
| 中局17 | Route 8, Underground Path, Route 7 to Celadon | 05.03 |
| 中局18 | Erika: Rainbow Badge | 06.02 |
| *中局19* | Coin Case | 06.01 `coin_case` |
| 中局20 | Poster grunt; switch; Rocket Hideout | 06.03 |
| 中局21 | B4F grunt: Lift Key | 06.04 |
| 中局22 | Giovanni: Silph Scope | 06.05 |
| *中局23* | Eevee on the Mansion roof | 06.01 `eevee` |
| 中局24 | Tea from the Mansion old lady | 06.01 |
| 中局25 | Tea to the Saffron guard | 06.06 |
| 中局26 | Back to Lavender | 06.06 |
| 中局27 | Scope reveals Marowak; can't be caught | 06.08 |
| 中局28 | 7F: three Rockets | 06.09 |
| 中局29 | Fuji: Poke Flute | 06.10 |
| 中局30 | Choose Route 12, Cycling Road or Silph next | 07.02 (`south_road`); Silph is segment 08 |
| *中局31* | Route 12 Snorlax | 07.02 (`south_road=EAST`, default) |
| *中局32* | Super Rod | 07.03 `super_rod` |
| *中局33* | Itemfinder (30 caught) | 07.03 `itemfinder` |
| 中局34 | Routes 13–15 to Fuchsia | 07.03 |
| *中局35* | Exp. Share (50 caught) | 07.03 `exp_share` |
| *中局36* | Route 16 Snorlax | 07.04 (`south_road=WEST`) |
| *中局37* | HM02 Fly | 07.01 `fly` |
| *中局38* | Amulet Coin (40 caught) | 07.04 `amulet_coin` |
| 中局39 | Routes 16–18 to Fuchsia | 07.04 (WEST only) |
| 中局40 | Koga: Soul Badge | 07.05 |
| 中局41 | Gold Teeth to the Warden: HM04 Strength | 07.06, 07.07 |
| 中局42 | Secret House: HM03 Surf | 07.06 |
| *中局43* | Good Rod | 07.07 `good_rod` |
| *中局44* | Routes 19/20 to Seafoam, or Pallet + Route 21 to Cinnabar | 09.01 / 09.02 (`cinnabar_approach`) |

### 终局 (endgame)

| Line | Gloss | Checkpoint |
|---|---|---|
| *终局1* | Power Plant (header) | 12.02 `zapdos` |
| *终局2* | Surf from Route 10 to the Plant | 12.02 `zapdos` |
| *终局3* | Zapdos in the Plant | 12.02 `zapdos` |
| *终局4* | Seafoam Islands (header) | 09.02 |
| *终局5* | Articuno in Seafoam | 09.02 `articuno` |
| 终局6 | Saffron City (header) | 08.01 |
| *终局7* | Dojo: Hitmonlee or Hitmonchan | 08.02 `dojo_gift` |
| 终局8 | Silph: 5F Card Key | 08.03 |
| 终局9 | 3F warp to 7F rival; *Lapras gift* | 08.04 (`lapras_gift`) |
| 终局10 | 11F Giovanni; President: Master Ball | 08.05 |
| 终局11 | Sabrina | 08.06 |
| 终局12 | Cinnabar Island (header) | 09.01 / 09.02 |
| *终局13* | Lab revives fossils | 10.01 `fossil_revival` |
| 终局14 | Mansion: Secret Key | 10.01 |
| 终局15 | Blaine | 10.02 |
| *终局16* | Bill outside the gym; ferry to One Island | 10.03 `sevii_before_league` |
| 终局17 | One Island (header) | 11.01 |
| 终局18 | Tri-Pass | 11.01 |
| 终局19 | Celio; Meteorite | 11.01 |
| 终局20 | Ferry to Two Island | 11.02 |
| 终局21 | Town Map here if missed in Pallet | 11.01 (automatic in the Center scene) |
| 终局22 | Sevii Town Map page | 11.01 |
| 终局23 | Two Island (header) | 11.02 |
| 终局24 | Lostelle's father; search Three Island | 11.02 |
| 终局25 | Three Island (header) | 11.03 |
| 终局26 | To Berry Forest | 11.03 |
| 终局27 | Find Lostelle; Hypno; back to Two Island | 11.03 |
| 终局28 | Two Island (header) | 11.04 |
| 终局29 | Meteorite to the father; Moon Stone | 11.04 |
| 终局30 | Back to One Island | 11.04 |
| 终局31 | One Island (header) | 11.04 |
| 终局32 | Bill back to Cinnabar | 11.04 |
| 终局33 | Cinnabar Island (header) | 12.01 |
| 终局34 | *Route 21 back to Pallet*, then Viridian | 12.01 (default while `fly` is off) |
| 终局35 | Viridian City (header) | 12.01 |
| 终局36 | Giovanni: Earth Badge | 12.03 |
| 终局37 | Route 22 rival | 13.01 |
| 终局38 | League gate, Victory Road, Indigo Plateau | 13.02, 13.03, 13.04 |
| 终局39 | Indigo Plateau (header) | 13.04, 14.01 |
| 终局40 | Elite Four | 14.02–14.05 |
| 终局41 | Rival; Champion; Hall of Fame | 14.06, 14.07 |

### 七之岛 (Sevii Islands, postgame)

| Line | Gloss | Checkpoint |
|---|---|---|
| 七之岛1 | 60 caught: National Pokedex from Oak | 15.01 |
| 七之岛2 | Ferry from Vermilion to One Island | 15.02 |
| 七之岛3 | Celio: find the Ruby | 15.02 |
| 七之岛4 | Kindle Road to Mt. Ember | 15.03 |
| *七之岛5* | Ember Spa: HM06 | 15.03 `rock_smash` |
| *七之岛6* | Moltres | 15.03 `moltres` |
| 七之岛7 | Rockets at the foot; Ruby in the cave | 15.03 |
| 七之岛8 | Ruby → Rainbow Pass; find the Sapphire | 15.04 |
| 七之岛9 | To Four Island | 15.05 |
| *七之岛10* | Four Island Day Care | 15.05 `four_island_daycare` |
| 七之岛11 | Into Icefall Cave | 15.05 |
| 七之岛12 | HM07 inside | 15.05 |
| 七之岛13 | Over the waterfall: Rockets with Lorelei | 15.06 |
| 七之岛14 | To Five Island | 15.07 |
| 七之岛15 | Warehouse shut: no password | 15.07 |
| *七之岛16* | Water Labyrinth egg | 16.01 `water_labyrinth_egg` |
| *七之岛17* | Lost Cave: Selphy's gift | 16.01 `selphy` |
| 七之岛18 | To Six Island | 15.08 |
| 七之岛19 | Ruin Valley; Cut into the Dotted Hole | 15.08 |
| 七之岛20 | Gideon takes the Sapphire; password | 15.08 |
| 七之岛21 | Back to Five Island; into the warehouse | 16.01 |
| 七之岛22 | Beat Gideon: Sapphire | 16.02 |
| 七之岛23 | Back to One Island | 16.03 |
| 七之岛24 | Sapphire to Celio; link with RS | 16.03 (no link trade) |

### 七之岛剧情后 (after the Sevii story) and 传说的宝可梦 (legendaries table)

| Line | Gloss | Checkpoint |
|---|---|---|
| *剧情后1* | Seven Island: Tanoby Ruins, Unown | 17.04 `tanoby_unown` |
| *剧情后2* | Cerulean Cave: Mewtwo | 17.01 `mewtwo` |
| *剧情后3* | Roaming beasts | 17.03 `roaming_beast` |
| *剧情后4* | MysticTicket: Navel Rock, Lugia, Ho-Oh | `navel_rock` (unsupported) |
| *剧情后5* | AuroraTicket: Birth Island, Deoxys | `birth_island` (unsupported) |
| *剧情后6* | Diploma for a completed Pokedex | `diploma` (unsupported) |
| 传说1 | Articuno, Seafoam | 09.02 `articuno` |
| 传说2 | Zapdos, Power Plant | 12.02 `zapdos` |
| 传说3 | Moltres, Mt. Ember | 15.03 `moltres` |
| 传说4 | Mewtwo, Cerulean Cave, after the Sevii story | 17.01 `mewtwo` |
| 传说5–7 | Raikou / Entei / Suicune by starter, after the Sapphire | 17.03 `roaming_beast` |
| 传说8–9 | Lugia, Ho-Oh, Navel Rock | `navel_rock` (unsupported) |
| 传说10 | Deoxys, Birth Island | `birth_island` (unsupported) |

No line is unplaced. Three are placed with a caveat: 中局14 splits between
`05.01` (the news) and the Tower rows; 中局30 is a choice line, resolved by
`south_road` and segment order; 七之岛14–15 (`15.07`) may turn out to be a
visit that changes nothing.
