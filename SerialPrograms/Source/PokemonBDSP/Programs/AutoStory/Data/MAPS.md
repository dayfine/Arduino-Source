# AutoStory map grids: source and status

Each map file's header starts with the same three lines, kept current with the code:

- **Map source**: where the tiles come from. "Decomp" means the Pokemon Platinum
  decompilation's collision table (pret/pokeplatinum `map_data_NNN.bin`) with the
  objects from its events file; BDSP keeps Diamond/Pearl's layout, and Platinum's
  few changes show up on the console. "DP image" means a Bulbapedia map image
  traced by hand or by colour; those images have 12 px rows for 16 px columns, so
  a traced grid has 3/4 of a tile per row and drifts by the map's height
  differences (cliffs are painted taller than their tiles).
- **Console checks**: the anchors measured on the Switch (D-pad taps, sign reads,
  recorded human walks), with the run or date.
- **Status**: the checkpoints that pass on this exact grid, and the next change.

| map | source | checkpoints passing | next |
|---|---|---|---|
| Route 201 | DP image (3/4 rows) | 00.02-00.04 | re-derive from the decomp |
| Route 202 | DP image (3/4 rows) | 00.04 | re-derive from the decomp |
| Sandgem Town | DP image rows 0-6, decomp rows 7+ | 00.04, 00.05 | two tiles on row 6; row-2 narrowing |
| Jubilife City | DP image (3/4 rows), console NPCs | 00.05, 01.01, 01.06, 01.07 | re-derive from the decomp |
| Oreburgh Mine B2F | DP image (stretched), console | 01.02, 01.03 (3x) | row 18 open, west ledge, top corridor row |
| Route 203 | DP image, east stretch from the console | 01.06 | re-derive the west and middle stretches |
| Route 204 South | decomp | 01.08 (1x) | more passes |
| Ravaged Path | decomp rows 30-53 | 01.09 by hand | program pass |
| Route 204 North | decomp | 01.10 (1x) | probe column 23; (12,6) marked from the console |
| Oreburgh City | decomp (pokediamond) | none yet (01.04, 01.05 run as records) | probe the Center doorstep (x 46 or 47) |
| Oreburgh Gate | decomp (pokediamond) | none yet | |
| Floaroma Town | decomp (pokediamond), the old couple from the console | 01.11 (1x, as a record) | |
| Twinleaf Town | decomp (pokediamond) | none: 00.01 to 00.03 replay recorded stick inputs (#1717), no route on the grid | redo 00.01 to 00.03 as planned walks |
| Verity Lakefront, Lake Verity | decomp (pokediamond) | none: 00.02, 00.03 replay recorded stick inputs (#1717) | redo as planned walks |

Rules: terrain from the decomp, people and Platinum-only edits from the console;
a route is coded only over tiles a human or a probe has walked; a map change
re-runs every checkpoint listed on it. The decomp comparison that found the 3/4
rows is the 2026-10-31 report (`compare.md`, with the decoded grids), to be
checked in with the re-derivation.
