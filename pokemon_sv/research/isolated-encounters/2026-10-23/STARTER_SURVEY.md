# Terarium starter survey and priority Charcadet hunt

2026-10-23 PDT. Incorporated at the user's request into the isolated shiny-hunt
plan. **Research only; no new live route, catches or program behavior verified.**

## Findings and evidence levels

Reviewed the local spawn/habitat tables in Bulbapedia's four biome pages and
Chargestone Cavern, rather than treating a biome as one uniform pool. All 24
Gen I–VIII starters are accounted for by their League Club upgrade. There are:

- **10 A candidates:** at least one published local habitat lists no other
  Pokémon of the selected sandwich type. Navigation and density remain unknown.
- **4 B candidates:** time, shoreline or a mixed two-starter pool needs resolving.
- **9 C candidates:** listed competitors require finer habitat isolation; not
  declared impossible or ineligible for a mixed-family hunt.
- **1 D candidate:** Mudkip's location is confirmed, but the cave table could
  not be inspected because its Bulbapedia page returned maintenance.

These tiers describe source support for isolation, not actual spawn probabilities,
automation readiness or measured difficulty. A does not guarantee that every
spawn will be the target. Boundary overlap, fixed encounters, spawn-position
availability and unlocked content still need field checks. Empty distance/time
fields mean unmeasured, not zero.

