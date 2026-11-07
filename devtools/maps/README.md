# Map tools for AutoStory routes

Offline helpers that give a route planner its tile grid (standard library
only, Python 3.8+): a decoder for the Pokemon Platinum and Diamond
decompilations, an overlay that draws a grid and a run's routes over the map
image, and a colour parser for the few maps no decompilation covers.

## Decomp first

A map grid has two kinds of facts, from two sources:

- **Terrain** (walls, tall grass, ledges, doors, warps) comes from the
  decompilation: `decomp` below decodes it tile for tile. BDSP keeps
  Diamond/Pearl's layout; at every anchor probed so far the decoded grid
  matched the console.
- **People and the few BDSP differences** come from the console: trainers who
  wander in BDSP but stand still in Platinum, a tile blocked by a prop BDSP
  added, a sign a row off. These are hand edits, written down in the data
  file's header with the run that found them.

A route is coded only over tiles a human or a probe has walked. Bulbapedia's
DP map images are what humans read, so routes are still drawn and checked on
them, but they are no longer traced for terrain: their rows are 12 px to the
columns' 16 (3/4 of a tile per row) and cliffs are painted taller than their
tiles, so no single offset fixes a traced grid.

### `decomp`: the grid and objects of one map

```sh
python3 -m devtools.maps decomp ROUTE_204_NORTH                 # C++ rows and header comment
python3 -m devtools.maps decomp ROUTE_204_NORTH --text          # bare rows
python3 -m devtools.maps decomp RAVAGED_PATH                    # a cave: its own matrix
python3 -m devtools.maps decomp ROUTE_204_NORTH --rect 160 660 32 46   # any world rectangle
python3 -m devtools.maps decomp ROUTE_204_NORTH --repo pret/pokediamond
```

The map is named by its header constant (`MAP_HEADER_<name>` in pokeplatinum,
`MAP_<name>` in pokediamond). Files are fetched once with
`gh api repos/<repo>/contents/<path>` into `~/.cache/maps/decomp/<repo>/`
(`--cache` moves it, `--local DIR` reads a checkout instead). What is read:

- `include/data/map_headers.h` (Platinum) or `arm9/src/map_header.c` and
  `include/constants/maps.h` (Diamond): the map's matrix and events file.
- the matrix, `res/field/matrices/map_matrix_NNN.json` or
  `files/fielddata/mapmatrix/map_matrix/narc_NNNN.bin`: 32 x 32 cells, the
  overworld's is 30 x 30 (matrix 000); caves have their own (the Ravaged Path
  is matrix 051, two cells tall). World x = 32 * column + lx, z = 32 * row + lz.
  Without `--rect`, the rectangle is the cell(s) carrying the map's header.
- land data per cell, `res/field/maps/data/map_data_NNN.bin` or
  `files/fielddata/land_data/land_data_release/narc_NNNN.bin`: a 16-byte header
  of four u32 sizes, then 1024 little-endian u16 attributes, row-major,
  index z * 32 + x. Bit 0x8000 blocks the tile; the low byte is the behaviour:
  0x02 tall grass `"`; 0x38..0x3b jump east/west/north/south, where only the
  south ledge has a legend character (`v`), the others are `#` with a note;
  0x69 door and 0x62..0x65 cave mouths (blocked, walked into) `D`; 0x6c..0x6f
  warp tiles (stood on) and 0x5e/0x5f warp stairs `W`; water `#`; cave floor
  0x08, sand 0x21 and anything else unblocked `.`.
- events, `res/field/events/events_<map>.json` or
  `files/fielddata/eventdata/zone_event_release/narc_NNNN.bin`: standing people
  `N`, signposts `S`, items on the ground and cut trees, smashable rocks and
  boulders `#`; a wanderer (`WANDER_*`, `WALK_*` movement) is left off the grid
  and listed with its range, the walker waits for him. Warp events are listed
  with their destination (a cave mouth's sits on the arrival tile in front of
  it). Trainers come with their facing and sight range. Diamond's files give
  these by number; pokeplatinum's `generated/*.txt` tables name them (the
  numbering is shared: Route 204 North's twelve objects come out the same).

The C++ output is a header comment ready to paste, then the rows:

