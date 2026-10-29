# Map tools for AutoStory routes

Offline helpers that turn Bulbapedia's DP map images into something a route
planner can use. Map images stay out of the repo; fetch them from
`https://archives.bulbagarden.net/media/upload/<h0>/<h0h1>/<File>`, where `h` is
the md5 of the file name (e.g. `Veilstone_City_DP.png`).

## `levels.py`: walkable tiles, by level

Towns and caves have levels: terraces and ledges bordered by rock rims, joined
only by stairs. Judging each 16 px tile by its colours alone walks a route off
one level onto another wherever a rim stripe is thin, so `levels.py` works at
pixel resolution first:

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

- `--profile town|oreburgh|cave`: colour rules. `oreburgh` counts the
  conveyor belts as ground (we walk under them; the pillars block).
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

`examples/` has Veilstone City's levels and two routes to the Gym (from the
Pokémon Center and from Route 215). Both reach the Gym's terrace only by its
south-west stairs. Oreburgh: the Pokémon Center to the mine is one column
straight south (checked on the console), and Mine B2F's route to Roark
with its edits file.

Check per map:

- **Tile alignment** (`--offset`): not always (0, 0). For Veilstone it is
  (8, 9), read from the edges of the three-tile stairs at the bottom.
- **Colours**: the rules are for Sinnoh towns. Caves need their own.
- **Stairs** are given by hand, one pixel inside each.
- **Not modelled:** one-way ledges and people. Static NPCs are added from
  what the console shows.