**Upgrade cost:** each biome diversity upgrade is 3,000 BP; all four total
12,000 BP if none is unlocked. Six starters are unlocked by each. Chespin needs
**Canyon**, even though its cavern extends under Polar. No BP was spent here.
Source: [League Club Room — Terarium Club](https://bulbapedia.bulbagarden.net/wiki/League_Club_Room#Terarium_Club).

## All 24 starters

The region is the **unlock biome**; scope describes the actual spawn table.
For unnamed first tables, “main” means that page's initial table, not the entire
biome. The source table's terrain category is retained; “Volcano” does not mean
that the named area necessarily looks like a volcano in the game.

| Starter | Unlock | Type | Tier | Selected scope | Listed competition / basis |
| --- | --- | --- | --- | --- | --- |
| Bulbasaur | Coastal | Poison | A | Torchlit Mountain / Rocky Area | No other listed Poison species in this habitat. Grass has several rivals. |
| Charmander | Savanna | Fire | B | Savanna entrance / Prairie | Litleo and Pyroar; their weights vary by time. |
| Squirtle | Canyon | Water | B | Canyon / Lake or Riverside ground | Only listed land Water target; fish share the water pool. |
| Chikorita | Coastal | Grass | C | Main Coastal / Prairie or Desert | Deerling, Sawsbuck, Exeggcute/Exeggutor; some areas add Grookey. |
| Cyndaquil | Polar | Fire | A | Iceberg Cave | No other listed Fire species in this cave. |
| Totodile | Savanna | Water | C | Savanna entrance or Northeast / Volcano | Dewpider on land; Bruxish in water. |
| Treecko | Canyon | Grass | C | Nature Preserve / Prairie | Deerling competes in the same habitat. |
| Torchic | Polar | Fire | A | North Mountain Cave | No other listed Fire species; Alolan Vulpix is Ice. |
| Mudkip | Coastal | Water | D | Torchlit Labyrinth | Species page confirms location/unlock; local table unavailable. |
| Turtwig | Canyon | Grass | C | Craggy Cliffs / Prairie or Rocky Area | Deerling and Sawsbuck compete. |
| Chimchar | Polar | Fire | B | Mountain / Snowfield | Scorbunny shares the listed Fire pool. |
| Piplup | Polar | Water | C | Polar snowfield / coast | Oshawott, Seel and Dewgong; more fish offshore. |
| Snivy | Savanna | Grass | C | Northeast Savanna / Prairie | Deerling, Sawsbuck and Alolan Exeggutor compete. |
| Tepig | Canyon | Fire | A | Main Canyon or Nature Preserve / Rocky Area | No other listed Fire species in these selected habitats. |
| Oshawott | Polar | Water | C | Polar snowfield / coast | Piplup, Seel and Dewgong; more fish offshore. |
| Chespin | Canyon | Grass | A | Chargestone Cavern / Cave | Only listed Grass species; Volcano habitat also lists it. |
| Fennekin | Savanna | Fire | C | Midwest Savanna / Prairie | Litleo, Pyroar, Magby and Magmar compete. |
| Froakie | Coastal | Water | A | Coastal Plaza Island / Prairie | No other listed Water species in Prairie; shore adds Popplio. |
| Rowlet | Savanna | Flying | A | Main Savanna table / Prairie | Only listed Flying species there; Grass has Deerling/Sawsbuck. |
| Litten | Canyon | Fire | A | Craggy Cliffs / Prairie or Rocky Area | No other listed Fire species in these habitats. |
| Popplio | Coastal | Water | A | Main Coastal or Southeast Beach / Desert | No other listed Water species in selected land habitat. |
| Grookey | Coastal | Grass | C | Southeast Beach / Prairie | Chikorita, Deerling/Sawsbuck, Exeggcute/Exeggutor compete. |
| Scorbunny | Polar | Fire | B | Mountain / Snowfield | Chimchar shares the listed Fire pool. |
| Sobble | Savanna | Water | A | Savanna / Bamboo Forest, or Northeast Prairie | No other listed Water species in these selected habitats. |

Source tables: [Savanna](https://bulbapedia.bulbagarden.net/wiki/Savanna_Biome#Pok%C3%A9mon),
[Coastal](https://bulbapedia.bulbagarden.net/wiki/Coastal_Biome#Pok%C3%A9mon),
[Canyon](https://bulbapedia.bulbagarden.net/wiki/Canyon_Biome#Pok%C3%A9mon),
[Polar](https://bulbapedia.bulbagarden.net/wiki/Polar_Biome#Pok%C3%A9mon),
[Chargestone](https://bulbapedia.bulbagarden.net/wiki/Chargestone_Cavern#Pok%C3%A9mon),
[Mudkip's game locations](https://bulbapedia.bulbagarden.net/wiki/Mudkip_(Pok%C3%A9mon)#Game_locations).

Machine-readable records: [24 candidates](STARTER_CANDIDATES.csv) and
[all inspected starter habitat/type cases](STARTER_POOL_EVIDENCE.csv).
The latter retains source weight, level, section, habitat, same-type competitors
across all listed terrains and a separate land-only competitor list. Species
and forms were resolved before comparing types: Alolan Vulpix is Ice; Deerling
and Sawsbuck are Normal/Grass; regional/form names cannot be dropped silently.

## Chespin: promote to first reusable-route experiment

Chespin/Grass in [Chargestone Cavern](https://bulbapedia.bulbagarden.net/wiki/Chargestone_Cavern#Pok%C3%A9mon)
is the best first **integration** candidate because BBQ already reaches the fly
point and walks into a known battle court. Its Cave entry has source weight 100;
the Volcano entry has weight 10. These are weights, not percentages or a measured
10x yield comparison. No other Grass species is listed in either habitat.
The separate Wasteland and Chargestone habitats do not list Chespin, so arriving
inside the cavern alone does not prove that the player reached an eligible spot.

Reuse candidates: central_to_chargestone(), the BBQ court approach, shared
Let’s Go tools, and Area Zero hunter encounter/recovery orchestration. The
internal rest area is a visited fly destination and has healing, per
[Bulbapedia's rest-area description](https://bulbapedia.bulbagarden.net/wiki/Chargestone_Cavern#Rest_area).
Actual fly-to-Chespin distance remains unmeasured. Known court access gives us
an integration advantage, not proof of a flat or sufficiently dense Chespin patch.

Field experiment:

1. Check the Canyon upgrade, correct lead, fly unlock, camera and Grass power.
2. Photograph landing, existing court and actual Chespin spawn footprint.
3. Compare repeated clearing, a short local refresh, and same-cavern re-flight.
   The existing central_to_chargestone() assumes Central Plaza as its start;
   it cannot be reused unchanged for a cave-to-cave flight.
4. Measure reachable target encounters, not only visible Chespin or kill sounds.
5. Verify battle/shiny handling and return-to-anchor after encounters.
6. Check whether picnic/sandwich setup works locally; otherwise prepare outside
   and measure flight/setup overhead. Do not assume the Area Zero restriction
   or any other location's picnic behavior applies here.

The BBQ call passes throw_ball_if_bubble=false. A shiny hunter needs an explicit
bounded acquisition/encounter policy; copying the clearing call alone is not a
complete hunt. Retain the earlier M9 warnings about unconditional heals and
long recovery loops. Area Zero's orchestration is a better model for a sustained
hunt than repeating the old BBQ leave/heal/re-fly cycle unchanged.

## Other first-round candidates

**Torchic / North Mountain Cave** and **Cyndaquil / Iceberg Cave** are the next
cave candidates: each local table contains only one Fire species. Both use the
Polar upgrade. Their approaches, entrances, room shapes and refresh methods
still need mapping; neither has the same existing route advantage as Chespin.

**Tepig and Litten / Fire** have separate eligible Canyon habitat tables. A
single Canyon upgrade also unlocks Chespin, making these useful follow-up scouts.
**Bulbasaur / Poison** is another strong data candidate: the Torchlit Mountain
Rocky Area table has no competing Poison species, while Grass has rivals.

**Sobble / Water** has selected land habitats without competing Water entries.
**Rowlet / Flying** similarly has a promising initial Savanna Prairie table.
**Froakie / Water** requires the island's inland Prairie; the desert/shore table
adds Popplio. **Popplio / Water** has a separate land-Desert candidate. These
last two especially require proving that the actual spawn footprint excludes
neighboring water/shore pools.

Mudkip's game-locations entry confirms Torchlit Labyrinth and the Coastal
upgrade. The local cave page returned a maintenance screen, so the report leaves
its pool purity unknown. Do not silently promote it based on general guide
reputation. It remains a good follow-up source/field check.

## Charcadet: explicit user-priority target

Charcadet is a separate Paldea hunt, not one of the 24 Terarium starters. Keep
it high priority independently of broad family-coverage ranking.

- [Bulbapedia: West Province Area One](https://bulbapedia.bulbagarden.net/wiki/West_Province_(Area_One)#Pok%C3%A9mon)
  lists Charcadet in Prairie and Rocky Area habitats. The selected Rocky Area
  table lists Makuhita, Skiddo, Yungoos, Gumshoos, Nacli and Charcadet: only
  Charcadet is Fire. Its source weight is 1; that is not a 1% encounter chance.
- The earlier optimizer extraction has 18 score-100 Charcadet table/type records
  in each version. That supports scouting, not a guaranteed live pool.
- [Austin John Plays at 8:39](https://www.youtube.com/watch?v=wP2rClXrn3A&t=519s)
  demonstrates the West Province Area One Central hunt. The exact video anchor
  still needs to be matched to the table and flight point.
- No Terarium biodiversity purchase is needed. First inspect the demonstrated
  spot and its reset geometry. Keep a prepared-start prototype independent of
  automatic travel until the movement loop is proven.
- Its small visual shiny distinction makes battle confirmation important.
  Spatial isolation can be easy while human-style visual inspection is harder.
  The same acquisition gate applies as for starter hunting.

## Revised field order and acceptance

1. **Chespin / Chargestone:** first integration experiment, familiar flight/court.
2. **Charcadet / West Province One Central:** user-priority target, Fire isolation.
3. **Torchic and Cyndaquil caves:** test transfer of the small-area hunt pattern.
4. **Tepig, Litten, Bulbasaur/Poison:** source-supported ground candidates.
5. **Sobble, Rowlet, Froakie, Popplio:** resolve exact footprint and access.
6. Mixed pools and Mudkip's source gap remain in the catalog for further scouting.

Mareep/Oricorio/Kirlia remain boundary-reset controls from the Paldea study;
Larvesta remains a riding-loop control. They are not discarded, but Chespin
and Charcadet now get the first field attention for reuse and user value.

For each candidate: verify unlocks and species at the exact anchor, record
approach time and terrain, screen reset cycles and bounded target acquisition,
then choose the first implementation. Follow the existing plan's native C++
testing, local build and targeted/stability validation gates. No automated
family coverage or shiny/hour rate is claimed from this desk research.

## Method and limits

Bulbapedia was read through the web tool. Direct HTML downloads returned 403;
Torchlit Labyrinth returned a maintenance page. Local text windows were merged
by line number and inspected, retaining section/habitat distinctions. Parsing
handled split species/form rows; known form types were resolved explicitly.
All parsed species rows in the inspected tables have resolved types. Twenty-three
starters have table evidence; Mudkip has location/unlock evidence only. League
Club's list checks completeness: six species per upgrade, 24 distinct starters.

This is research-data processing, not an application test harness. Raw page
windows remain in /tmp/sv-starter-survey; metadata and cached-text hashes are
in [starter_source_manifest.json](starter_source_manifest.json). No source wiki
revision was pinned, so new revisions should trigger a fresh comparison.
Terrain, precise distance, conditions and reset/acquisition success are still
unmeasured. A list without another same-type entry does not exclude adjacent
biomes, fixed spawns or source omissions.