```
//  Map source: the decomp (map_data_031, 2026-10-31) with the objects from
//      its events file.
//  Console checks: none yet.
//  Status: no checkpoint passes on this grid yet.
//
//  Decoded from Pokemon Platinum's collision table for this map (the
//  pret/pokeplatinum decompilation, map_data_031: one cell of the Sinnoh
//  matrix at column 5, row 21), with the objects from its events file
//  (events_route_204_north.json); BDSP keeps Diamond/Pearl's layout. Local
//  (x, y) = world (x - 160, z - 672); 32 x 32 tiles.
//  Water and the items on the ground are '#'. Objects, from the events file:
//    warp tile (20, 26) to Ravaged Path;
//    Aroma Lady Taylor (17, 20), looks south and east, sees 4;
//    Twin Liv (15, 7), looks south, sees 3;
//    ...
//    Bug Catcher Brandon walk back and forth from (22, 13), columns 18 to 26,
//    row 13, sees 1: off the grid;
    //     0         1         2         3
        R"(########.#...###################)",  //   0
        R"(########.#...###################)",  //   1
        ...
```

The first three lines are the annotation `Data/MAPS.md` asks every map file to
keep current; fill in the console checks and status as runs happen. Against
`PokemonBDSP_AutoStory_Route204.cpp` the decoded rows differ only where that
file's header records a hand edit (Brandon placed where the console met him,
the cave's arrival tile kept open, a tile blocked on the console). Platinum
and Diamond decode Route 204 North identically.

Platinum moved a few things after Diamond/Pearl; where the two decompilations
disagree, `--repo pret/pokediamond` is the one BDSP follows.

## Overlay: a grid, and a run's routes, over the map image

Map images stay out of the repo; `fetch` gets one from
`https://archives.bulbagarden.net/media/upload/<h0>/<h0h1>/<File>`, where `h`
is the md5 of the file name (e.g. `Sinnoh_Route_204_DP.png`), into
`~/.cache/maps`. `overlay` draws a navigation data file's grid over the map
image it was read from, as SVG, and optionally the walks a console run planned
and walked:

```sh
python3 -m devtools.maps fetch Sandgem_Town_DP.png
python3 -m devtools.maps overlay path/to/Data/..._SandgemTown.cpp -o sandgem.svg
python3 -m devtools.maps overlay path/to/Data/..._SandgemTown.cpp \
    --log "$HOME/Library/Application Support/SerialPrograms/Profiles/<profile>/SerialPrograms.log" \
    --last 3 -o sandgem-run.svg
python3 -m devtools.maps decomp ROUTE_204_NORTH --text > r204n.txt
python3 -m devtools.maps overlay r204n.txt --image ~/.cache/maps/Sinnoh_Route_204_DP.png \
    --origin 0,0 --row-px 12 --level 12:24 -o r204n.svg
python3 -m unittest discover -s devtools/maps/tests -t .
```

The source is a C++ data file (its `R"(...)"` rows, `const TilePos NAME{x, y};`
tiles, and from its header the image `(Name.png, W x H px)` and where tile
(0, 0) starts, `starts 8 px in from the left edge`) or a text file of bare
rows such as `decomp --text` prints, with `--image` and `--origin X,Y` given.
Walls are red, tall grass green, ledges purple, signs orange, doors blue,
every fifth row and column numbered. With `--log`, a leg planned "to wall"
that stopped on anything but `blocked` is marked yellow: the grid has a wall
where the game doesn't.

A decomp grid over a DP image needs the image's geometry: `--row-px 12` for
its 12 px rows, and `--level ROW:PX` for each cliff, meaning rows from `ROW`
down sit `PX` lower than the rows above (about 24 px per level; repeat the
option per cliff, reading the rows off the picture). Town maps drawn at 16 px
rows need neither.

## Routes: a run's walks, planned and walked, on the JSON maps

