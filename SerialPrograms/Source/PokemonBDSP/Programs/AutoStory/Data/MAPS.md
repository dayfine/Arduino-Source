# AutoStory map grids: source and status

Each map file's header starts with the same three lines, kept current with the code:

- **Map source**: where the tiles come from. "Decomp" means a decompilation's
  collision table (pret/pokediamond `land_data` narc, or pret/pokeplatinum
  `map_data_NNN.bin`) with the objects from its events file; BDSP keeps
  Diamond/Pearl's layout, and Platinum's few changes show up on the console. "DP image" means a Bulbapedia map image
  traced by hand or by colour; those images have 12 px rows for 16 px columns, so
  a traced grid has 3/4 of a tile per row and drifts by the map's height
  differences (cliffs are painted taller than their tiles).
- **Console checks**: the anchors measured on the Switch (D-pad taps, sign reads,
  recorded human walks), with the run or date.
- **Status**: the checkpoints that pass on this exact grid, and the next change.

| map | source | checkpoints passing | next |
|---|---|---|---|
| Route 201 | decomp (Diamond) | none yet (00.02-00.04 and mart_lady passed on the image grid) | re-run them |
| Route 202 | decomp (Diamond) | none yet (00.04 passed on the image grid) | re-run 00.04; the save-spot legs grew three rows |
| Sandgem Town | decomp (Diamond) | none yet (00.04, 00.05 passed with image rows 0-6) | re-run; the Route 202 entry stop is (29,1) |
| Jubilife City | decomp (Diamond), console NPCs | none yet (00.05, 01.01, 01.06, 01.07 passed on the image grid) | re-run; re-anchor the Mart clown and the steps walk |
| Oreburgh Mine B2F | decomp (Diamond) | none yet (01.02, 01.03 passed 3x on the stretched image grid) | re-run both |
| Route 203 | decomp (Diamond) | none yet (01.06 passed on the image grid) | re-run; the row to Jubilife is 18, four sand rows |
| Route 204 South | decomp | 01.08 (1x) | more passes |
| Ravaged Path | decomp rows 30-53 | 01.09 by hand | program pass |
| Route 204 North | decomp | none (01.10) | probe column 23 and the rows under the cave |

Rules: terrain from the decomp, people and Platinum-only edits from the console;
a route is coded only over tiles a human or a probe has walked; a map change
re-runs every checkpoint listed on it. The decomp comparison that found the 3/4
rows is the 2026-10-31 report (`compare.md`, with the decoded grids); the
re-derivation of 2026-10-31 took every grid above from `devtools/maps decomp
--repo pret/pokediamond` and translated each file's named tiles with its
console anchors.
