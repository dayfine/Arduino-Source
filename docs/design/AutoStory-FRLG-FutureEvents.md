# FRLG AutoStory: draft events after Brock

Status: **design data / pseudocode only**. These rows are not registered in
`story_checkpoints()` and are not executable or console-validated.

Continue the structure of `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTable.cpp`
(fork #120, frlg/autostory-mvp). The implemented table ends at `00.09`, in Pewter Gym after Brock
(00.07 Viridian shopping into the forest, 00.08 forest to the Pewter Center, 00.09 Brock). Keep those
IDs unchanged. Proposed `01.xx` onward IDs below are editorial grouping, not
a commitment to checkpoint sizes or public selector values.

The [Bulbapedia walkthrough](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen)
supplies the progression inventory. Ordering choices, checkpoint boundaries,
verification contracts and helper names below are our automation proposals.
This is an original event outline, not a reproduction of the guide's routes.
Resolve coordinates, exact trigger tiles, script conditions, trainer movement
and scene end poses against the decomp and then a live run before conversion.

Run modes (planned, see the notes branch `autostory/DEMO_PLAN.md`): SPEEDRUN, DEFAULT and SAFE
choose which battles to take, whether to flee wild Pokemon, the heal threshold and what to buy.
Rows below describe DEFAULT; choices a mode would switch should be named in each row when the
row is implemented.

## How to read and eventually implement the table

The first target remains FireRed with Squirtle, as in the current MVP.
LeafGreen and other starters need explicit encounter/team profiles; the draft
does not assume the current Bubble-oriented battle logic wins later fights.

Current table helpers are `route`, `legs`, `step_into`, `talk`, `answer`,
`scene`, `battle`, `ready`, `heal` and `make`. Their current limits matter:

- `route` and `legs` need actual mapped geometry. An `@anchor` below is a
  symbolic location, **not** an existing map point or a fabricated coordinate.
- `battle` currently uses the MVP battle implementation; each named fight
  below needs its own readiness and move/party policy. All progression fights
  require a win; the tutorial's `may_lose=true` exception does not carry on.
- `ready` covers the MVP's HP/status preparation, not arbitrary PP, level,
  matchup, roster or inventory checks. `SHOP` is currently a fixed Viridian
  purchase, not a general restocking primitive.
- `scene` is the current MASH step. Item pickup, scripted travel, puzzles,
  gifts, field moves and special battle modes must not be implemented as blind
  mashing merely because the story describes a cutscene.

Pseudo-operations shown in capitals are **new requirements**, not existing
`StoryStepKind` values: `VERIFY`, `ITEM`, `TEACH`, `FIELD`, `PARTY`, `PREP`,
`PUZZLE`, `TRAVEL`, `CAPTURE`, `CHOICE`, `SAVE_BOUNDARY` and `STOP_AT`.
`VERIFY` means an observable postcondition; semantic facts such as
`ticket_received` are planning labels, not claims that we can read game RAM
or that a same-named script flag exists.

Each table row is a candidate checkpoint with ordered subevents. Split long
travel and dungeon rows into bounded, named route/battle events as their maps
are surveyed. Do not guess the unavoidable trainers from a walkthrough's
listing: choose a path, inspect sight lines, then name each fight in its table.

```text
checkpoint("01.03", "Mt. Moon fossil", start=@MtMoon.safe_entry,
           supplies=OBSERVED_AT_START, menu=FULL) {
    route("to_fossils", @MtMoon.fossil_approach, fight_wild=EXP_POLICY);
    PREP("Miguel", hp_status_pp_and_matchup_policy);
    talk("challenge_miguel");
    battle("super_nerd_miguel", pose_after=VALIDATE_ON_CONSOLE, may_lose=false);
    CHOICE("fossil", configured_choice);
    ITEM("chosen_fossil");
    VERIFY(inventory_contains(chosen_fossil) && exit_lane_open);
    route("safe_end", @MtMoon.exit_approach);
    SAVE_BOUNDARY();
}
```

In executable form, use the real `StoryPose`, `Supplies`, event parameters and
save mechanism. No new C++ helper or default option is introduced by this file.
The inventory/supplies at future starts are unknown until measured; `{2, 1}`
from Brock must not be copied forward.

## Proposed progression and dependencies

Default proposed story order:

```text
Brock -> Mt. Moon -> Cerulean / Bill / Misty -> S.S. Anne / Cut / Surge
 -> Rock Tunnel -> Celadon / Erika / Rocket Hideout / Silph Scope
 -> Pokemon Tower / Poke Flute -> Fuchsia / Surf / Strength / Koga
 -> Silph Co. / Sabrina -> Cinnabar / Blaine -> Viridian / Giovanni
 -> Victory Road -> Elite Four / Champion -> Hall of Fame
```

This is one chosen schedule, not a claim that all gyms have a forced order.
In particular, Misty and Bill can be scheduled differently; the schedule below
uses Bill first for progression and experience. Safari before Koga obtains Surf
but does not authorize its field use until the Soul Badge. Silph and Sabrina
can be moved earlier if the chosen battle policy benefits.

Bill's first Sevii visit is a selectable detour after Blaine. For a first-Hall-
of-Fame target it may be deferred; the postgame gemstone story must first
complete that visit. Optional branches never silently become prerequisites.

| Capability or state | Producer | Consumers |
|---|---|---|
| S.S. Ticket | Bill restored | S.S. Anne boarding |
| Cut known + Cascade Badge | Captain + party management + Misty | Trees gating routes and gyms |
| Flash known + Boulder Badge | 10 caught species + Route 2 aide + compatible party member | Proposed illuminated Rock Tunnel route; optional if a dark route is validated |
| Silph Scope | Rocket Hideout | Tower ghost identification |
| Poke Flute | Fuji rescued, then spoken to at home | Selected Snorlax roadblock |
| Tea handed to a guard | Celadon Mansion | Saffron access; vending-machine drinks are not the FRLG gate key |
| Bicycle | Fan Club voucher exchanged in Cerulean | Cycling Road alternative; not the coastal default |
| Surf known + Soul Badge | Safari Secret House + Koga | Sea routes, Route 23 and later islands |
| Strength known + Rainbow Badge | Gold Teeth returned + Erika | Victory Road and selected cave puzzles |
| Seven badges | Earlier gyms | Viridian Gym access |
| All eight badges | Giovanni | League checkpoint gates |
| National Pokedex | First Hall of Fame + at least 60 caught species + Oak | Postgame gemstone quest |
| Rainbow Pass | Ruby delivered to Celio | Four through Seven Island |
| Rock Smash known + Marsh Badge | Ember Spa + Sabrina | Optional obstacle routes |
| Waterfall known + Volcano Badge | Icefall Cave + Blaine | Lorelei's Icefall back-cave scene |

These dependencies summarize the cited chapters below. Field-move acquisition,
learning and badge permission are separate facts and require separate checks.

## Main story table

For every row: start from the previous verified safe endpoint; its listed
postcondition must hold before the engine commits the checkpoint's end save.
Every `@anchor` and scene pose remains TBD. A visible destination and a completed
dialog are insufficient when the row's purpose is a badge, item or changed gate.

### 01 — Pewter to Cerulean

Sources: [Part 3, after Brock](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_3),
[Part 4, Route 3 / Mt. Moon / Route 4](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_4).

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| 01.01 | Leave Pewter | `ready`/`heal`; `route(@Pewter.east_exit)`; `scene(Oaks_aide)`; receive Running Shoes. | Shoes received; aide scene over; route endpoint known. Continue walking with existing timings; running needs separate calibration. |
| 01.02 | Route 3 and cave staging | `route(Route3)` with selected trainer fights; `route(@Route4.center)`; `heal`; `PREP(MtMoon)`; optional Magikarp purchase. | Healthy party, usable PP, affordable cave supplies, recorded safe save outside Mt. Moon. |
| 01.03 | Traverse Mt. Moon | Route selected 1F/B1F/B2F path; fight required path trainers/Rockets; `PREP(Miguel)`; `battle(Miguel)`; select and collect one fossil. | Selected fossil in inventory; exit path open; exact pose reacquired. Optional chambers and loot are separate branches. |
| 01.04 | Route 4 descent | Exit cave; optional Mega Punch/Kick tutor; route down the one-way ledges to `@Cerulean.center`; `heal`. | Cerulean reached and healed; save after descent. Do not plan a walking return uphill. |

### 02 — Bill, Misty and the southern exit

Source: [Part 5](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_5).

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| 02.01 | Cerulean rival | `PREP(rival)`; route to bridge approach trigger; `battle(rival_cerulean)`; finish his return-and-gift scene. | Win; Fame Checker received; rival departure complete. |
| 02.02 | Nugget Bridge | Five individually named trainer battles with bounded healing policy; receive Nugget; battle Rocket recruiter. | All five bridge fights and recruiter resolved; north path usable. |
| 02.03 | Route 25 | Route selected trainer corridor to `@SeaCottage.bill_front`; fight route-specific trainers. | Cottage entered at an observed, stable pose. |
| 02.04 | Restore Bill | `talk(Bill)`; `answer(help,true)`; wait for chamber entry; interact with PC; `scene(separation)`; `talk(Bill)` again. | Bill human; S.S. Ticket received; return route available. |
| 02.05 | Cascade Badge | Return to Cerulean; heal/restock; route selected gym approach; `talk(Misty)`; `battle(Misty)`; finish rewards. | Cascade Badge and TM03 received; exit pose known. Squirtle-only battle assumptions must be replaced first. |
| 02.06 | Burgled house | Verify officer moved; enter house and rear exit; challenge thief; `battle(rocket_thief)`; collect TM28. | Thief resolved; southern passage usable; Dig reward accounted for. |
| 02.07 | Underground Path | Route 5; optional Day Care left disabled; enter north-south Underground Path; emerge on Route 6. | Correct southern exit/map confirmed; no unintended deposited party member. |

### 03 — Vermilion, Cut and Lt. Surge

Source: [Part 6](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_6).

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| 03.01 | Route 6 to Vermilion | Route selected trainer path; enter Center; `heal`; `PREP(ship_and_surge)`. | City endpoint known; battle and Cut-user plan recorded. |
| 03.02 | Travel support | Optional `ITEM(VsSeeker)` and Old Rod; obtain Bike Voucher if choosing bike branch. | Each requested gift observed; skipped gifts remain skipped, not inferred present. |
| 03.03 | Board S.S. Anne | Route harbor; ticket check; enter ship; route to selected healing cabin or rival approach. | Boarding accepted; correct ship room and return route known. |
| 03.04 | Ship rival | `PREP(rival_ship)`; route upper-deck trigger; `battle(rival_ss_anne)`; resolve departure dialog. | Win and stable captain approach. |
| 03.05 | Captain's Cut | `talk(captain)`; `scene(backrub)`; receive HM01; `PARTY(compatible_cut_user)`; `TEACH(Cut,explicit_slot)`; leave ship. | Cut actually learned; ship departure scene finished; player at harbor anchor. |
| 03.06 | Reach Surge | `FIELD(Cut,gym_tree)`; enter gym; fight selected trainers; `PUZZLE(two_trash_switches)` with observed resets. | Both locks open in this attempt; no fixed trash-can sequence assumed. |
| 03.07 | Thunder Badge | `PREP(Surge)`; `talk(Surge)`; `battle(Surge)`; finish rewards; return/heal. | Thunder Badge and TM34 received; party usable. Ground coverage or another validated strategy required. |

### 04 — Flash branch, Rock Tunnel and Celadon

Sources: [Part 7](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_7),
[Part 8](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_8).

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| 04.01 | Flash preparation branch | If illuminated route selected: `CAPTURE(unique_species_until_10)`; traverse Diglett's Cave to eastern Route 2; Cut access trees; talk to aide; `TEACH(Flash)`. | HM05 received and learned. If declining branch, require a separately validated dark-navigation profile. |
| 04.02 | Return to Cerulean | Route back through cave and Vermilion, then Underground Path; optionally exchange Bike Voucher. | Correct Cerulean anchor; Bicycle only assumed if exchange verified. |
| 04.03 | Routes 9 and 10 | Cut eastern exit tree; route chosen trainer path; heal at Route 10 Center; stage Rock Tunnel. | Cave-entry save and sufficient HP/status/PP supplies. |
| 04.04 | Rock Tunnel | Enter; `FIELD(Flash)` if selected; route floor/ladder anchors; resolve path trainers; exit south. | Correct Route 10 south exit; no inferred shortcut through darkness. |
| 04.05 | Lavender staging | Route to Center; heal/restock. Default skips early Tower ascent so its rival is fought once after the Scope. | Lavender endpoint recorded; Tower progress accurately marked untouched. |
| 04.06 | Celadon access | Route 8; east-west Underground Path; Route 7; enter Celadon Center. | Celadon arrival and safe save; do not attempt Saffron gates before Tea. |

### 05 — Erika, Rocket Hideout and the Poke Flute

Sources: [Part 9](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_9),
[Rocket Hideout](https://bulbapedia.bulbagarden.net/wiki/Rocket_Hideout).

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| 05.01 | Celadon services | Obtain Tea from mansion; shop by explicit budget; optional Eevee/stone or battle TMs as roster policy requires. | Tea held; purchases/gifts verified; no accidental move replacement. |
| 05.02 | Rainbow Badge | Cut gym access; route trainers; `PREP(Erika)`; `battle(Erika)`; finish rewards. | Rainbow Badge and TM19; learned Strength will later be usable. |
| 05.03 | Hideout entrance | Battle poster guard; inspect poster switch; enter stairs. | Hidden entrance open and B1F anchor confirmed. |
| 05.04 | Lift Key | Traverse spinners/stairs to B4F northwest; battle key grunt; **talk again**; pick up dropped key. | Lift Key held; not merely grunt beaten. |
| 05.05 | Hideout Giovanni | Return to lift; select B4F; defeat door guards; open boss access; `battle(giovanni_hideout)`; collect Silph Scope. | Scope held; boss scene finished. |
| 05.06 | Saffron gate unlock | Give Tea to a gate guard; traverse Saffron toward Lavender. | Guard admits player; shared gate access established. |
| 05.07 | Tower rival | Enter Tower; `PREP(rival_tower)`; `battle(rival_tower)`; route upper floors. | Rival beaten; named floor anchor. |
| 05.08 | Ghost and rescue | Ascend with healing zone policy; identify/defeat Marowak; fight three upper-floor Rockets; speak to Fuji; complete escort. | Fuji returned home; correct post-escort pose. Ghost is a special encounter, not a capture target. |
| 05.09 | Poke Flute | Speak to Fuji at home; receive flute; prepare southern travel. | Poke Flute held. Rescue alone is not the item postcondition. |

### 06 — Fuchsia, Safari tools and Koga

Source: [Part 10](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_10).

Default route choice is the eastern coastal approach, avoiding a compulsory
Bicycle. The western branch is retained explicitly rather than running both.

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| 06.01 | Choose southern road | East default: Route 12, flute, resolve Snorlax, Routes 13/14/15. West alternative: Bicycle, Route 16 flute/Snorlax, Routes 17/18. | Selected roadblock removed; capture vs defeat policy explicit; no dependency on the unused road. |
| 06.02 | Fuchsia staging | Route selected trainers to Center; heal; budget Safari fee and supplies. | Fuchsia safe save; Cut/party roles and bag space checked. |
| 06.03 | Safari entry | Pay admission; use Safari-specific encounter handling; route Center → Area 1 → Area 2 → Area 3. | Remaining step budget sufficient; normal wild-battle combat/flee handler never used here. |
| 06.04 | Gold Teeth and Surf | Pick up Gold Teeth; enter Secret House; talk for HM03; leave/retire with a known exit. | Both Gold Teeth and HM03 observed; no success from walking near their tiles. |
| 06.05 | Strength and party | Return teeth to Warden; receive HM04; `TEACH(Strength)`; `TEACH(Surf)` to chosen compatible members. | Both moves learned; move-slot choices recorded. Surf still gated by Koga. |
| 06.06 | Soul Badge | Route invisible-wall gym path; fight selected trainers; `PREP(Koga)`; `battle(Koga)`; finish rewards. | Soul Badge and TM06; actual Surf field capability confirmed. |

### 07 — Silph Co. and Sabrina

Sources: [Part 11](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_11),
[Silph Co.](https://bulbapedia.bulbagarden.net/wiki/Silph_Co.).

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| 07.01 | Saffron services | Return via unlocked gates or verified Fly; heal; optional Psychic TM/Dojo branch; enter Silph. | Silph entry and staging save; no assumed teleport unlocks. |
| 07.02 | Card Key | Route to fifth-floor key corridor; fight selected guards/scientists; pick up Card Key. | Key held; door-opening behavior tested. |
| 07.03 | Rival access | Return to third-floor central room; open key door; use mapped warp to seventh-floor rival room. | Arrival floor/room verified; not just a screen fade. |
| 07.04 | Silph rival | `PREP(rival_silph)`; `battle(rival_silph)`; optional Lapras gift with party space; restore party readiness. | Rival beaten; requested Lapras received; onward warp usable. |
| 07.05 | Silph Giovanni | Warp to 11F; resolve guard and locked access; `PREP(giovanni_silph)`; `battle(giovanni_silph)`. | Boss leaves; occupation-ending scene complete. |
| 07.06 | President and city | Talk to President for Master Ball; exit; confirm gym obstruction removed. | Master Ball accounted for; Saffron Gym accessible. |
| 07.07 | Marsh Badge | Follow mapped gym teleport sequence; `PREP(Sabrina)`; `battle(Sabrina)`; finish rewards. | Marsh Badge and TM04; gym exit pose established. |

### 08 — Cinnabar and Blaine

Sources: [Part 12, sea approach](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_12),
[Part 13, Cinnabar](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_13).

The proposed default returns to Pallet and Surfs south on Route 21; this is
our scheduling choice. The walkthrough's Route 19/20/Seafoam approach is an
alternative branch with its own Strength/current puzzle, listed later.

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| 08.01 | Sea preparation | Return to Pallet using verified travel; heal; verify Surf user, permission and sea-route battle policy. | Known shore entry pose and usable party. |
| 08.02 | Reach Cinnabar | Surf Route 21; handle chosen trainer/encounter path; land at Cinnabar; heal. | Landed at correct island Center and saved. |
| 08.03 | Mansion entry | Enter Mansion; traverse mapped stairs/statue switches to upper-floor drop approach. | Correct switch state and drop alignment; route cannot treat closed doors as ordinary floor. |
| 08.04 | Secret Key | Take validated drop; descend basement; solve remaining switch/door sequence; collect Secret Key; exit. | Key held; player outside Mansion at known pose. |
| 08.05 | Gym rooms | Unlock gym; solve each quiz terminal or explicitly fight its trainer; verify each door. | Blaine approach reachable; answer policy tied to actual question, not repeated YES. |
| 08.06 | Volcano Badge | `PREP(Blaine)`; `battle(Blaine)`; finish rewards; exit to Bill's invitation. | Volcano Badge and TM38; invitation recognized without accidental acceptance. |
| 08.07 | Bill branch decision | First-Hall-of-Fame default: decline and record deferred visit. If first-islands target selected: run branch `S1` below. | Either free control on Cinnabar or S1 completed; correct branch state saved. |

### 09 — Eighth badge and Victory Road

Sources: [Part 15](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_15),
[Part 16](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_16).

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| 09.01 | Return to Viridian | Travel north via Route 21/Pallet or validated Fly; heal; verify seven badges and gym entry. | Viridian staging save with gym accessible. |
| 09.02 | Gym spinner path | Route mapped spinner sequence; deliberately trigger trainer movements needed to clear access; fight selected trainers. | Giovanni approach open; moved NPC positions reflected in route state. |
| 09.03 | Earth Badge | `PREP(giovanni_gym)`; challenge/battle; finish reward/departure scene. | Earth Badge and TM26; all eight badges observed. |
| 09.04 | Route 22 rival | Heal/restock; route west trigger; `battle(rival_route22)`. | Win; League approach free; party ready to continue. |
| 09.05 | Badge gates | Traverse reception/Route 23 checks in order; use Surf on the water section; reach Victory Road entry. | All badge checks passed; known cave-entry save. |
| 09.06 | Victory Road 1F | Activate Strength; push first mapped boulder to switch; verify barrier removal; route to next floor. | Correct boulder/switch state and safe floor anchor. |
| 09.07 | Victory Road upper floors | Solve second-floor switch and third-floor switch; resolve selected trainers; push drop boulder and follow it; solve final switch. | Each barrier actually open; state tracked across falls and map reloads. |
| 09.08 | Indigo staging | Exit cave; Route 23 north to Plateau; heal/shop; `PREP(full_league_gauntlet)`. | Team, levels, PP, healing/revival budget and move coverage verified before entering. |

### 10 — First League and Hall of Fame

Source: [Part 17](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_17).

League interior rows are event boundaries, **not automatically safe reload
checkpoints**. Choose an explicit save policy: replay from the pre-League save,
or persist a validated room-specific save and its exact remaining resources.
The existing universal checkpoint save behavior must be reviewed first.

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| 10.01 | Enter League | Verify target is first-round teams; commit pre-League save; enter Lorelei room. | Correct run mode and no unsupported mid-gauntlet reset assumption. |
| 10.02 | Lorelei | `PREP(Lorelei,carried_items_only)`; challenge; `battle(Lorelei)`; handle level/move prompts. | Win; next door open; HP/status/PP reconciled. |
| 10.03 | Bruno | Restore with carried resources; enter/challenge; `battle(Bruno)`. | Win; next door open; surviving roster checked. |
| 10.04 | Agatha | Restore; enter/challenge; `battle(Agatha)` with status/confusion policy. | Win; no unresolved status/menu before next room. |
| 10.05 | Lance | Restore; enter/challenge; `battle(Lance)`. | Win; Champion door open; final preparation performed before crossing trigger. |
| 10.06 | Champion | Enter automatic rival trigger; `battle(champion_rival,starter_profile)`; complete Oak/rival aftermath. | Champion defeated; Hall of Fame sequence actually reached. |
| 10.07 | Hall of Fame and credits | Register team; wait through credits and title/Continue sequence; reacquire postgame control. | Hall of Fame recorded; postgame state/pose confirmed; first-story target stops here. |

## S1 — First Sevii visit, selectable after Blaine or before postgame quest

Source: [Part 14](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_14).
This branch is optional for the first League target and required before the
later Celio quest. It must be invoked once, not replayed on every resume.

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| S1.01 | Bill's ferry | Stage party before leaving; accept Bill's invitation; complete One Island Network Center scene. | One Island pose; Tri-Pass and Meteorite received; initial unavailable PC services accounted for. |
| S1.02 | Two Island request | `TRAVEL(ferry,TwoIsland)`; talk to Game Corner owner; learn Lostelle is missing. | Search scene complete; no claim that Meteorite has already been delivered. |
| S1.03 | Three Island bikers | Ferry to Three Island; route to biker confrontation; resolve its battle sequence. | Bikers depart; Bond Bridge route usable. Each fight needs an individual battle boundary. |
| S1.04 | Berry Forest rescue | Route Bond Bridge/forest; talk to Lostelle; resolve Hypno by configured defeat/capture policy; finish escort. | Lostelle safe; automatic transfer to Two Island recognized. |
| S1.05 | Deliver Meteorite | Talk to father after rescue; complete delivery/reward; return to One Island and Bill. | Meteorite delivery complete; Bill/Celio scene returns player to Cinnabar. |
| S1.06 | Optional Ember tools | At an explicit island-travel opportunity: visit Ember Spa for HM06; teach Rock Smash only if requested. | HM06/learned move recorded; return anchor/travel destination explicit. |

## Postgame progression table

Target `network_complete` includes P1 and P2. Target `mewtwo_ready` continues
through P3. Capture, rematch and exploration targets remain opt-in branches.

### P1 — National Pokedex, Ruby and Rainbow Pass

Source: [Part 18](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_18).

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| P1.01 | National Dex gate | Verify first Hall of Fame; if not already upgraded, require 60 caught species via an explicit bounded collection plan or stop at unmet prerequisite; complete Oak's upgrade scene. | National Pokedex upgrade observed. Seen count is not caught count. |
| P1.02 | Complete first visit | If S1 incomplete, return to Bill and finish S1; travel to One Island; talk to Celio about gemstone. | Initial island story complete; Ruby request active. |
| P1.03 | Mt. Ember entrance | Route Kindle Road to Ember; observe Rocket password scene; battle both guards. | First warehouse password learned; Ruby tunnel accessible. |
| P1.04 | Ruby cave | Traverse mapped floors/Strength puzzle; pick up Ruby; return to Celio. | Ruby delivered; Rainbow Pass received. |
| P1.05 | Icefall tools | Ferry Four Island; enter Icefall Cave; solve cracked-floor/ice path; collect HM07; teach Waterfall; verify field permission. | Waterfall learned and usable; exact entrance-side pose. |
| P1.06 | Lorelei rescue | Climb waterfall; reach Back Cave; speak to Lorelei; battle assigned Rocket; complete interrogation. | Lorelei scene complete; next quest access state updated. |

### P2 — Sapphire and the Network Machine

Sources: [Part 18, Six Island](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_18),
[Dotted Hole](https://bulbapedia.bulbagarden.net/wiki/Dotted_Hole),
[Part 19, Five Island](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_19).

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| P2.01 | Six Island | Ferry Six Island; resolve optional rival conversation if encountered; route Water Path south to Ruin Valley. | Dotted Hole approach and correct quest state; do not force entry while story-blocked. |
| P2.02 | Dotted Hole | Use Cut on sealed door; descend mapped hole sequence; approach Sapphire. | Scientist theft scene complete; second warehouse password learned; Sapphire is **not** yet in inventory. |
| P2.03 | Warehouse entry | Ferry Five Island; route Five Isle Meadow; use both learned passwords at warehouse entrance. | Entrance opens; indoor anchor confirmed. |
| P2.04 | Warehouse maze | Follow mapped spinner path; fight path grunts; then resolve female and male Admin battles. | Admin defeat/movement states open scientist route. |
| P2.05 | Reclaim Sapphire | `PREP(Gideon)`; challenge Scientist Gideon; win; receive Sapphire. | Sapphire held; safe exit route. |
| P2.06 | Complete network | Return One Island; give Sapphire to Celio; complete machine scene. | Network quest completion observed; target `network_complete` may stop here. No link trade is attempted. |

### P3 — Cerulean Cave and optional League rematch

Source: [Part 20](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_20).

| ID | Event | Ordered draft steps | Commit only after |
|---|---|---|---|
| P3.01 | Cave access | Return Cerulean; Surf from Route 24; verify guard no longer blocks entrance. | Actual cave entry, not just assumed completion of prerequisites. |
| P3.02 | Mewtwo approach | Traverse mapped 1F/2F/B1F path; prepare Repel/encounter policy; stage at Mewtwo interaction anchor. | Stable pre-encounter save; target `mewtwo_ready` stops without pressing A. |
| P3.03 | Optional Mewtwo capture | Only for capture target: verify capture policy and resources; interact; execute bounded capture; verify roster/PC result. | Mewtwo captured, or explicit failure without overwriting the pre-encounter save. |
| P3.04 | Rematch staging | If requested after network quest: return Plateau; heal/restock; verify upgraded team profile. | Rematch roster/PP/resources ready; separate pre-League save. |
| P3.05 | Rematch battles | Separate events for Lorelei, Bruno, Agatha, Lance and Champion; reconcile resources between each. | Five wins against rematch profiles; not reused first-round assumptions. |
| P3.06 | Second Hall of Fame | Finish registration/credits; return to stable postgame control. | Rematch completion recorded; stop rather than looping indefinitely. |

## Optional branches and walkthrough coverage

These are proposed event chains, not implied work for the main-story target.
All permanent gift/capture choices need a selected policy and a checked result.
Ordinary hidden items, full encounter collections and every optional trainer
are not individually enumerated here; selected routes must expand their own
trainer and supply events before implementation.

| Branch | Gate / source | Draft event chain and completion evidence |
|---|---|---|
| O.01 Magikarp / fossil support | [Part 4](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_4) | Purchase Magikarp only for chosen roster → verify party/PC destination. Optional cave chambers, Moon Stones and route tutors require explicit pickup/teaching checks. Fossil choice itself stays in 01.03. |
| O.02 Cerulean/Vermilion services | [Parts 5](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_5) / [6](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_6) | Optional Powder Jar, Day Care, Old Rod, Vs. Seeker, NPC trades → verify respective gift/exchange and resulting party. Bike Voucher → Bicycle is its own checked exchange. |
| O.03 Route 11 / museum | [Part 7](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_7) | Optional Itemfinder aide requires 30 caught species → verify gift. Cut museum side access → receive Old Amber → reserve for Cinnabar revival. Flash route is already represented in 04.01. |
| O.04 Early Tower visit | [Part 8](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_8) | Fight rival before Celadon if selected → record it → skip that fight in 05.07. Do not attempt ghost completion without Scope. Optional Name Rater interaction needs its own naming policy. |
| O.05 Celadon roster/economy | [Part 9](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_9) | Eevee gift → chosen evolution; Coin Case/prize purchases; rooftop exchanges/tutors → verify costs, party space and exact moves. No unbounded gambling event. |
| O.06 Fly and Cycling Road | [Part 10](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_10) | Cut Route 16 access → receive HM02 → teach compatible member → verify Fly destination selection. Western road also needs Bicycle and chosen Snorlax resolution; downhill movement requires its own controller policy. |
| O.07 Eastern coast extras | [Part 10](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_10) | Optional Good/Super Rod and Exp. Share aide (50 caught species) → verify gifts; alternate road exploration must not re-trigger a resolved Snorlax as if untouched. |
| O.08 Dojo / Saffron gifts | [Part 11](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_11) | Fight selected Dojo trainers and master → choose Hitmonlee/Hitmonchan → verify gift. Optional Mr. Psychic, Copycat and Silph tutor events need item/move contracts. Lapras selection is in 07.04. |
| O.09 Seafoam alternative / Articuno | [Part 12](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_12) | Surf Routes 19/20 → solve Strength boulder/current stages → optionally stop/save before Articuno → capture only if selected → continue west to Cinnabar. Track boulders/floors separately from player pose. |
| O.10 Cinnabar revival / tutors | [Part 13](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_13) | Hand over chosen Mt. Moon fossil or Old Amber → complete required leave/return interaction → receive revived Pokemon; verify party space. Optional trades/tutor are separate choices. |
| O.11 One–Three Island extras | [Part 14](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_14) | Treasure Beach, Ember Spa, Cape Brink tutor and Two Island Move Maniac are explicit detours. Moltres approach → pre-encounter stop/save → optional capture. Do not conflate Moltres with S1 rescue progress. |
| O.12 Power Plant / Zapdos | [Part 15](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_15) | Surf to Plant → distinguish item balls from stationary encounters → reach Zapdos → stop/save → optional capture. Return to a named travel anchor. |
| O.13 Four / Six Island exploration | [Part 18](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_18) | Day Care, tutor/sticker services, Pattern Bush, Outcast Island/Altering Cave are opt-in visits with explicit interaction goals; none substitutes for Lorelei/Dotted Hole progression. |
| O.14 Five Island side quests | [Part 19](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_19) | Memorial Pillar tribute → checked reward; Water Labyrinth friendship gift → checked egg/space; Lost Cave rescue of Selphy → escort state; later requested-species/reward tasks are bounded, separate objectives. |
| O.15 Seven Island / Tanoby | [Part 19](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_19) | Canyon Entrance → Sevault Canyon → solve Tanoby Key boulders → verify Ruins encounter unlock → optional Unown collection. Trainer Tower is a separate challenge mode with explicit battle format and endpoint. |
| O.16 Roaming beast | [Part 20](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:Pok%C3%A9mon_FireRed_and_LeafGreen/Part_20) | After network completion, choose bounded search/capture target; beast depends on starter (Squirtle profile: Raikou). Preserve one-time encounter state; do not insert random searching into linear story progress. |
| O.17 Birth Island / Navel Rock | Walkthrough index Part 21 is a red link at research time. | Reserved external-access branches, not part of the default route. Ticket/access prerequisites, version/platform support, puzzle and encounter contracts need separate sourcing before any runnable rows are written. Do not assume access from finishing the network quest. |

## Missing capabilities exposed by the draft

| Capability | First use | Requirement before executable events |
|---|---|---|
| Broader battle/party policy | Misty and later rivals; Surge in particular | HP/status/PP, move effectiveness, switching, fainted members, bounded recovery, level/evolution/move prompts and trainer vs wild distinction. |
| Capture and caught-species accounting | Cut user / Flash branch; later National Dex | Bounded encounters/balls, unique-species counting, party/PC destinations, no assumed count from sightings. A full 60-species plan is still to be designed. |
| General item and shop interaction | Fossil, key items, evolving roster | Item identity/quantity, bag capacity, purchase budget, repeat-safe gifts and consumed items. |
| HM/TM learning and field actions | Cut | Explicit compatible member and replacement slot; confirm learned move and badge; replan with changed cut-tree/field state. |
| Story-aware map state | Bill return, gyms, Rocket occupation | Actual current object/trigger/switch state. Do not remove all post-Hall-of-Fame objects up front as a general substitute. |
| Forced movement / dynamic puzzles | Hideout spinners; later boulders | Directed transitions, door state, pushable objects, floor drops, currents and ice. Ordinary walkable grids do not model these automatically. |
| Observed puzzle search | Surge | Read first/second-switch/reset outcomes; bounded attempts; no assumed random layout. |
| Special interaction modes | Safari, ship lift/ferry, teleports | Correct menu and mode ownership; Safari encounters cannot use ordinary battle handlers. |
| Arrival and scene verification | Every new dungeon/escort | Map/pose/facing after automatic movement and transfers, with evidence; no guessed `pose_after`. |
| Save/resume manifests | Every promoted checkpoint; League especially | Exact story prerequisites, party, inventory, puzzle state and retry scope. Reloading a save must restore the state the row actually expects. |

## Suggested first implementation slice

Implement only `01.01`–`01.04` next: the post-Brock aide, Route 3, Mt. Moon's
required path/fossil choice and arrival in Cerulean. That slice tests the first
new map set, item selection, a required dungeon fight and a one-way descent
without committing to the whole battle/party strategy for Misty onward.

For each subsequent promotion: resolve symbols against map scripts; split
route-specific trainers into named events; choose party/move/supply policy;
test table dependencies and map replay; validate actual trigger and scene end;
then record a restartable save boundary. Simulation establishes table
consistency, not proof that a controller action completed the story effect.