`routes` reads a SerialPrograms log (or a `Lab/runs/<run_id>/` run record,
from its `log.txt`), finds every AutoStory route walk of both games, and draws
each on the maps the walkers plan on (the JSON files: FRLG's
`PokemonFRLG/Overworld/Maps/`, BDSP's `PokemonBDSP/.../AutoStory/Maps/`):

```sh
python3 -m devtools.maps routes SerialPrograms.log --list
python3 -m devtools.maps routes SerialPrograms.log --run -1 \
    --maps SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/Maps \
    --maps SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Maps -o routes/
```

Per run it writes `routes/<run_id>/`, with `index.html` showing, in order:

- **per checkpoint attempt, one picture per map** (`map_NN_<checkpoint>_a<attempt>_<map>`)
  joining all the walks of that attempt on that map, e.g. the whole Viridian
  Forest of 00.07's first attempt, each walk in its own colour, numbered at
  its start, and listed in a legend. Walks over the same tiles lie side by side;
- **one picture per walk and map** (`NNN_<event or label>_<map>`).

Each picture is cropped to the walked area plus four tiles and zoomed to fit
(up to 40 px a tile), so a four-tile walk is as easy to see as a long one.
`trace.json` has the walks as data (plans, steps with where they started and
ended and how that is known, and marks), the same shape for both games.

How a line is drawn says how it is known:

- **solid**: walked as planned, or seen (BDSP's stop and arrival tiles); as
  wide as the times that tile edge was walked, so back and forth is a fat line;
- **pale and striped, labelled "assumed after battle"** (or message, dialog,
  stall): something interrupted the leg and the walker went on taking it to
  have ended as planned (FRLG walks to the wall again; BDSP counts the battle);
- **orange dotted, labelled**: the tile isn't known: an FRLG leg stopped by a
  battle, the hold that found the player again, a step the walk failed on;
- **dashed, beside the walked line**: the plan (a BDSP replan in purple).

Badges sit at a tile's corner so they don't hide the line: S (or the walk's
number) the start, E the plan's end, A an arrival, R where the player was
found again, B a wild battle, M a message, T a trainer, a red X the failure.
Standard library only: the PNG is drawn by `picture.py`.

What each walker logs, and so what can be drawn: both games' walkers write
the shared route log lines (`Pokemon/Navigation/Pokemon_RouteLog.h`, lifted
from BDSP's walker): the plan from one tile or a set, each leg as it starts
and where it ended (walked, assumed or unknown), replans, where the player
was found again, trainers and the arrival. `route_trace.py` reads those, and
the older lines below for older logs:

- **FRLG** logs each step as it starts (`Route: walk north 8 -> Route1 (12, 21)`),
  how a leg was interrupted (`Leg: a wild battle after 1253ms, stopping the
  leg.`) and the re-anchor (`Route: stopped part-way, finding the player
  again at ...; <holds>`). Where the walker also logs the plan up front
  (`Route: plan, N steps: ...`), the plan is drawn whole even when the walk
  fails; otherwise a walk is one story event's steps, its plan those less the
  holds and the way back. A walk's start is worked back from its first step,
  or is where the last walk ended (after a warp). Only the last hold of a
  re-anchor ends on a known tile, so the others aren't drawn.
- **BDSP** logs the plan from a set of start tiles with its legs, each leg as
  it starts, how it ended (`blocked`, `limit`), where it stopped and replanned
  from, and where it arrived. The picture follows the first tile of each set.

## Maps without a decomp: `levels.py`

For a map the decompilations don't cover, `levels.py` reads walkable tiles off
the image by colour, level by level. Towns and caves have terraces and ledges
bordered by rock rims, joined only by stairs; judging each 16 px tile by its
colours alone walks a route off one level onto another wherever a rim stripe
is thin, so it works at pixel resolution first:

1. Each pixel is walkable (paving, road, grass, curb), stairs, or blocked
   (rims, roofs, trees, water).
2. Walkable pixels are flood-filled into regions; a rim of any width keeps
   two levels apart.
3. Each blob of stair pixels joins the regions it touches.
4. Each tile takes the region holding at least half its pixels. A step is
   allowed between tiles of the same region, or onto and off a stair tile
   that touches both.
5. A breadth-first search gives the route; the output lists its corners.

```sh
swiftc -O png2bmp.swift -o png2bmp     # sips mangles palette PNGs
./png2bmp Veilstone_City_DP.png veil.bmp
python3 levels.py veil.bmp out/veil --offset 8,9 \
    --stairs 704,290 --stairs 512,527 --stairs 80,665 ...   # one pixel per staircase
    --from 605,474 --to 82,470                              # pixel points
# writes out/veil_regions.bmp, out/veil_grid.txt, out/veil_route.txt
swiftc -O draw_route.swift -o draw_route
./draw_route Veilstone_City_DP.png route.png "$(head -1 out/veil_route.txt)"
```

Options for harder maps:

- `--profile town|sandtown|oreburgh|cave`: colour rules. `oreburgh` counts the
  conveyor belts as ground (we walk under them; the pillars block).
  `sandtown` adds dirt roads and sand (Sandgem) to `town`.
- `--height H`: stretch a squashed image back to H pixels tall. Oreburgh
  Mine B2F is 512x395 for a 512x512 map: its rocks are 16 px apart across
  and 12.5 px down.
- `--objects on`: rocks, boulders, crates and people standing on the floor
  block the tile under their feet, even when drawn smaller than a tile or
  off the grid.
- `--edits FILE`: tiles opened or blocked from what the console showed,
  `open ROW COL0 COL1` / `block ROW COL0 COL1`; edits apply before objects.
  E.g. rows the coal block's edge is drawn over are walkable, and people
  touching the coal block are not found as objects.

`examples/` keeps the grids levels.py read for Veilstone City, Oreburgh City
and Oreburgh Mine B2F, with the mine's edits file, as text. They were traced
from DP images before the decomp was in use, so they carry the 3/4-row
drift; take the terrain of those maps from `decomp` and use these only to
see what the colour rules make of a town.

Check per map:

- **Tile alignment** (`--offset`): not always (0, 0). For Veilstone it is
  (8, 9), read from the edges of the three-tile stairs at the bottom.
- **Colours**: the rules are for Sinnoh towns. Caves need their own.
- **Stairs** are given by hand, one pixel inside each.
- **Not modelled:** one-way ledges and people. Static NPCs are added from
  what the console shows.
- **Row height**: a DP image's rows are 3/4 of a tile; a traced grid drifts
  by the map's height differences. Prefer the decomp wherever it has the map.

Sandgem Town, levels.py against the console-checked grid: with `sandtown`
and `--objects on`, rows 0..7 agree but for two lit door fronts read as
objects (we stood on (17, 7)) and the Lab's front, (6..9, 7), read as blocked
(walked on the console). Lawns, roads and the beach come out as separate
regions with no ledge between them, and the image squeezes rows 7..18 into
7..15, so the south half is taken from the game's collision instead.

## Map, route, walk: getting a route to work on the console

A route that fails on the console has failed in one of three layers. Find
which one before changing anything, and fix that layer:

| Layer | What it is | How it is checked |
| --- | --- | --- |
| Map | the tile grid: walls, signs, doors, exits, NPCs | terrain from the decomp; people and BDSP's differences by measuring on the console |
| Route | the legs a checkpoint walks | offline, by the plan tests, before any console run |
| Walk | holding the stick until something stops us | console runs; the route absorbs its error |

- **Map**: a leg planned "to wall" ran out of time, or went through a
  transition, or stopped where the grid has no wall. Measure that place
  again; don't tune the walker.
- **Route**: the plan used something it must not, such as an exit as a wall or
  a long open leg. Add the plan test that would have caught it.
- **Walk**: map and plan are right, but a hold moved a different number of
  tiles, or someone stood in the way. End more legs on walls, or add a
  re-anchor.

### Measuring a map

Decode the grid first; the console then confirms its anchors and finds the
people. Measure with the Command File Runner's `walk x y`, which holds the
stick until the screen stops changing and prints how long it moved. Don't use
timed `stick` holds for this: on BDSP the same 300 ms hold moved one row on
one try and two on the next. Write down:

- borders and exits: where each edge stops you or takes you elsewhere;
- anchors: tiles a walk can prove it stands on (two walls, a door, the tile
  beside a sign), and how each is proven;
- distances between anchors, as `walk` measures them;
- objects: the row and column of each sign, NPC and door relative to the walls
  around it, which sides it can be read or talked from, and whether the tiles
  next to it are open.

Decide rows by stops and sign reads, never by eye: at the overworld camera's
angle an NPC on your row looks a row higher, and a sign a row below the trees
can look right under them. Mark exits (steps, walkable edges) `D`, never `#`;
a `#` there is a wall the planner will walk "to", and off the map. Put what
was measured in the data file's header ("Console checks: ...").

### Planning the route

- Start and end each checkpoint on an anchor, and save there, so a reload
  starts exactly there; its first event checks the anchor and re-anchors if
  the check fails.
- End every leg against something: a wall, a sign, an NPC who answers. Keep
  open legs to a tile or two, and follow each with a leg that shows whether it
  took.
- Out and back can take different rows; pick each direction's legs for the
  walls they end on.
- A sign is a gate: walk until it stops you and press A. If it doesn't open,
  you are not on its row.
- Check every planned walk offline, and draw it with `overlay` before the
  first console run.
