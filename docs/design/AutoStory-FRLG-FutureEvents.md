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
- **What a fast route skips** comes from the [speedrun notes](AutoStory-FRLG-SpeedrunNotes.md), a sampled review of
  pokeguy's LeafGreen 1:59:34 any% run, quoted as "WR" below.
- **RNG handoffs** follow the upstream FRLG RNG programs' docs pages.

## Reading the rows

Each checkpoint starts from the previous checkpoint's end save, unless its
Start says otherwise, and ends by saving at its End. A row's postcondition (the
badge, item or gate it exists for) must be observed before that save; a
visible destination or a closed dialog is not enough.

Columns: **ID**, **Title**, **Start** (pose or anchor), **Does** (ordered
steps), **End / save**, **52poke** (lines covered; italic lines in *italics*).
`[opt: name]` marks a step or row that runs only when that option is on (see
the [Options reference](#options-reference)). `[team: …]` marks a step the
[team plan](#team-plan) adds for some `starter` × `game_version`.

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

## Run modes and sections

`run_mode` (see [Run control](#run-control)) picks SPEEDRUN, DEFAULT or SAFE.
Rows describe DEFAULT.

**Sections** follow the story-sections rule (`docs/design/AutoStory-StoryTracks.md`
on `docs/story-tracks`): a section ends where the user must step in.

| Section | Segments | Ends at | Why the break |
|---|---|---|---|
| Main story | 00–14 | `14.07`, first Hall of Fame | The National Pokedex (`15.01`) needs 60 caught species; collecting them is the user's job (or a separate program) |
| Postgame | (11 if deferred), 15–16, optional 17 | `16.03`, network machine complete | — |

Postgame rows assume the first Hall of Fame. `15.01` checks its precondition
(post-HoF save, 60 caught) at the start and stops with a clear message before
saving anything.

## Options reference

Every option the list uses is defined here once. Rows mark the step an option
adds with `[opt: key]`; other sections link here instead of restating an
option. Defaults keep today's behavior: off, or the value today's table or the
WR takes. Options marked **exists** are in #120's program today; **reserved**
ones belong to the segment-00 work in progress; the rest are planned.

Each table has the same columns:

- **Key**: the name used in rows and checks.
- **Values**: type and values; the default in **bold**.
- **Changes**: what it adds or changes.
- **Rows**: the checkpoints it touches.
- **Needs / conflicts**: options or story state it depends on, and what it
  can't be combined with. The run-start check enforces these; all of them are
  collected in [Checks at run start](#checks-at-run-start).
- **User data**: what the user supplies; "—" for nothing.

Groups: [run control](#run-control), [story choices](#story-choices),
[optional story](#optional-story-52poke-italic-lines),
[team](#team), [legendaries and extras](#legendaries-and-extras),
[economy and grinding](#economy-and-grinding), [glitches](#glitches),
[RNG](#rng).

### Run control

| Key | Values | Changes | Rows | Needs / conflicts | User data |
|---|---|---|---|---|---|
| `start_checkpoint`, `end_checkpoint` | checkpoint id; **first**, **last** registered | Range to run, both inclusive; the program loads the start's save. **Exists** (`StoryRunOptions`). | all | start not after end | — |
| `new_game_from_title` | bool, **on** | Start 00.01 from the title screen with no save. **Exists.** | 00.01 | needed when the start is 00.01 | — |
| `attempts_per_checkpoint` | 1–20, **3** | A failed checkpoint reloads its save and retries. **Exists.** | all | — | — |
| `stop_at_first_failure` | bool, **off** | Stop where it failed, without reloading. **Exists.** | all | — | — |
| `checks` | **THOROUGH** \| FAST | How much each step confirms on screen. **Exists.** | all | — | — |
| `language` | **English** \| the game's other languages | Text reads (shop, Bag). **Exists.** Also picks the RNG seed list. | all | RNG seed lists exist for English and Japanese only; other languages use English | — |
| `game_version` | **FIRERED** \| LEAFGREEN | Wild tables and version exclusives, Game Corner prizes, the [team plan](#team-plan) row | 05.03, 06.01, 07.03, team plan rows | must match the game and the RNG plan's version | — |
| `run_mode` | SPEEDRUN \| **DEFAULT** \| SAFE | Which battles to take, fleeing wilds, heal threshold, purchases (`autostory/DEMO_PLAN.md` on the notes branch). SPEEDRUN turns on the WR's travel options (`bike_voucher`, `fly`, `south_road=WEST`; [speedrun notes](AutoStory-FRLG-SpeedrunNotes.md)) and may drop non-italic steps the WR skips, such as Silph's Master Ball. | all | — | — |
| `player_name` | **reserved** | Player-name setting | 00.01 | segment-00 PR | the name |

### Story choices

One value is always taken.

| Key | Values | Changes | Rows | Needs / conflicts | User data |
|---|---|---|---|---|---|
| `starter` | BULBASAUR \| CHARMANDER \| **SQUIRTLE** | The ball in 00.04; the rival's starter and parties; HM users, supporters and catches ([team plan](#team-plan)); the roaming beast. Today's table implements SQUIRTLE only. | 00.04, 03.04, 07.07, 14.06, 17.03, team plan rows | BULBASAUR and CHARMANDER need a Surf user before 09.01 (check S1) | — |
| `fossil` | DOME \| **HELIX** (the WR's) | 01.02's `CHOICE(fossil)` | 01.02, 10.01 | `fossil_revival.rng` species must match | — |
| `south_road` | **EAST** \| WEST | Which Snorlax road reaches Fuchsia; the other is never run | 07.02–07.04 | WEST needs `bike_voucher`; EAST passes the Super Rod house (`super_rod`) | — |
| `snorlax_policy` | FLEE \| **DEFEAT** \| CAPTURE | `battle(snorlax)` on the chosen road (default proposed: EXP) | 07.02 or 07.04 | CAPTURE needs balls; `south_road.rng_snorlax` implies CAPTURE | — |
| `hypno_policy` | FLEE \| **DEFEAT** \| CAPTURE | `battle(hypno)` in Berry Forest (default proposed) | 11.03 | `rng_hypno` implies CAPTURE | — |
| `cinnabar_approach` | **ROUTE21** \| SEAFOAM | Pallet + Route 21, or Routes 19/20 and Seafoam | 09.01 / 09.02 | SEAFOAM needs Strength; `articuno` needs SEAFOAM | — |
| `sevii_before_league` | bool, **off** | On: accept Bill on Cinnabar and play segment 11 before Giovanni. Off: decline, as the WR does (1:23:10 in the [speedrun notes](AutoStory-FRLG-SpeedrunNotes.md)); segment 11 runs at the start of the postgame. | 10.03, segment 11 | — | — |

Italic 52poke lines that sit on the default path anyway: one of
*中局31*/*中局36* (a Snorlax must be passed), one half of *中局44* (some
route to Cinnabar), and *终局34* (Route 21 north; italic because Fly replaces
it).

### Optional story (52poke italic lines)

All bool, default **off**, no user data unless noted. Each is one italic
52poke line (the coverage table maps them).

| Key | Changes | Rows | Needs / conflicts |
|---|---|---|---|
| `town_map` | Daisy's Town Map in the rival's house | 00.06 | Off: 11.01's One Island scene gives the Town Map instead |
| `route22_rival_early` | Optional first Route 22 rival battle | 00.07 | — |
| `magikarp` | Buy the Magikarp at the Route 4 Center (¥500) | 01.01 | ¥500 on hand. Gyarados (Lv20) can Surf: a fallback Surf user |
| `vs_seeker` | Vs. Seeker from the girl in Vermilion Center | 03.02 | needed by `nugget_bridge_loop` |
| `bike_voucher` | Fan Club voucher; Bicycle in Cerulean | 03.02, 04.02 | needed by `south_road=WEST` and `water_labyrinth_egg.rng` |
| `old_rod` | Old Rod from the Fishing Guru's house | 03.02 | — |
| `flash` | Diglett's Cave → Route 2 aide → HM05 Flash; `CAPTURE` until 10 caught | 04.01, 05.01 | a Flash user ([team plan](#hm-users)) |
| `old_amber` | Pewter Museum side door (Cut) → Old Amber; runs the 04.01 trip | 04.01 | Cut (03.04); needed for the Aerodactyl target |
| `early_tower` | Pokemon Tower before Celadon: 2F rival, ghosts on 3F | 05.02, 06.08 | 06.08 skips the rival if this won it |
| `coin_case` | Coin Case from the man in the Celadon restaurant | 06.01 | needed by `game_corner_prize`, `prize_tm` |
| `eevee` | Eevee (Lv25, `CeladonCity_Condominiums_RoofRoom`) | 06.01 | With a Water Stone (same store): Vaporeon, a Surf user in both versions |
| `fly` | HM02 Fly from the Route 16 house; enables Fly travel | 07.01, 08.01, 09.01, 12.01 | a Fly user ([team plan](#hm-users)); Thunder Badge; needed by `zapdos` |
| `super_rod` | Super Rod from the Route 12 fishing house (south of the Snorlax: house warp (12,86), Snorlax (14,70), `Route12/map.json`) | 07.03 | EAST passes it; from WEST it is reached from Route 13 (**unverified**). Turned on by the LeafGreen Starmie plan |
| `itemfinder` | Itemfinder from the Route 11 gate aide | 07.03 | 30 caught |
| `exp_share` | Exp. Share from the Route 15 gate aide | 07.03 | 50 caught |
| `amulet_coin` | Amulet Coin from the Route 16 gate aide; doubles prizes ([money plan](#sources)) | 07.04 | 40 caught; on the WEST road's row |
| `good_rod` | Good Rod from the Fishing Guru's brother in Fuchsia | 07.07 | — |
| `dojo_gift` | **OFF** \| HITMONLEE \| HITMONCHAN: Karate King, then the chosen ball | 08.02 | — |
| `lapras_gift` | Lapras (Lv25, `SilphCo_7F` `givemon SPECIES_LAPRAS, 25`) after the rival | 08.04 | Turned on by the FireRed Bulbasaur/Charmander plans (their Surf user) |
| `fossil_revival` | Revive the Mt. Moon fossil (and Old Amber) at the Cinnabar Lab | 10.01 | — |
| `rock_smash` | HM06 from the old man at Ember Spa | 15.03 | — |
| `four_island_daycare` | Leave Pokemon at the Four Island Day Care | 15.05 | user picks the Pokemon |
| `water_labyrinth_egg` | Egg from the old man at Water Labyrinth | 16.01 | a free party slot |
| `selphy` | Lost Cave: rescue Selphy; show her the asked-for Pokemon | 16.01 | the asked-for Pokemon (user data) |

### Team

| Key | Values | Changes | Rows | Needs / conflicts | User data |
|---|---|---|---|---|---|
| `supporters` | **REQUIRED** \| FULL | REQUIRED: only the HM and Surf users the [team plan](#team-plan) marks required for `starter` × `game_version`. FULL: also the matchup supporters, their stones and TMs. SQUIRTLE + REQUIRED is the WR's team, today's default. | the team plan's catch rows | FULL adds stone purchases ([money plan](#sinks)); some FULL catches turn on `super_rod` | — |

### Legendaries and extras

Each legendary option is **OFF** \| STOP \| CAPTURE: STOP saves facing the
Pokemon and stops (`STOP_AT`); CAPTURE also battles it to capture.

| Key | Values | Changes | Rows | Needs / conflicts | User data |
|---|---|---|---|---|---|
| `articuno` | OFF / STOP / CAPTURE | Seafoam B4F | 09.02 | `cinnabar_approach=SEAFOAM` | — |
| `zapdos` | OFF / STOP / CAPTURE | Power Plant trip | 12.02 | `fly` | — |
| `moltres` | OFF / STOP / CAPTURE | Mt. Ember summit | 15.03 | — | — |
| `mewtwo` | OFF / STOP / CAPTURE | Cerulean Cave | 17.01 | postgame | — |
| `roaming_beast` | bool, **off** | Bounded search for the beast set by the starter: Bulbasaur → Entei, Charmander → Suicune, Squirtle → Raikou (`roamer.c` `GetRoamerSpecies`) | 17.03 | — | — |
| `tanoby_unown` | bool, **off** | Sevault Canyon, Tanoby Key, Unown chambers | 17.04 | — | — |
| `league_rematch` | bool, **off** | Elite Four round 2 (not on 52poke) | 17.02 | — | — |
| `navel_rock`, `birth_island` | **unsupported** | Need event tickets (MysticTicket, AuroraTicket) | — | — | — |
| `diploma` | **unsupported** | Game Freak, Celadon Mansion 3F; needs a completed Pokedex | — | a dex-completion program | — |

### Economy and grinding

GRIND steps are part of DEFAULT, not options; see the [team plan](#levels-and-grind-steps).

| Key | Values | Changes | Rows | Needs / conflicts | User data |
|---|---|---|---|---|---|
| `nugget_bridge_loop` | **AUTO** \| N ≥ 0 | N Vs. Seeker loops of Routes 24/25 for money and EXP. AUTO takes N from the [money plan](#ledger-default-east-road-route-21-no-options) at run start: 0 when there is no deficit. Also serves the glitch-setup GRINDs. | 04.02 | N > 0 needs `vs_seeker` | optional N override |
| `prize_tm` | **NONE** \| list of Game Corner TMs (TM13, TM23, TM24, TM30, TM35) | Buy coins and the TMs; prices in the [money plan](#sinks) | 06.01 | `coin_case`; money | which TMs |
| `game_corner_prize` | **NONE** \| a prize Pokemon from the version's list | Buy coins (bounded budget, no gambling) and the prize | 06.01 | `coin_case`; the species must be in `game_version`'s list | which Pokemon |

### Glitches

| Key | Values | Changes | Rows | Needs / conflicts | User data |
|---|---|---|---|---|---|
| `item_dupe` | bool, **off** | Mail glitch duplication ([Item duplication](#item-duplication-item_dupe-the-mail-glitch)). Changes the save for good. Replaces sweeper GRINDs after 06.07 with Rare Candies. | 01.01, 02.04, 03.02, 04.01, 04.02, 05.03, 06.01, 06.06, 06.07, later `ITEM(rare_candy)` | the 04.01 trip; Twins Eli & Anne unfought before 06.07; Box 3 Slot 1 empty from 06.07 on | user's consent to the save risk |
| `item_dupe.rare_candies` | count, **34** (+ the second carrier's, [team plan](#levels-and-grind-steps)) | Candies to duplicate | 06.07 | `item_dupe` | optional count |
| `item_dupe.nuggets` | count, **8** | Nuggets to duplicate and sell | 06.07 | `item_dupe` | optional count |
| `item_dupe.handoff` | bool, **on** | Hand off to upstream's ItemDuplication program instead of duplicating in-table | 06.07 | `item_dupe` | — |

### RNG

| Key | Values | Changes | Rows | Needs / conflicts | User data |
|---|---|---|---|---|---|
| `rng_plan` | table, **empty** | Each listed target turns on its handoff: save, stop, tell the user what to run ([RNG plan setting](#rng-plan-setting)) | per target | each target's parent option on and checkpoint in range (check R2) | the [plan's fields](#rng-plan-setting): version, language, sound, profile, TID/SID, per-target seed and advances |
| `<target>.rng` | derived from `rng_plan`, not set directly | One handoff per target ([RNG handoffs](#rng-handoffs)) | per target | its parent option, if any | from the plan |
| `sid_helper_handoff` | **reserved** | Stop at the naming screen before OK for SID Helper; resume after it | 00.01 | segment-00 PR | — |
| `starter_rng_handoff` | **reserved** | Split 00.04 to save under the ball for StarterRng | 00.04 | target species = `starter` | seed, advances |

### Checks at run start

Before anything is saved, the program refuses an option set that breaks one
of these, with a message naming the rule.

| # | Rule |
|---|---|
| O1 | `south_road=WEST` needs `bike_voucher`. |
| O2 | `game_corner_prize` and `prize_tm` need `coin_case`; `nugget_bridge_loop` N > 0 needs `vs_seeker`. |
| O3 | `zapdos` needs `fly`; `articuno` needs `cinnabar_approach=SEAFOAM`. |
| O4 | `flash`, `fly`: the [team plan](#hm-users) has a member that learns it, caught before the row. |
| O5 | Caught-count gates (`flash` 10, `itemfinder` 30, `amulet_coin` 40, `exp_share` 50) are met by the planned catches before the row, or the row's bounded `CAPTURE` closes the gap. |
| S1 | `starter` BULBASAUR or CHARMANDER needs a Surf user before 09.01: `lapras_gift`, LeafGreen Staryu (`super_rod`), `eevee` + Water Stone, or `magikarp` (Gyarados). The team plan turns the first two on; turning them off without another source is an error. |
| S2 | `item_dupe`: the battle policy must not fight Twins Eli & Anne in 05.03 or 06.06; Box 3 Slot 1 stays empty from 06.07. |
| R1 | Each Target Seed is found by `SeedsDatabase::find_seed` in the list for the version and language, with that sound setting (as the RNG programs check at their start). |
| R2 | Each target's checkpoint is in the start–end range and not skipped by a route choice or a parent option that is off. Examples: Articuno with ROUTE21; Togepi without `bike_voucher`; Kabuto with `fossil=HELIX`; Aerodactyl without `old_amber`; a roaming beast other than the one `starter` sets; the other version's Dratini; any target on `navel_rock` or `birth_island`. A listed target never turns its parent option on by itself. |
| R3 | A TID/SID is known before the first target. |
| R4 | `starter_rng_handoff`'s species is `starter`; `fossil_revival.rng`'s is `fossil`'s (or Aerodactyl). |
| P1 | Postgame start (15.01): post-HoF save, 60 caught, segment 11 done. |

## RNG handoffs

Each upstream FRLG RNG program (`SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/RngManipulation/`,
docs `https://pokemonautomation.github.io/Programs/PokemonFRLG/<Program>.html`)
starts from a save the player made in a set spot. A handoff is a split in its
checkpoint: AutoStory sets up that start state, **saves and stops**, the user
runs the RNG program, then AutoStory resumes from the listed checkpoint. All
handoffs are off by default; one turns on only when its target is in the
[RNG plan](#rng-plan-setting) (`rng_plan`, see [RNG options](#rng)). Where the gift or encounter is itself an
option, the handoff is a sub-option of it and needs that option on.

Start states are from each program's docs page. Every resume point is
**unverified**: it assumes the RNG program leaves the player where AutoStory
saved, with the target in the party, and must be checked on the console.

| Target | Program | Checkpoint (sub-option) | Start state the program needs | Split: AutoStory saves and stops | Resume after (unverified) |
|---|---|---|---|---|---|
| Bulbasaur / Charmander / Squirtle | StarterRng | 00.04 (`starter_rng_handoff`, reserved) | South side of the lab table, saved facing the ball of `starter` | Designed in the segment-00 PR | Per that PR |
| Magikarp | GiftRng | 01.01 (`magikarp.rng`) | ¥500 or more; saved facing the Magikarp salesman (Route 4 Center) | Before the purchase | 01.01's heal and save, same spot |
| Eevee | GiftRng | 06.01 (`eevee.rng`) | Saved facing Eevee's ball (Mansion rooftop room) | Before taking the ball | 06.01 from the room: rest of the services |
| Game Corner Abra / Clefairy / Dratini (FR or LG) / Scyther / Pinsir / Porygon | GiftRng | 06.01 (`game_corner_prize.rng`) | Enough coins for the prize; saved facing the prize counter. Prize list depends on version. | Before buying the prize | 06.01 from the prize corner |
| Snorlax | StaticRng | 07.02 EAST or 07.04 WEST (`south_road.rng_snorlax`) | Poke Flute held; saved facing Snorlax | Before playing the flute | The same row after the Snorlax tile is clear |
| Hitmonchan / Hitmonlee | GiftRng | 08.02 (`dojo_gift.rng`) | Karate King beaten; saved facing the chosen ball | After the Karate King battle | 08.02 from the Dojo: leave for the Center |
| Lapras | GiftRng | 08.04 (`lapras_gift.rng`; `lapras_gift` is on in the FireRed Bulbasaur/Charmander plans) | Rival beaten; saved facing the Silph 7F employee | After the rival battle | 08.04 from the employee: on to 08.05 |
| Articuno | StaticRng | 09.02 (`articuno.rng`) | Saved facing Articuno; optionally take overworld items and break smashable rocks first so they add no advances | The row's `STOP_AT` | 09.02 after the encounter: on to Cinnabar |
| Omanyte / Kabuto / Aerodactyl | GiftRng | 10.01 (`fossil_revival.rng`) | Fossil given to the Lab scientist, building left and re-entered, saved next to the scientist. Species comes from 01.02's fossil choice; Aerodactyl needs `old_amber`. | After the hand-over and re-entry | 10.01 from the Lab: on to the Mansion |
| Hypno | StaticRng | 11.03 (`rng_hypno`; story battle, no parent option) | Saved facing Lostelle in Berry Forest | Before talking to Lostelle | 11.03 after the Hypno battle: escort to Two Island |
| Electrode | StaticRng | 12.02 (`zapdos.rng_electrode`; the Power Plant trip) | Saved facing the Electrode | At the Electrode, before Zapdos | 12.02 from the same tile |
| Zapdos | StaticRng | 12.02 (`zapdos.rng`) | Saved facing Zapdos; items and rocks as for Articuno | The row's `STOP_AT` | 12.02 after the encounter: Fly back |
| Moltres | StaticRng | 15.03 (`moltres.rng`) | Saved facing Moltres | The row's `STOP_AT` | 15.03 after the encounter: on to the Ruby |
| Togepi | GiftRng | 16.01 (`water_labyrinth_egg.rng`) | No Repel active; max-friendship lead; on the Bicycle (needs `bike_voucher`); saved facing the old man. "Togepi (pre-approved)" if he already saw the lead but the party was full. | Before talking to the old man | 16.01 from Water Labyrinth |
| Raikou / Entei / Suicune | RoamingLegendaryRng | 16.03 (`roaming_beast.rng`) | Sapphire held and **not yet given**; saved on Celio's left, facing him (machine's left light red, right dark); lead Lv5–49; a Fly user last with Fly as its first field move; a free slot; 3+ Max Repels second in the bag; balls on top. The beast follows `starter` (Bulbasaur: Entei, Charmander: Suicune, Squirtle: Raikou). | Before giving the Sapphire. With this on, 16.03 must not save after the delivery. | 16.03's end (network complete), if the program delivers the Sapphire |
| Mewtwo | StaticRng | 17.01 (`mewtwo.rng`) | Saved facing Mewtwo | The row's `STOP_AT` | 17.01 after the encounter |
| Ho-Oh / Lugia | StaticRng | `navel_rock` | Top of Navel Rock's steps / facing Lugia | — | **Unsupported**: MysticTicket event island |
| Deoxys | StaticRng | `birth_island` | Puzzle solved, saved before the red triangle | — | **Unsupported**: AuroraTicket event island |

Two more programs could hook in later; not designed here:

- **WildRng** (grass, surf, fishing, Rock Smash): the hook is a checkpoint
  whose end tile can spawn the target. Needs a Sweet Scent user in the last
  slot (grass, surf), a rod registered to SELECT and facing water (fishing), or
  a Rock Smash user facing a rock. Natural spots: Tanoby Unown (17.04; slow to
  calibrate per its docs), the Safari Zone (07.06; the program has a Safari
  mode), any route a row already ends on. LeafGreen Staryu (Super Rod,
  Vermilion 07.03 or Pallet 09.01; [team plan](#supporters-by-starter-and-version))
  is a fishing target at a row end.
- **EggRng** (day care eggs): postgame only, at the Four Island Day Care
  (15.05, `four_island_daycare`). Needs both parents deposited with no steps
  taken after, three free slots, a Surf user, ideally Rock Smash. The program
  saves twice by itself, so its resume point needs its own design.

## RNG plan setting

The user decides targets before the run, with an RNG search tool, and fills
one table. AutoStory never picks seeds.

| Field | Scope | Notes |
|---|---|---|
| Game version | shared | FireRed / LeafGreen; must match the game AutoStory runs |
| Language | shared | Picks the seeds list (only English and Japanese lists exist; other languages use English) |
| Sound | shared | Mono / Stereo; the in-game setting must match |
| Switch profile position | shared | The RNG programs' `PROFILE` option |
| TID / SID | shared | From `sid_helper_handoff` or entered by the user; targets are searched with it |
| Target | per row | One of the targets above |
| Target Seed, Advances | per row | From the search tool |
| Target-specific | per row | Starter species, Game Corner prize, fossil: must match `starter`, `game_corner_prize`, `fossil` (check R4) |

How AutoStory uses it:

- A handoff turns on only for a target in the table; nothing else stops.
- At the handoff it reaches the start state, saves, stops, and tells the user
  the exact program and values: program name and docs link, Target, Target
  Seed, Advances, Game Version, Language, Sound, profile position, and the
  checkpoint to start AutoStory from afterwards.
- Later, possibly: call the RNG code inline instead of stopping (the programs
  share calibration and reset routines in `PokemonFRLG_RngLoopRoutines` and
  `PokemonFRLG_HardReset`). Not designed.
- The RNG programs need Text Speed Fast, Button Mode **HELP**, Frame Type 1 and
  the right sound setting. AutoStory must set or check these in its options
  step before the first handoff; check that its own menus work with Button Mode
  HELP.

Checks before the run starts: R1–R4 in [Checks at run start](#checks-at-run-start).

## Item duplication (`item_dupe`, the "mail glitch")

Off by default. It lets the run duplicate held items, mainly Rare Candies (to
level the sweeper instead of grinding) and Nuggets (to sell instead of
looping trainers for money).

Sources: [Game8's guide for the Switch release](https://game8.co/games/Pokemon-FireRed-LeafGreen/archives/583965)
(updated 2026-03-10), [pomeg-letterbombers' mail glitch notes](https://pomeg-letterbombers.github.io/pokemon-ace-notes/mail-glitch),
[Glitch City wiki: Mail Trick glitch](https://glitchcity.wiki/Mail_Trick_glitch),
and upstream's [ItemDuplication](https://pokemonautomation.github.io/Programs/PokemonFRLG/ItemDuplication.html)
program, which automates the duplication loop once the glitch is set up.
Bulbapedia blocked automated reads and was not used. Pokemon, moves and
locations below were checked in the decomp.

**One method exists for FRLG:** Knock Off plus Recycle in a double battle
(found by luckytyphlosion, Nov 2023, per Glitch City). Trick is a Japanese
Ruby/Sapphire method, not FRLG's. Knock Off removes the Mail without freeing
its Mail data slot; afterwards giving Mail to a Pokemon that holds an item
opens a "???" Mail screen, and confirming it adds one more of the held item to
the bag while one Mail is used up.

| Need | Where (decomp) | Story point |
|---|---|---|
| Farfetch'd with Knock Off | Trade a Spearow for CH'DING in Vermilion (`VermilionCity_House2`, `INGAME_TRADE_FARFETCHD`); Knock Off at Lv21. The traded Pokemon gets the level of the one given (`CreateInGameTradePokemonInternal`). Spearow: Route 3 grass, Lv6–8. | 03.02 |
| Mr. Mime with Recycle | Trade an Abra for MIMIEN in the Route 2 house (`Route2_House`, `INGAME_TRADE_MR_MIME`), reached from Route 2's east side; Recycle at Lv33 (Porygon learns it at 44). Abra: Routes 24/25, Lv8–13; it Teleports, so it needs a first-turn catch. | 04.01 trip |
| Obedience | A traded Pokemon above Lv30 disobeys until the Rainbow Badge (`IsMonDisobedient`: 10, 30 Cascade, 50 Rainbow, 70 Marsh). Mr. Mime at 33 needs it. | 06.02 |
| Retro Mail | Celadon Dept. Store 4F, ¥50 each. Five held at once plus one per duplicate; pomeg suggests buying at least 100. | 06.01 |
| A berry | Game8: any berry; pomeg: Sitrus works best (eaten at 50% HP). Found, not sold in Marts. | before 06.07 |
| A double battle | First FRLG double on the route: Twins Eli & Anne, Route 8 (`trainers.inc`). Their Vs. Seeker rematch only opens after Fuchsia (`vs_seeker.c`), so the glitch battle must be their first battle: 05.03 and 06.06 must stay out of their sight. | 06.07 |

Steps, as the sources give them (they differ in party order; both are listed,
the console run picks one):

1. Party: Game8 — Farfetch'd, a berry holder, Mr. Mime holding Retro Mail;
   pomeg — berry holder, Knock Off user, Recycle user holding Mail. Save before
   the battle.
2. Double battle: get the berry holder to half HP so it eats the berry; switch
   the Mail-holding Mr. Mime into that spot; Knock Off it; next turn use
   Recycle (it brings back the berry, not the Mail). Finish the battle.
3. Check: if any Pokemon still holds Mail, reload the save and retry (pomeg).
4. At a Center, fill the party to six, original three in place; give Retro
   Mail (same message as the lost one, Game8) to the five that aren't
   Farfetch'd.
5. Duplicate: give Farfetch'd the item to copy, then give it a Retro Mail; on
   the "???" screen confirm without naming. One copy per Mail. Upstream's
   ItemDuplication automates this step (Farfetch'd first holding the item,
   party full holding the right Mail, Retro Mail at the top of the bag), so
   AutoStory can hand off to it like an RNG handoff.

**Save risk:** neither Game8 nor Glitch City reports corruption from the
Knock Off method itself. But the save stays in a glitched Mail state (pomeg);
in non-Japanese FRLG the "???" Mail writes into PC Box 3, Slot 1 and can
change that Pokemon's substructure order, nature and gender (Glitch City).
AutoStory keeps Box 3, Slot 1 empty for the rest of the run. Pokemon created
from an empty slot by it can vanish in group selections (pomeg). The save
can't be cleanly undone, so this option is the user's call.

**Our version (English FireRed on Switch):** Game8 says it works on the
Switch versions, and upstream ships an ItemDuplication program for the Switch
release, but **neither I nor the lab have confirmed it on our console**, and
pomeg's FAQ warns the Switch rerelease is "heavily internally modified" with
quirks that break some cartridge codes. Japanese FRLG behaves differently; we
run English. Verify once by hand on console B before AutoStory relies on it.

**Placement:** the earliest point is right after the Rainbow Badge with Retro
Mail bought, at Route 8 on the way back to Lavender: new checkpoint `06.07`.
It also needs the Diglett's Cave trip (04.01) for the Route 2 house, a Spearow (01.01) and an Abra (02.04), and two
GRIND steps (Farfetch'd to 21, Mr. Mime to 33; see the team plan).

**Rare Candies for a sweep:** one Candy is one level. The plan raises the
sweeper from about Lv36 at 06.07 (the WR's level at that story point) to
Lv60 there, and to Lv70 at 14.01: **34 Candies**, an upper bound (fight EXP
in between ignored; estimated). Bulbasaur and Charmander add their second
carrier's candies ([team plan](#levels-and-grind-steps)). Add Nuggets for money (see the money plan):
about 8 for the League's Full Restores. That is ~42 duplicates, so ~50 Retro
Mail on top of the five held. Buy 15 at 06.01 (¥750), duplicate and sell
Nuggets first, then buy the rest (up to pomeg's 100). Seeds: keep
one found Rare Candy unused and keep the Route 24 Nugget unsold.

## Team plan

One plan per `starter` × `game_version`. The starter is the main sweeper;
supporters cover the HMs it can't learn and the fights it loses on type.
`supporters=REQUIRED` takes only the HM and Surf users; FULL also takes the
matchup supporters ([Team options](#team)). SQUIRTLE with REQUIRED is the WR's
team: one sweeper plus HM users, no grinding (see the
[speedrun notes](AutoStory-FRLG-SpeedrunNotes.md)).

Sources, all pret/pokefirered: parties and levels `src/data/trainer_parties.h`
(`sParty_Leader*`, `sParty_EliteFour*`, `sParty_Rival*`, `sParty_Champion*`);
level-up moves `src/data/pokemon/level_up_learnsets.h`; TM/HM compatibility
`src/data/pokemon/tmhm_learnsets.h`; evolutions `src/data/pokemon/evolution.h`;
wild slots `src/data/wild_encounters.json` (land slot rates
20/20/10/10/10/10/5/5/4/4/1/1, Super Rod slots 40/40/15/4/1); shop
`data/maps/CeladonCity_DepartmentStore_4F/scripts.inc`; prices
`src/data/items.json`; trades `ingame_trades.h`. Type effectiveness is the
Generation III chart, not re-read from the decomp. Rates below are per
encounter of that kind, from the slot tables.

### The rival follows the starter

The rival takes the starter strong against the player's: Bulbasaur → rival
Charmander, Charmander → rival Squirtle, Squirtle → rival Bulbasaur. Its
parties are named by its own starter (Squirtle player: `_BULBASAUR`). Levels
are the same in all three, so the [level table](#levels-and-grind-steps) holds
for every starter. The Champion's ace is Lv63: Charizard against Bulbasaur,
Blastoise against Charmander, Venusaur against Squirtle; the rest is Pidgeot
59, Alakazam 57, Rhydon 59 and two of Gyarados, Arcanine, Exeggutor.

### Matchups by fight

"Good": the starter's moves hit super effectively, or it resists the
opponent. "Problem": its moves are resisted, or it is hit super effectively.
"Even": neither.

| Fight | Opponent (decomp) | Bulbasaur → Venusaur | Charmander → Charizard | Squirtle → Blastoise |
|---|---|---|---|---|
| Brock 00.09 | Geodude 12, Onix 14 (Rock/Ground) | Good: Vine Whip at 10 | **Problem**: Ember resisted; Metal Claw at 13 is its only answer | Good: Bubble, Water Gun at 13 |
| Misty 02.01 | Staryu 18, Starmie 21 (Water; Starmie Water/Psychic) | Good | **Problem**: Fire resisted | Even: Bite on Starmie (WR) |
| Surge 03.06 | Voltorb 21, Pikachu 18, Raichu 24 (Electric) | Good: resists | Even | **Problem**: weak (WR wins at 28 with TM03 Water Pulse and Mega Kick) |
| Erika 06.02 | Victreebel 29, Tangela 24, Vileplume 29 (Grass, Grass/Poison) | **Problem** | Good | **Problem** |
| Koga 07.05 | Koffing 37, Muk 39, Koffing 37, Weezing 43 (Poison; Koffing and Weezing Levitate) | **Problem**: Grass resisted | Even | Even |
| Sabrina 08.06 | Kadabra 38, Mr. Mime 37, Venomoth 38, Alakazam 43 (Psychic) | **Problem**: Psychic hits Poison | Even | Even |
| Blaine 10.02 | Growlithe 42, Ponyta 40, Rapidash 42, Arcanine 47 (Fire) | **Problem** | Even | Good |
| Giovanni 12.03 | Rhyhorn 45, Dugtrio 42, Nidoqueen 44, Nidoking 45, Rhyhorn 50 (Ground) | Good | **Problem** | Good |
| Lorelei 14.02 | Dewgong 52, Cloyster 51, Slowbro 52, Jynx 54, Lapras 54 (Water/Ice) | **Problem**: Ice | **Problem**: Water resists Fire | Even |
| Bruno 14.03 | Onix 51, Hitmonchan 53, Hitmonlee 53, Onix 54, Machamp 56 | Even (good on Onix) | Even (Charizard's Wing Attack on the fighters) | Good on Onix |
| Agatha 14.04 | Gengar 54, Golbat 54, Haunter 53, Arbok 56, Gengar 58 (Ghost/Poison) | **Problem**: Grass resisted by all | Even | Even |
| Lance 14.05 | Gyarados 56, Dragonair 54 ×2, Aerodactyl 58, Dragonite 60 | **Problem**: no Ice move | **Problem**: no Ice move | Good with TM14 Blizzard (WR) or TM13 |
| Champion 14.06 | see above | **Problem**: Charizard 63 | **Problem**: Blastoise 63 | **Problem**: Venusaur 63 (WR wins) |

Bulbasaur's problems are late (Erika onward); Charmander's are early (Brock,
Misty) and at Giovanni and Lorelei; Squirtle's are Surge and Erika.

### HM users

| HM | Bulbasaur | Charmander | Squirtle |
|---|---|---|---|
| Cut (03.04) | Bulbasaur | Charmander | Rattata (WR, Route 1) or Farfetch'd (`item_dupe`) |
| Flash (`flash`) | Bulbasaur | Pikachu (FULL); LG: Starmie | LG: Starmie; else Mr. Mime (`item_dupe`) |
| Surf (first use 09.01) | FR: Lapras; LG: Starmie | FR: Lapras; LG: Starmie | Squirtle |
| Strength | Bulbasaur | Charmander | Squirtle |
| Fly (`fly`) | Pidgey (Route 1/2) | Charizard (from Lv36) | Pidgey (WR) or Farfetch'd |
| Rock Smash / Waterfall (postgame) | Bulbasaur / the Surf user | Charmander / the Surf user | Squirtle |

Rattata, Pidgey and Blastoise can't learn Flash; Bulbasaur and Charmander
can't learn Surf. Fallback Surf users in both versions: Vaporeon (`eevee`, Lv25
at 06.01, Water Stone sold in the same building) and Gyarados (`magikarp`,
evolves at 20). Lapras is a gift in both versions; LeafGreen prefers Starmie
because it also covers Koga, Agatha and Flash.

### Supporters by starter and version

"Req." is REQUIRED (always taken); "FULL" only with `supporters=FULL`.

| Starter | Version | Supporter | Fixes | Where and when (decomp) | Then | Taken |
|---|---|---|---|---|---|---|
| Bulbasaur | both | Pidgey → Pidgeotto | Fly; Erika (Wing Attack at 27) | Route 1/2 grass, Lv2–5, 00.05–00.07 | — | Req. with `fly`, else FULL |
| Bulbasaur | FR | Lapras | Surf; Blaine; Lance (Ice Beam at 31); Champion's Charizard | Silph 7F gift, Lv25, 08.04 (`lapras_gift`) | `TEACH(Surf)` before 09.01 | Req. |
| Bulbasaur | FR | Growlithe → Arcanine (FR exclusive) | Erika; Koga (strong, even); Blaine with TM28 Dig; Lorelei's Jynx and Cloyster | Route 8 (20%, Lv15–18) or Route 7 (10%, Lv18–20), 05.03 | Fire Stone ¥2,100 at 06.01; TM28 (02.07) | FULL |
| Bulbasaur | LG | Staryu → Starmie (LG exclusive) | Surf, Flash; Koga and Agatha with TM29 Psychic; Blaine; Champion's Charizard | Super Rod only: Vermilion, Pallet, Cinnabar, S.S. Anne exterior (40%, Lv15–25); 07.03 detour, see below | Water Stone ¥2,100 at 06.01; TM29 from Mr. Psychic, Saffron (06.06) | Req. |
| Bulbasaur | LG | Vulpix → Ninetales (LG exclusive) | Erika; Blaine with TM28 Dig | as Growlithe | Fire Stone ¥2,100; TM28 | FULL |
| Charmander | both | Mankey | Brock: Low Kick at 6 (power by weight, high on Onix) | Route 22 grass (45%, Lv2–5): a west detour in 00.07 | — | FULL (fallback: Metal Claw at 13) |
| Charmander | both | Pikachu → Raichu | Misty (Thunder Shock, Thunder Wave at 8); rival's Squirtle line; Lorelei's Water types; Lance's Gyarados | Viridian Forest (5%, Lv3–5), 00.08, bounded encounters | Thunderbolt at 26; Thunder Stone ¥2,100 at 06.01 | FULL |
| Charmander | FR | Lapras | Surf; Giovanni; Lorelei; Lance (Ice Beam) | 08.04 (`lapras_gift`) | `TEACH(Surf)` before 09.01 | Req. |
| Charmander | LG | Staryu → Starmie | Surf, Flash; Giovanni; Lorelei with TM24 (`prize_tm`) | as for Bulbasaur | as for Bulbasaur | Req. |
| Squirtle | both | Rattata, Pidgey → Pidgeotto | Cut, Fly (WR); Pidgeotto for Erika | Rattata: Route 1 (WR); Pidgey: Route 1/2 (50%/45%, Lv2–5) | — | Req. (Pidgey with `fly`) |
| Squirtle | both | — (TM28 Dig on Squirtle, from 02.07) | Surge: Ground hits all three; none Levitate | — | `TEACH(TM28)` before 03.06, or the WR's Water Pulse + Mega Kick | FULL |
| Squirtle | FR | Growlithe → Arcanine | Erika; Champion's Venusaur | as above | Fire Stone | FULL |
| Squirtle | FR | Lapras | Second Ice user; TM24/TM29 carrier | 08.04 (`lapras_gift`) | — | optional |
| Squirtle | LG | Staryu → Starmie | Koga, Agatha and the Champion's Venusaur with TM29 Psychic; Flash | as for Bulbasaur | Water Stone; TM29 | FULL |
| Squirtle | LG | Vulpix → Ninetales | Erika | as above | Fire Stone | FULL |

Alternatives considered and not planned: Oddish (FR, Route 24/25) only has
Absorb before Lv14, too weak for Misty; Bellsprout (LG, Route 24 25%, Vine Whip
at Lv1) fixes Charmander's Misty but needs Misty moved after Bill (a schedule
change); Butterfree's Confusion is resisted by Starmie; Diglett (Diglett's
Cave, 95%) is reached in 04.01, after Surge in our order, and Koga's Levitate
users are immune to Ground.

**LeafGreen Staryu.** It is only on the Super Rod (decomp: fishing slot 6 in
seven LeafGreen maps; no grass, surf, gift or trade), so it can't join before
07.03. Two placements:

- 07.03 (EAST, proposed): take the Super Rod (`super_rod` on), walk north to
  the Route 11 gate beside the Snorlax tile and west along Route 11 (72 tiles
  wide) to Vermilion, fish with a bounded bite count, come back. About 90
  tiles each way (estimated from the map sizes). Starmie then trains from Koga
  on.
- 09.01 (no detour): fish at Pallet before surfing south; the row already
  stands there. Starmie is Lv15–25 against Blaine's 47, so this needs
  `item_dupe` Rare Candies or a long GRIND. Fallback for WEST.

Buy the Water Stone at 06.01 either way and evolve on capture: Starmie
learns no level-up moves after Lv1 except Confuse Ray at 33, so evolving at
once only loses Staryu's later moves (Bubble Beam 28), which TMs replace.

### Catches, purchases and teaching added to rows

| Row | Step | When |
|---|---|---|
| 00.05–00.07 | `CAPTURE(rattata)` (Squirtle); `CAPTURE(pidgey)` (Bulbasaur, Squirtle) | per the table above |
| 00.07 | West detour to Route 22 grass; `CAPTURE(mankey)` | Charmander, FULL |
| 00.08 | `CAPTURE(pikachu)` in Viridian Forest, bounded | Charmander, FULL |
| 02.07 → 03.06 | `TEACH(TM28, squirtle)` | Squirtle, FULL |
| 05.03 | `CAPTURE(growlithe)` (FR) / `CAPTURE(vulpix)` (LG) on Route 8 | Bulbasaur FULL, Squirtle FULL |
| 06.01 | `ITEM`: Fire / Thunder / Water Stone, ¥2,100 each (Dept. Store 4F); evolve | per plan |
| 06.06 | Saffron: Mr. Psychic's house, TM29 (`SaffronCity_MrPsychicsHouse`) | LG Starmie |
| 07.03 | LG: Super Rod; Vermilion detour; `CAPTURE(staryu)`; evolve | LG, Bulbasaur/Charmander Req., Squirtle FULL |
| 07.07 | `TEACH(Surf)` to the Surf user (FR Bulbasaur/Charmander: after 08.04) | all |
| 08.04 | `lapras_gift` on | FR Bulbasaur/Charmander |
| 09.01 | Fallback `CAPTURE(staryu)` at Pallet before surfing | LG, WEST road |

In-game trades (all in `ingame_trades.h`; the received Pokemon has the level
of the one given). None is needed by a starter plan; the first two serve
`item_dupe`.

| You give | You get | Where | Plan use |
|---|---|---|---|
| Abra | Mr. Mime MIMIEN | Route 2 house | `item_dupe`; Flash for Squirtle |
| Spearow | Farfetch'd CH'DING (Stick) | Vermilion house | `item_dupe`, Cut/Fly |
| Poliwhirl | Jynx ZYNX (Fab Mail) | Cerulean house | — (pomeg mentions it can affect the glitch state; not used) |
| Nidoran (one sex) | Nidoran (other) MS./MR. NIDO | Underground Path north entrance (Route 5) | — |
| Nidorino / Nidorina | NINA / NINO | Route 11 gate 2F | — |
| Golduck (FR) / Slowbro (LG) | Lickitung MARC | Route 18 gate 2F | — |
| Raichu | Electrode ESPHERE | Cinnabar Lab lounge | — |
| Venonat | Tangela TANGENY (Stardust) | Cinnabar Lab lounge | — |
| Ponyta | Seel SEELOR | Cinnabar Lab experiment room | — |

### Levels and GRIND steps

Levels per major fight: opponent's highest level (decomp, the same for every
starter), the WR sweeper's level there (speedrun notes), and a DEFAULT target
(estimated: WR level, at least the opponent's ace + 2 for gyms and rivals).
Our order differs from the WR's after Celadon (it did Koga and Blaine before
Erika), so later WR levels are a guide, not a curve to copy. A supporter that
takes a fight is held to the same target for it.

| Fight | Checkpoint | Opponent max | WR level | DEFAULT target |
|---|---|---|---|---|
| Brock | 00.09 | 14 | 11 | 14 |
| Miguel | 01.02 | 12 | ~16 | 16 |
| Misty | 02.01 | 21 | 20–21 | 23 |
| Rival, Cerulean | 02.02 | 18 | 21 | 21 |
| Rival, S.S. Anne | 03.03 | 20 | ~27 | 27 |
| Surge | 03.06 | 24 | 28 | 28 |
| Erika | 06.02 | 29 | 44 (fought late) | 32 |
| Giovanni, Hideout | 06.05 | 29 | 36 | 36 |
| Rival, Tower | 06.08 | 25 | 36–38 | 36 |
| Koga | 07.05 | 43 | 40 | 45 |
| Rival, Silph | 08.04 | 40 | 44–45 | 45 |
| Giovanni, Silph | 08.05 | 41 | 45–46 | 45 |
| Sabrina | 08.06 | 43 | 47 | 47 |
| Blaine | 10.02 | 47 | 42 | 49 |
| Giovanni, Gym | 12.03 | 50 | 49 | 52 |
| Rival, Route 22 | 13.01 | 53 | 49 | 53 |
| Elite Four | 14.02–14.05 | 60 (Dragonite) | 51–55 | 58 |
| Champion | 14.06 | 63 | 55–57 | 60 |

The WR is under target at Koga, Blaine and the League and wins with X items
(X Special, X Speed, X Accuracy) and Full Restores; DEFAULT buys those too.

**Second carrier (Bulbasaur, Charmander).** Their late problems need the Surf
user (Lapras Lv25 at 08.04, or Starmie Lv15–25 at 07.03) to carry fights from
Blaine (Bulbasaur) or Giovanni (Charmander) on: roughly +25 levels on a second
Pokemon by 10.02/12.03, and to 58 by the League (estimated). That is the main
cost of these starters. With `item_dupe`, add about 25–35 Rare Candies to
`item_dupe.rare_candies` (estimated); without it, add GRINDs below.

**GRIND steps (no `item_dupe`).** `GRIND(who, where, level)`: fight until the
named Pokemon reaches the level, with a bounded encounter count, then heal.
Proposed, all estimated until runs measure EXP:

| Before | GRIND | Starters | Notes |
|---|---|---|---|
| 00.09 Brock | Mankey to ~10 on Route 22, or Charmander to 13 (Metal Claw) | Charmander | |
| 02.01 Misty | Route 24/25 trainers, first pass (02.02–02.04 are on the way) | all; Charmander: Pikachu to ~16 | Fight Misty after Bill if below 23 (schedule change) |
| 03.06 Surge | Vermilion: Route 6/11 trainers | all | |
| 06.02 Erika | the fire or flying supporter to 32 | Bulbasaur, Squirtle (FULL) | Celadon is where the stones are bought |
| 07.05 Koga | Route 12–15 trainers (EAST road) | all; LG: Starmie from 07.03 | Most of the gap closes on the road itself |
| 10.02 Blaine | Cinnabar Mansion trainers / Route 21 | all; Bulbasaur: the Surf user to 49 | |
| 12.03 Giovanni | Route 21 / Seafoam surf encounters | Charmander: the Surf user to 52 | |
| 14.01 League | Victory Road and Route 23 wild Pokemon | all; both carriers to 58 | Biggest gap: 55→58 |

With `item_dupe`, every GRIND for the sweeper after `06.07` is replaced by a
Rare Candy step (`ITEM(rare_candy, n)` to the table's target). The two
glitch-setup GRINDs stay: Farfetch'd to 21 and Mr. Mime to 33 (both on
Route 24/25 trainers or the `nugget_bridge_loop`, which also pays).

## Money plan

Worked backwards from what the run buys. Every number is marked: **(d)**
decomp, **(w)** speedrun notes, **(e)** estimated. Prices are `items.json`
(d); selling gives half the price (`item_menu.c`) (d).

### Sinks

| Checkpoint | What | Cost |
|---|---|---|
| 00.07 | 5 Potions, 3 Antidotes, 1 Parlyz Heal (the existing `shop` row) | ¥2,000 (d: table) |
| 01.01 `magikarp` | Magikarp | ¥500 (GiftRng docs) |
| 02.06 | Cerulean restock, ~5 Potions + 2 Repels | ¥2,200 (e) |
| 03.01 | 5 Super Potions, 3 Parlyz Heals, 3 Repels (WR's buy) | ¥5,150 (d prices, w quantities) |
| 05.01 | 4 Escape Ropes, 7 Super Repels (WR's buy) | ¥5,700 (d, w) |
| 06.01 | 6 X Speed, 21 X Special, 5 X Accuracy, 2 Guard Spec (WR's buy) | ¥15,600 (d, w) |
| 06.01 `item_dupe` | 15 Retro Mail now, ~85 more after the first Nugget sales | ¥750 + ¥4,250 (d) |
| 06.01 `prize_tm` | Game Corner TM: TM13 Ice Beam 4000 coins, TM24 Thunderbolt 4000, TM35 Flamethrower 4000, TM23 Iron Tail 3500, TM30 Shadow Ball 4500; coins ¥1,000 per 50 (¥20 each) | Ice Beam = ¥80,000 (d) |
| 06.01 `game_corner_prize.rng` | Prize Pokemon, FR: Abra 180, Clefairy 500, Dratini 2800, Scyther 5500, Porygon 9999 coins; LG: Abra 120, Clefairy 750, Pinsir 2500, Dratini 4600, Porygon 6500 | FR Dratini = ¥56,000 (d) |
| 07.06 | Safari Zone entry | ¥500 (d) |
| 06.01 team plan | Evolution stones, ¥2,100 each (d), and about 3 Poke Balls (¥200, d) per planned capture (e); per plan below | ¥0–6,000 |
| 14.01 | ~13 Full Restores (WR bought about 13) | ¥39,000 (d price, w count) |
| — | Bicycle: voucher, free | ¥0 |

The WR gets Blizzard free (TM14, Pokemon Mansion B1F item ball (d)), so
`prize_tm` is off by default.

Team-plan purchases by `starter` × `game_version` (stones (d), balls (e)):

| Starter | FR, REQUIRED | FR, FULL | LG, REQUIRED | LG, FULL |
|---|---|---|---|---|
| Bulbasaur | ¥0 (¥600 with `fly`) | ¥3,300: Pidgey, Growlithe, Fire Stone | ¥2,700: Staryu, Water Stone | ¥6,000: + Pidgey, Vulpix, Fire Stone |
| Charmander | ¥0 | ¥3,300: Mankey, Pikachu, Thunder Stone | ¥2,700: Staryu, Water Stone | ¥6,000: + Mankey, Pikachu, Thunder Stone |
| Squirtle | ¥0 (WR) | ¥2,700: Growlithe, Fire Stone | ¥0 | ¥5,400: Staryu, Vulpix, Water and Fire Stones |

### Sources

- Trainer payouts: `4 × last Pokemon's level × class base × 2 if double ×
  2 with Amulet Coin` (`battle_script_commands.c`) (d). The table below sums
  them per segment from `trainer_parties.h`, `trainers.h` and the trainers'
  map scripts (d). "Required" is leaders, rivals, bosses, Miguel and the
  Nugget Bridge (forced on a one-tile bridge). "Estimate" adds half of the
  other trainers on those maps (e). The WR's money readings match the
  decomp's prizes (Brock 1,400, Misty 2,100, Silph rival 1,440, Lorelei 5,400).
- Nugget from the Route 24 Rocket: sells for ¥5,000 (d). Other Nuggets on the
  default path (d): Rocket Hideout B3F (hidden), Pokemon Tower 5F, Safari
  Center, Silph 8F (hidden), each ¥5,000 if picked up (not counted below).
- Amulet Coin (`amulet_coin`, Route 16 gate, 40 caught): doubles prizes when
  its holder is sent into the battle (`battle_util.c`) (d).
- Vs. Seeker rematches (`vs_seeker`, Vermilion Center) (d, `vs_seeker.c`):
  the battery needs 100 steps after each successful use; each fought trainer
  on screen accepts with 70% chance; the rematch party is the highest tier the
  story allows: tier 1 after getting the Vs. Seeker, 2 after visiting Celadon,
  3 after Fuchsia, 4 after the Hall of Fame. Routes 24/25 have 15 rematchable
  trainers; Hiker Franklin only from tier 2.

| Nugget Bridge loop (Routes 24+25, all rematchable trainers) | Sum of prizes (d) | Expected at 70% (d) | With Amulet Coin |
|---|---|---|---|
| Tier 1 (Vs. Seeker, before Celadon) | ¥4,396 | ¥3,077 | — (coin comes later) |
| Tier 2 (after Celadon) | ¥5,296 | ¥3,707 | ¥7,414 |
| Tier 3 (after Fuchsia) | ¥5,840 | ¥4,088 | ¥8,176 |

One loop needs several Vs. Seeker uses (trainers must be on screen), each
after 100 steps: about 5 uses per loop (e).

### Ledger (DEFAULT: EAST road, Route 21, no options)

Start money ¥3,000 (w: Trainer Card at 0:00). "Floor" counts only required
fights (d) plus the bridge Nugget sold; "estimate" adds half the other
trainers (e).

| Segment | Income floor | Income est. | Spend | Cumulative floor | Cumulative est. |
|---|---|---|---|---|---|
| start | | | | 3,000 | 3,000 |
| 00 | 1,480 | 1,764 | 2,000 | 2,480 | 2,764 |
| 01 | 288 | 2,578 | 0 | 2,768 | 5,342 |
| 02 (+ Nugget 5,000) | 9,564 | 11,498 | 2,200 | 10,132 | 14,640 |
| 03 | 3,120 | 11,572 | 5,150 | 8,102 | 21,062 |
| 04 | 0 | 1,932 | 0 | 8,102 | 22,994 |
| 05 | 0 | 10,820 | 5,700 | 2,402 | 28,114 |
| 06 | 6,700 | 20,124 | 15,600 | **−6,498** | 32,638 |
| 07 | 4,300 | 24,856 | 500 | −2,698 | 56,994 |
| 08 | 5,740 | 24,864 | 0 | 3,042 | 81,858 |
| 09 | 0 | 2,544 | 0 | 3,042 | 84,402 |
| 10 | 4,700 | 18,732 | 0 | 7,742 | 103,134 |
| 12 | 5,000 | 10,180 | 0 | 12,742 | 113,314 |
| 13 | 1,908 | 11,678 | 0 | 14,650 | 124,992 |
| 14.01 | 0 | 0 | 39,000 | **−24,350** | 85,992 |

The estimate is high against the WR (¥15,694 at Celadon, ¥41,818 at the
Plateau, both read on screen), which skipped most trainers; reality sits
between floor and estimate.

**Loops needed** (passes at tier 1, ¥3,077 expected each, run in 04.02):

| Plan | Deficit (floor) | Loops (floor) | Loops (estimate) |
|---|---|---|---|
| No dupe | ¥24,350 by 14.01 (¥6,498 by 06.01) | 8 | 0 |
| No dupe, costliest team plan (LG Bulbasaur/Charmander FULL, +¥6,000) | ¥30,350 by 14.01 (¥12,498 by 06.01) | 10 | 0 |
| No dupe + `prize_tm` Ice Beam | ¥104,350 | 34 (or 15 at tier 2 with Amulet Coin) | 0 |
| `item_dupe` (skip X items; keep the bridge Nugget as the seed; 15 Retro Mail) | ¥3,348 at 06.01 | 2 | 0 |
| `item_dupe` after 06.07 | covered by duplicated Nuggets (¥5,000 each, 8 for the League) | 0 | 0 |

`nugget_bridge_loop` ([option](#economy-and-grinding)) reads this ledger:
AUTO's N is the floor deficit up to the next money check divided by the
expected loop payout, rounded up.
Every loop count is an estimate until a console run measures payouts per
loop and steps per Vs. Seeker use.


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
| 00.04 | Squirtle, then the rival's battle | LAB (6,4) N | Pick `starter` (today's table: Squirtle); rival picks the counter; tutorial battle (`may_lose`) `[opt: starter_rng_handoff]` | LAB (7,8) S | 开局3, 开局4, 开局5 |
| 00.05 | Route 1 to Viridian Mart | LAB (7,8) S | Route 1 north; Mart clerk gives Oak's Parcel | VIRIDIAN_MART (4,3) W | 开局6, 开局7 |
| 00.06 | Parcel to the Pokedex | VIRIDIAN_MART (4,3) W | Back to Pallet; deliver Parcel; Pokedex and balls `[opt: town_map — rival's house, Daisy]` | LAB (6,4) S | 开局8, 开局9, *开局10* |
| 00.07 | Viridian shopping, into the forest | LAB (6,4) S | Route 1 north; shop; old man; Route 2 west into Viridian Forest `[opt: route22_rival_early — Route 22 battle before Route 2]` `[team: Route 22 CAPTURE(mankey)]` | FOREST (29,62) N | 开局11, *开局12*, 开局13 |
| 00.08 | Forest to Pewter Center | FOREST (29,62) N | Forest trainers in path `[team: CAPTURE(pikachu), bounded]`; Pewter Center heal | PEWTER_CENTER (7,4) N | 开局13 |
| 00.09 | Camper Liam, then Brock | PEWTER_CENTER (7,4) N | Gym; Liam; Brock → Boulder Badge | GYM (6,6) N | 开局14 |
| 00.10 | The Running Shoes | GYM (6,6) N | Route 3 entrance trigger; Oak's aide gives Running Shoes | PEWTER (46,21) N | 开局15 |
| 00.11 | A run to a wall | PEWTER (46,21) N | Timing run for running calibration (lab row) | — | — |

### Segment 01 — Route 3, Mt. Moon, Route 4 (Part 4)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 01.01 | Route 3 to the Mt. Moon Center | PEWTER (46,21) N | `ready`; Route 3 trainers on the chosen path; Route 4 Center `heal` `[opt: magikarp]` `[opt: item_dupe — CAPTURE(spearow)]` | `@Route4.center_counter` | 开局16, *开局17* |
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
| 02.04 | Route 25 to Sea Cottage | `@Route24.bridge_north` | Route 24 grass and Route 25 trainers on the chosen path; `[opt: item_dupe — CAPTURE(abra), first-turn catch]`; enter the cottage | `@SeaCottage.door` | 开局23 |
| 02.05 | Restore Bill | `@SeaCottage.door` | `talk(Bill)`; `answer(help, YES)`; PC; separation `scene`; `talk(Bill)`; `VERIFY(SS_Ticket)` | `@SeaCottage.door` | 开局24 |
| 02.06 | Back to Cerulean | `@SeaCottage.door` | Routes 25 and 24 south; Center `heal` | `@Cerulean.center_counter` | 开局25 |
| 02.07 | The burgled house | `@Cerulean.center_counter` | Officer has moved (after the ticket); through the house to the back yard; `battle(rocket_thief)`; TM28 | `@Cerulean.house_backyard` | 中局1 |
| 02.08 | Route 5 and the Underground Path | `@Cerulean.house_backyard` | Route 5 south (the Saffron gate guard turns the player back; the route does not touch it); Underground Path to Route 6 | `@Route6.underground_exit` | 中局2, 中局3 |

### Segment 03 — Route 6, Vermilion, S.S. Anne, Vermilion Gym (Part 6)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 03.01 | Route 6 to Vermilion | `@Route6.underground_exit` | Route 6 trainers on the path; Vermilion Center `heal` | `@Vermilion.center_counter` | 中局3 |
| 03.02 | To the harbor | `@Vermilion.center_counter` | `[opt: vs_seeker]` girl in the Center; `[opt: bike_voucher]` Fan Club chairman; `[opt: old_rod]` Fishing Guru; `[opt: item_dupe]` trade Spearow for Farfetch'd (Vermilion house), `GRIND(farfetchd, 21)` for Knock Off; harbor ticket check; board | `@SSAnne.1F_entry` | *中局4*, *中局5*, *中局6*, 中局7, 中局8 |
| 03.03 | Ship rival | `@SSAnne.1F_entry` | Route to the captain's deck; `PREP(rival_ship)`; `battle(rival_ss_anne)` | `@SSAnne.captain_hall` | 中局9 |
| 03.04 | Captain's Cut | `@SSAnne.captain_hall` | `talk(captain)`; back-rub `scene`; HM01; leave; ship departs; `PARTY(cut_user)`; `TEACH(Cut, slot)`. Cut user per the [team plan](#hm-users): Bulbasaur or Charmander itself; Squirtle can't learn Cut (WR: Rattata from Route 1). | `@Vermilion.center_counter` | 中局10 |
| 03.05 | Surge's switches | `@Vermilion.center_counter` | `FIELD(Cut, gym_tree)`; gym trainers on the path; `PUZZLE(two_trash_switches)` read from the screen, bounded attempts | `@VermilionGym.surge_front` | 中局11 |
| 03.06 | Thunder Badge | `@VermilionGym.surge_front` | `PREP(Surge)` (needs a Ground or other validated plan); `battle(Surge)`; Thunder Badge, TM34; Center `heal` | `@Vermilion.center_counter` | 中局11 |

### Segment 04 — Route 11, Diglett's Cave, Route 2 east, Museum, Routes 9/10 (Part 7)

Default skips Flash and walks Rock Tunnel dark, as the WR does (45:00–50:00 in the [speedrun notes](AutoStory-FRLG-SpeedrunNotes.md)).

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 04.01 | `[opt: flash, old_amber or item_dupe]` Diglett's Cave to HM05 | `@Vermilion.center_counter` | Route 11; Diglett's Cave; `[opt: flash]` Route 2 east gate aide (needs 10 caught: `CAPTURE` until met), `TEACH(Flash)`; `[opt: old_amber]` museum side door via Cut; `[opt: item_dupe]` Route 2 house: trade Abra for Mr. Mime; back through the cave | `@Vermilion.center_counter` | *中局12* |
| 04.02 | Back to Cerulean | `@Vermilion.center_counter` | Route 6; Underground Path; Route 5; `[opt: bike_voucher]` Bike Shop exchange; `[opt: nugget_bridge_loop]` N Vs. Seeker loops of Routes 24/25 (needs `vs_seeker`), also `GRIND(mr_mime, 33)` with `item_dupe`; Center `heal` | `@Cerulean.center_counter` | 中局13 |
| 04.03 | Routes 9 and 10 | `@Cerulean.center_counter` | Cut the east tree; Route 9 trainers on the path; Route 10 Center `heal` | `@Route10.center_counter` | 中局13 |

### Segment 05 — Rock Tunnel, Route 10 south, Lavender, Route 8, Underground Path, Route 7 (Part 8)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 05.01 | Rock Tunnel to Lavender | `@Route10.center_counter` | Rock Tunnel floors and ladders, dark (or `FIELD(Flash)` with `[opt: flash]`); path trainers; Route 10 south; Lavender Center `heal`. Hears Fuji is missing; does not enter the Tower. | `@Lavender.center_counter` | 中局13, 中局14 |
| 05.02 | `[opt: early_tower]` Tower 2F rival | `@Lavender.center_counter` | Enter the Tower; 2F rival trigger (decomp `PokemonTower_2F` coord events (17,5), (16,6)); `battle(rival_tower)`; 3F ghosts can't be fought without the Scope: flee | `@Lavender.center_counter` | 中局14, *中局15*, *中局16* |
| 05.03 | West to Celadon | `@Lavender.center_counter` | Route 8 (with `item_dupe`, out of Twins Eli & Anne's sight) `[team: CAPTURE(growlithe \| vulpix)]`; Underground Path (7–8); Route 7; Celadon Center. No Saffron gate before the Tea. | `@Celadon.center_counter` | 中局17 |

### Segment 06 — Celadon, Celadon Gym, Rocket Hideout, Pokemon Tower (Part 9)

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 06.01 | Celadon services | `@Celadon.center_counter` | Mansion 1F old lady: Tea; Dept. Store by budget `[opt: coin_case]` `[opt: game_corner_prize]` `[opt: eevee]` `[opt: prize_tm]` `[opt: item_dupe — 15 Retro Mail]` `[team: evolution stones, evolve]` | `@Celadon.center_counter` | *中局19*, *中局23*, 中局24 |
| 06.02 | Rainbow Badge | `@Celadon.center_counter` | Cut the gym tree; gym trainers on the path; `PREP(Erika)`; `battle(Erika)`; Rainbow Badge, TM19 | `@Celadon.center_counter` | 中局18 |
| 06.03 | Hideout entrance | `@Celadon.center_counter` | Game Corner; `battle(poster_grunt)`; press the poster switch; stairs | `@RocketHideoutB1F.stairs` | 中局20 |
| 06.04 | Lift Key | `@RocketHideoutB1F.stairs` | Spinner floors to B4F northwest room; `battle(key_grunt)`; **talk again**; pick up the Lift Key | `@RocketHideoutB4F.key_room` | 中局21 |
| 06.05 | Hideout Giovanni | `@RocketHideoutB4F.key_room` | Lift to B4F; door guards; `PREP(giovanni_hideout)`; `battle(giovanni_hideout)`; Silph Scope | `@Celadon.center_counter` | 中局22 |
| 06.06 | Tea and back to Lavender | `@Celadon.center_counter` | Route 7 gate: guard takes the Tea; through Saffron `[team: TM29 from Mr. Psychic]`; Route 8 gate (with `item_dupe`, still out of the Twins' sight); Lavender Center | `@Lavender.center_counter` | 中局25, 中局26 |
| 06.07 | `[opt: item_dupe]` The mail glitch | `@Lavender.center_counter` | Party: berry holder, Farfetch'd (Knock Off), Mr. Mime (Recycle, holding Retro Mail); to Route 8; `battle(twins_eli_anne)` as a double: berry eaten, switch Mr. Mime in, Knock Off it, Recycle; check no one holds Mail (else reload); Center: fill party, Mail on five; hand off to ItemDuplication (or duplicate in-table): Rare Candies and Nuggets per the plans; sell Nuggets; `ITEM(rare_candy)` to the team plan's target. Box 3 Slot 1 stays empty from here. | `@Lavender.center_counter` | — |
| 06.08 | Tower rival and ascent | `@Lavender.center_counter` | Tower; 2F rival (skipped if `early_tower` won it); 3F–5F with the healing zone | `@PokemonTower5F.stairs_up` | 中局15 (if not early) |
| 06.09 | Marowak | `@PokemonTower5F.stairs_up` | 6F: Scope reveals Marowak; `battle(marowak_ghost)` — can't be caught | `@PokemonTower6F.stairs_up` | 中局27 |
| 06.10 | Fuji's rescue | `@PokemonTower6F.stairs_up` | 7F: three Rockets; Fuji; escort home | `@FujiHouse.fuji_front` | 中局28 |
| 06.11 | Poke Flute | `@FujiHouse.fuji_front` | `talk(Fuji)`; Poke Flute; Center `heal` | `@Lavender.center_counter` | 中局29 |

### Segment 07 — Routes 12–18, Fuchsia, Fuchsia Gym, Safari Zone (Part 10)

`south_road` picks one Snorlax; the other road is never run.

| ID | Title | Start | Does | End / save | 52poke |
|---|---|---|---|---|---|
| 07.01 | `[opt: fly]` HM02 Fly | `@Lavender.center_counter` | Through Saffron to Celadon; Route 16 north via Cut (no Snorlax); house: HM02; `TEACH(Fly)`; `FIELD(Fly, Lavender)` | `@Lavender.center_counter` | *中局37* |
| 07.02 | EAST: Route 12 Snorlax | `@Lavender.center_counter` | Route 12; flute; `battle(snorlax)` by policy (flee \| defeat \| capture) | `@Route12.snorlax_tile` | 中局30, *中局31* |
| 07.03 | EAST: Routes 12–15 to Fuchsia | `@Route12.snorlax_tile` | Routes 12, 13, 14, 15 trainers on the path `[opt: super_rod]` `[team: LG Vermilion detour, CAPTURE(staryu)]` `[opt: itemfinder]` `[opt: exp_share]`; Fuchsia Center | `@Fuchsia.center_counter` | *中局32*, *中局33*, 中局34, *中局35* |
| 07.04 | WEST: Cycling Road to Fuchsia | `@Lavender.center_counter` | Needs `bike_voucher`. To Celadon; Route 16 flute and Snorlax; `[opt: amulet_coin]`; Cycling Road (17) downhill; Route 18; Fuchsia Center | `@Fuchsia.center_counter` | *中局36*, *中局38*, 中局39 |
| 07.05 | Soul Badge | `@Fuchsia.center_counter` | Invisible-wall gym path; trainers on it; `PREP(Koga)`; `battle(Koga)`; Soul Badge, TM06 | `@Fuchsia.center_counter` | 中局40 |
| 07.06 | Safari: Gold Teeth and Surf | `@Fuchsia.center_counter` | Pay; Safari encounter mode (never the normal battle handler); Areas 1→3; Gold Teeth; Secret House: HM03; leave by a known exit | `@Fuchsia.center_counter` | 中局41, 中局42 |
| 07.07 | Strength, party | `@Fuchsia.center_counter` | Warden: teeth → HM04; `TEACH(Strength)`, `TEACH(Surf)` to the [team plan](#hm-users)'s users (FR Bulbasaur/Charmander: Surf after 08.04) `[opt: good_rod]` | `@Fuchsia.center_counter` | 中局41, *中局43* |

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
| 09.01 | ROUTE21: Pallet and Route 21 | `@Saffron.center_counter` | To Pallet (walk via Vermilion, Diglett's Cave and Route 2, or `FIELD(Fly)`); `[team: fallback CAPTURE(staryu) at Pallet]`; `FIELD(Surf)` south on Route 21; Cinnabar Center | `@Cinnabar.center_counter` | *中局44* (second half), 终局12 |
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
| A Cut user + HM01 | [team plan](#hm-users) + 03.04 | 03.05, 04.03, 06.02, 07.01, 15.08 |
| Flash (optional) | 04.01, 10 caught | 05.01 lit; default walks dark |
| Silph Scope | 06.05 | 06.09 |
| Tea | 06.01 | 06.06 (Saffron gates) |
| Poke Flute | 06.11 | 07.02 or 07.04 |
| Bicycle | `bike_voucher` | 07.04 (WEST) |
| A Surf user (Bulbasaur, Charmander) | 08.04 Lapras (FR) or 07.03 Staryu (LG) | 09.01 |
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
| 中局14 | Fuji is missing; to the Tower | 05.01 (news), 05.02 / 06.08 (Tower) |
| *中局15* | Tower 2F rival | 05.02 `early_tower`; else 06.08 |
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
| 中局27 | Scope reveals Marowak; can't be caught | 06.09 |
| 中局28 | 7F: three Rockets | 06.10 |
| 中局29 | Fuji: Poke Flute | 06.11 |
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
| 传说1 | Articuno, Seafoam | 09.02 `articuno` (`articuno.rng`) |
| 传说2 | Zapdos, Power Plant | 12.02 `zapdos` (`zapdos.rng`) |
| 传说3 | Moltres, Mt. Ember | 15.03 `moltres` (`moltres.rng`) |
| 传说4 | Mewtwo, Cerulean Cave, after the Sevii story | 17.01 `mewtwo` (`mewtwo.rng`) |
| 传说5–7 | Raikou / Entei / Suicune by starter, after the Sapphire | 17.03 `roaming_beast`; RNG at 16.03 (`roaming_beast.rng`) |
| 传说8–9 | Lugia, Ho-Oh, Navel Rock | `navel_rock` (unsupported) |
| 传说10 | Deoxys, Birth Island | `birth_island` (unsupported) |

RNG handoffs add no 52poke lines; they split checkpoints already listed.

No line is unplaced. Three are placed with a caveat: 中局14 splits between
`05.01` (the news) and the Tower rows; 中局30 is a choice line, resolved by
`south_road` and segment order; 七之岛14–15 (`15.07`) may turn out to be a
visit that changes nothing.
