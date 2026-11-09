# Mine/Roark checkpoint: route evidence

This sidecar preserves the reasoning that previously accompanied the C++ row. The JSON defines current actions; these observations explain why they were chosen. They are historical evidence, not new console verification. Entry assumes the preceding Barry conversation and Center heal have completed.

## Entry and Center alignment

The Center door is `(47,20)`, so its approach is `(47,21)` facing north. The published `pokemon_center_doorstep` is `(46,21)`: a route there hit the wall west of the door and healing found no door (lab-B, 2026-11-07 20:18). Both `(47,21)` and `(46,21)` facing south remain exit alternatives. Earlier Gym alignment runs r217d/r218d inferred column 46 from twenty west taps; they did not independently establish where those taps began.

`above_the_mine` goes to `(46,57)` facing south, two tiles short of the warp row so an open walk cannot slide onto it (01.05, lab-B 2026-11-08). The city's five warp tiles `(44..48,59)` map into the hall at `(11,1),(11,1),(12,1),(13,1),(13,1)`. Thus column 46 enters B1F at `(12,1)`. The mine hall is Diamond's B1F; the coal room below is the room with Roark. Diamond zone-event sources were narc_0044/narc_0197; Platinum's event files provide the corresponding warp layout, used as evidence for BDSP's retained DP layout.

## Into the coal room

`into_the_mine` and `into_the_coal_room` cross the cave mouths. Hall movement uses counted taps rather than an open hold toward a fade: a wild battle on a hold can be mistaken for the transition. Warp entry itself is expected on the subsequent step. These are executor assumptions that still need detector/interruption coverage.

The hall's coal-room mouth is `(11..13,21)`; column 12 enters the coal room at `(15,2)`. `south_to_the_rock` clips the corridor rock before the grouped inbound legs. The script's anchor is `(15,9)` facing south; the rock was described at `(15,11)`.

## Roark approach and talk

`to_roarks_corner` groups all nine inbound legs through `(20,28)` facing west, rather than ending recovery at an open intermediate tile. Lab-B, 2026-11-08 11:55: recovery replanned only to `(15,16)`, a subsequent south-five leg overshot a row, and later taps ran under Roark's rocks. Grouping through a corner allows a replanned path to finish with wall clips. The offline interrupted-leg case deliberately stops with uncertainty; it does not prove live recovery/rejoin.

`beside_roarks_rock` clips the room's south wall to `(20,29)`, then `below_roark` moves west two to `(18,29)` facing north. Lab-B, 2026-11-07: an open counted south-three leg moved only two, motivating the wall anchor. Roark's map point is `(18,28)`; native tests check the authored below-Roark pose against that map point.

`roark` initiates the conversation. Barry's earlier message that Roark went to the mine is a prerequisite: Roark ignored A without it in r101/r104. `roark_smashes_the_rock` represents the lines, Rock Smash east of him, and his departure east, consistent with Platinum's mine B2F script. The declared return pose is an executor fallback, not an observed receipt proving the scene completed.

## Back through the hall

`below_the_coal_rooms_warp` contains the ten outbound legs: east around the crate, north along column 29, west across row 17, then up the corridor to `(15,2)` facing north. Entering `(15,1)` returns to the hall at `(12,21)`, not `(11,21)`.

`up_to_the_hall` and `out_of_the_mine` continue up column 12. The hall's `(12,1)` exit leads to city `(46,59)`. The city belief also permits `(46,58)` if movement continued past the fade. Lab-B, 2026-11-08 09:46, 10:39 and 12:03: the exit was east of the sign `(45,56)` and northward movement did not stop there. At 13:50, a north tap and east wall hold matched the map only from column 46.

## Return to the nurse

`north_to_the_centers_wall` clips the wall at `(46,20)` from either exit tile: 37 tiles from row 58 or 38 from row 59. The Hiker wanders through columns 46..48 and rows 25..27; the live walker must wait or recover safely, and the offline model uses an explicit synthetic clearance window.

This column avoids the street east of the mine. At 13:50, an east hold from `(48,57)` moved zero although the map showed `(49,57)` open; later movement went astray. That discrepancy remains unresolved and is not grounds to change terrain silently.

`east_below_the_centers_door` moves one tile east to `(47,21)` facing north. `heal_oreburgh` enters, heals and exits to the two-tile doorstep belief. The next Gym checkpoint starts there. The checkpoint keeps save-at-start/reload-and-retry guarded by the Oreburgh City save card.

## Updating this route

Edit the authoritative JSON, update relevant native/scenario expectations, and preserve new run evidence here. A changed map pin requires anchor review. A claimed transition or recovery fix needs an independent observation/regression case; do not turn `declared_after` into proof of success. The initial migration was checked against the pre-migration row and the reported successful lab-B sequence of 2026-11-08 14:17–14:23; later changes must record their own evidence.
