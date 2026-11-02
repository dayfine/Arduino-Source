"""Writes BDSP AutoStory's maps as JSON, from the Pokemon Diamond decompilation.

TODO: move the maps to PokemonAutomation/Packages (Resources/PokemonBDSP/Maps) when ready.

Usage:
    python3 devtools/maps/bdsp_maps.py                  # rewrite every map in OUT
    python3 devtools/maps/bdsp_maps.py Route202         # rewrite one
    python3 devtools/maps/bdsp_maps.py --check          # regenerate, fail on any difference

Each map is a rectangle of the decompilation's world grid (MAPS below),
decoded tile for tile by devtools/maps/decomp.py from the files cached in
~/.cache/maps/decomp (`python3 -m devtools.maps decomp` fetches missing ones
with `gh api`; this script never fetches, it names the file it lacks). BDSP
keeps Diamond/Pearl's layout, so Diamond is read unless a map says otherwise.
The tables below add what the decompilation doesn't say or BDSP changed:

- POINTS: named tiles the story walks from and to. A region (a set of
  tiles, in order where the order matters) is written as numbered points,
  `<region>.1`, `<region>.2`, ...: the shared schema (Pokemon_MapData.h)
  has single-tile points only.
- OBJECTS: which of the events file's objects BDSP shows. People hidden by
  a story flag are left out, and people who stand still or wander are kept,
  unless an entry here says otherwise (`show`, `hide`, `still`, `at`), or
  adds someone the console found that Diamond doesn't have.
- WARP_DOORS: warp tiles written 'D' so that no plan steps onto them; the
  other warp tiles are written '.', since the story arrives on them.
- WARPS: doors that lead to a point of another map in OUT.
- PATCHES: tiles measured on the console that differ from Diamond's, with
  the run that measured them. The map's tiles stay the decompilation's; the
  loader applies the patches.

The legend is the schema's: '#' wall, '.' open, '~' tall grass, 'w' water,
'^' 'v' '<' '>' ledges hopped the arrow's way, 'D' a door or cave mouth
(walked into) or a warp tile in WARP_DOORS, 'S' a sign or mailbox (read by
facing it; a sign script on a wall too). Items on the ground are objects of
kind "item"; rocks, cut trees, berry soil and steam vents are '#'.

Rules: terrain from the decomp, people and BDSP's edits from the console;
a route is coded only over tiles a human or a probe has walked; a map change
re-runs every checkpoint that walks it. Bulbapedia's DP images (12 px rows
for 16 px columns, cliffs painted tall) are for drawing routes over, not for
terrain. Scenes' coord events have no place in the schema (Segment00Lines).
"""
import argparse
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from devtools.maps import decomp  # noqa: E402

OUT = 'SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Maps'
CACHE = Path.home() / '.cache' / 'maps' / 'decomp'
#   When the cache was filled from the decompilations' default branches.
READ = '2026-10-31'

LEDGES = {'north': '^', 'south': 'v', 'west': '<', 'east': '>'}
SIGN_GFX = ('MAILBOX',)
OBSTACLE_GFX = ('CUT_TREE', 'ROCK_SMASH', 'STRENGTH_BOULDER', 'BERRY_SOIL', 'VENT')
FACING = {'LOOK_NORTH': 'north', 'LOOK_SOUTH': 'south', 'LOOK_WEST': 'west', 'LOOK_EAST': 'east'}


#   Decoded from pokeplatinum at this revision, not yet checked on the console.
INTERIOR = {'repo': decomp.Platinum.REPO, 'revision': 'c6689696d5fb', 'read': '2026-11-01', 'unverified': True}

#   Each map: its header constant, the world rectangle (x0, z0, width,
#   height), and the decompilation when not Diamond. Local (x, y) = world
#   (x - x0, z - z0). A map that joins another on the world grid keeps its
#   rectangle's world corner, so story code can place them side by side.
MAPS = {
    #   Cells (3..4, 26) from z 839: the image-traced grid's columns and the
    #   game's rows. Console: the border sign read from (54, 4) facing east,
    #   row 3 open east of the grass, row 5 to Sandgem without a stop, the
    #   Potion lady on row 3 (2026-10-28 to 30, CFR "walk").
    'Route201': {'header': 'ROUTE_201', 'rect': (96, 839, 64, 25)},
    #   Cell (5, 25) from x 159. Console: under the south ledge at (22, 24),
    #   the save spot (30, 24) against the east trees, Natalie's challenge at
    #   (22, 22), column 30 north to row 4 (r75), the catch spot's walls.
    'Route202': {'header': 'ROUTE_202', 'rect': (159, 800, 36, 32)},
    #   Cells (5..6, 23) from x 191: column 0 is Jubilife's east edge.
    #   Console (r227 hand pass, 2026-10-31): the Gate's warp at column 55,
    #   the ledge on row 15, the girl at (43, 17), row 18 straight west to
    #   Jubilife (35 taps). Diamond has four sand rows under the ledge where
    #   the console counted three (the first tap may only have turned).
    'Route203': {'header': 'ROUTE_203', 'rect': (191, 740, 61, 24)},
    #   Cell (5, 21). Console: row 27 open under the cliff, the stairs,
    #   Taylor's sight at column 17 (human walk, 2026-10-31). Suspects:
    #   column 23 rows 12..19 blocked (r242a); three open rows under the cave,
    #   not four (r241a, r243a).
    'Route204North': {'header': 'ROUTE_204_NORTH', 'rect': (160, 672, 32, 32)},
    #   Cell (5, 22). Console: north up column 14 stops at the signpost
    #   (14, 17); column 13 is grass to the cliff (13, 2); Sarah walked up and
    #   fought on row 21 (r233a, r234a). 01.08 passed once (r235a).
    'Route204South': {'header': 'ROUTE_204_SOUTH', 'rect': (160, 704, 32, 32), 'read': '2026-11-01'},
    #   Cell (5, 26). Console (2026-10-29/30): the three doors, five signs,
    #   the doors' front row, the Lab's sign, the stop above the Mart (29, 5).
    'SandgemTown': {'header': 'SANDGEM', 'rect': (160, 832, 34, 32)},
    #   Cells (4..5, 23..24). Console (2026-10-30/31): the lamps (43, 53) and
    #   (49, 53), north from (52, 53) to the Center's door, the School's door
    #   from the Center's doorstep, the east street to Route 203 (r230a), the
    #   north street, and the 00.05 people on their events-file tiles.
    'JubilifeCity': {'header': 'JUBLIFE', 'rect': (128, 736, 64, 64)},
    #   Cells (8..9, 23..24); Platinum's map_data_022/023/175/024 are the
    #   same terrain. Console (r204 to r215): 20 D-pad taps from the
    #   Center's doorstep (46, 21) to below the Gym's door.
    'OreburghCity': {'header': 'OREBURGH', 'rect': (256, 736, 64, 64)},
    #   The only cell of matrix 003; Platinum identical.
    'OreburghGate': {'header': 'OREBURGH_GATE_1F', 'rect': (0, 0, 32, 32)},
    #   The only cell of matrix 113. 01.04 walks north up column 5.
    'OreburghGym': {'header': 'OREBURGH_GYM', 'rect': (0, 0, 32, 32)},
    #   B2F, the coal room: matrix 006. Console (2026-10-28, r99): the
    #   corridor's rocks zigzag a row at a time, the room's second row passes
    #   under the worker, Roark a row above the bottom between two rocks; the
    #   walk north into the hall went up column 14 (the warp is column 15: a
    #   stick hold drifts, or BDSP's stairs are wider).
    'OreburghMine': {'header': 'OREBURGH_MINE_B2F', 'rect': (0, 0, 32, 32)},
    #   Platinum's matrix 051 (Diamond's matrix is not in the cache), rows
    #   0..53. Console (2026-10-31, r235b and D-pad probes): in from the south
    #   door onto (19, 50), up the stairs (18..20, 45..47) to the wall at row
    #   39, the corridor three wide to row 41 and open east on rows 42..44,
    #   the rocks (22, 42), (22, 43), (23, 44) and the Potion (22, 44). Rows
    #   0..29 were a hand trace from the DP image before; nothing walks there.
    'RavagedPath': {'header': 'RAVAGED_PATH', 'rect': (0, 0, 32, 54), 'repo': decomp.Platinum.REPO},
    #   Cell (5, 20); the cell above carries the same header but is cut off
    #   from the town. Console: the old couple (2026-10-31 walk, r251a).
    'FloaromaTown': {'header': 'FLOAROMA', 'rect': (160, 640, 32, 32)},
    #   Cell (3, 27). The player's house is the south-east one, door
    #   (20, 21); Barry's the north-west one, door (9, 11) (the interiors say
    #   so; Diamond's map constants name them the other way round).
    'TwinleafTown': {'header': 'TWINLEAF', 'rect': (96, 864, 32, 32)},
    #   Cell (2, 26), of the four the header covers (the others are trees
    #   and water). The ledges on column 20 (rows 14..18) are hopped west.
    'VerityLakefront': {'header': 'VERITY_LAKEFRONT', 'rect': (64, 832, 32, 32)},
    #   Cell (1, 1) of the lake's matrix 101: the shore where the starter
    #   scene plays. Only this cell is reached on foot.
    'LakeVerity': {'header': 'LAKE_VERITY', 'rect': (32, 32, 32, 32)},
    #   00.01 to 00.03's interiors, from pokeplatinum (c6689696d5fb), each
    #   the only cell of its own matrix: decomp only, not yet checked on the
    #   console. Mom's warning on the way out is a coord event on the
    #   player's door tile (6, 10), which the schema has no place for.
    'PlayerHouse1F': {'header': 'TWINLEAF_TOWN_PLAYER_HOUSE_1F', 'rect': (0, 0, 13, 12), **INTERIOR},
    'PlayerHouse2F': {'header': 'TWINLEAF_TOWN_PLAYER_HOUSE_2F', 'rect': (0, 0, 12, 12), **INTERIOR},
    'RivalHouse1F': {'header': 'TWINLEAF_TOWN_RIVAL_HOUSE_1F', 'rect': (0, 0, 13, 12), **INTERIOR},
    'RivalHouse2F': {'header': 'TWINLEAF_TOWN_RIVAL_HOUSE_2F', 'rect': (0, 0, 12, 12), **INTERIOR},
    'RowansLab': {'header': 'SANDGEM_TOWN_POKEMON_RESEARCH_LAB', 'rect': (0, 0, 22, 18), **INTERIOR},
}


def box(x0, y0, x1, y1):
    """Every tile of a box, both corners inclusive, row by row."""
    return [(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)]


#   Named tiles; a list is a region, written as numbered points.
POINTS = {
    'Route201': {
        #   The arrow sign to Sandgem. Console (CFR "walk"): east along row 4
        #   stops at its west side and A reads it.
        'border_sign': (55, 4),
        #   The Poke Mart lady (10 Potions): Diamond spawns her at (44, 3), the
        #   console had her at (43, 3) and walking west along row 3 stopped at
        #   (44, 3), facing her (99715d5d6, f823453e9; r135a missed her on row 4).
        'potion_giver': (43, 3),
        'potion_giver_talk_from': (44, 3),
        #   Segment 00's lines (Segment00Lines).
        'route_201_arrival': (15, 24),
        'barry_warning': (15, 18),
        'route_201_return': (0, 14),
        'rowan_stop': (5, 14),
        'route_201_east_end': (63, 5),
    },
    'Route202': {
        #   00.03's save after the catch: the catch patch's bottom row. The
        #   sign (24, 25) is 5 right and 2 up; south are trees (r253).
        'catch_spot': (19, 27),
        #   00.04's save before the trainers, under the south ledge against
        #   the east trees (r113a..r117a).
        'save_spot': (30, 24),
        'trainer_tristan': (7, 13),
        'trainer_natalie': (22, 18),
        'trainer_logan': (26, 4),
        #   The middle of the gap to Jubilife, below its 3-wide neck.
        'exit_jubilife': (15, 0),
        #   The open rows at the top: north from any of them is Jubilife.
        'jubilife_exit': box(11, 0, 18, 3),
        #   Where 00.04b's save after the catch may stand: the patch and the
        #   lawn column east of it, down to the pocket where r252 saved.
        'catch_save_area': box(15, 25, 21, 27) + [(21, 28), (21, 29)],
        #   Where training may leave us (the catch spot's column and one either
        #   side, between the tree rows), and its grass: all of it.
        'after_training_area': box(18, 25, 20, 27),
        'catch_grass': box(18, 25, 20, 27),
    },
    'Route203': {
        #   The warp tile the Gate puts us on, and its open neighbours.
        'gate_exit': [(55, 9), (54, 9), (55, 8), (55, 10)],
        #   Row 18 between the cliffs: it runs straight west into Jubilife.
        'west_line': box(24, 18, 42, 18),
    },
    'Route204North': {
        'cave_exit': (20, 26),
        #   The line a human walked on 2026-10-31: out of the cave, west under
        #   the cliff, up the stairs, east (Taylor), north up column 19, west
        #   below the flower beds, north between them on column 11 ((12, 6)
        #   is blocked on the console).
        'floaroma_waypoints': [(20, 27), (11, 27), (11, 22), (19, 22), (19, 12), (11, 12), (11, 5)],
        #   The path north into Floaroma: walking north from here leaves.
        'floaroma_path': box(10, 0, 12, 5),
        'trainer_twin_left': (15, 7),
        'trainer_twin_right': (16, 7),
        'trainer_taylor': (17, 20),
        'trainer_brandon': (20, 12),
    },
    'Route204South': {
        'below_door': (11, 2),
        #   Where the walk north from Jubilife may stop, up to two tiles past
        #   the border (400 ms holds between looks).
        'jubilife_entry': [(14, 31), (14, 30), (14, 29)],
        'trainer_samantha': (11, 8),
        'trainer_sarah': (12, 21),
        'trainer_tyler': (7, 18),
    },
    'SandgemTown': {
        #   The walk south from Route 202 down the road's east side stops here,
        #   above the Mart where the road narrows (console, 2026-10-29/30).
        'route_202_entry_stop': (29, 5),
        'lab_door': (8, 10),
        'pokemon_center_door': (17, 10),
        #   Walking north from row 14 in its column stops below it.
        'pokemon_center_sign': (20, 10),
        'poke_mart_door': (27, 10),
        #   East along the doors' front row stops here, at the trees (CFR
        #   "walk", 2026-10-30): a known tile to plan north from.
        'front_row_east_end': (29, 11),
        #   The front row below the gap between the Center's and the Mart's
        #   signs: the only way north from that row that passes no door.
        'below_sign_gap': box(21, 11, 24, 11),
        'sandgem_arrival': (0, 12),
        'dawn_stop': (4, 12),
    },
    'JubilifeCity': {
        #   A doorway (warp tile) stepped onto from (40, 41).
        'trainers_school_door': (40, 40),
        #   Walked into from (52, 41); eleven taps west from its doorstep put
        #   us under the School's door (2026-10-30).
        'pokemon_center_door': (52, 40),
        'tv_station_door': (36, 15),
        #   Talked to from the east (also works from the south).
        'poketch_president': (46, 35),
        #   His spawn tile; Diamond has him wander a tile either side, the
        #   console had him a tile east of it.
        'clown_mart': (55, 33),
        'clown_tv_station': (36, 16),
        #   Talked to from the south; west along row 18 stops beside him.
        'clown_poketch_company': (15, 18),
        #   A tile west of 00.05's save below the steps, then north: on
        #   column 46 or 47 a few rows up (CFR "walk", 2026-10-30).
        'up_the_steps': box(46, 58, 47, 60),
        #   Where Route 203's road enters, rows 21..24 at the east edge.
        'east_entrance': box(61, 21, 63, 24),
        #   The north road where it leaves the plaza: north from here goes
        #   up the road to Route 204 and Rowan's scene.
        'north_road': box(44, 16, 48, 20),
    },
    'OreburghCity': {
        'pokemon_center_door': (47, 20),
        #   The taps say x 46, a column west of the door.
        'pokemon_center_doorstep': (46, 21),
        'gym_door': (26, 20),
        'gym_doorstep': (26, 21),
        'gate_mouth': (1, 13),
    },
    'OreburghGym': {
        #   Coming in puts us here; the door is below.
        'entrance': (5, 23),
        'roark': (5, 3),
        'trainer_first_tier': (4, 18),
        'trainer_second_tier': (7, 11),
    },
    'OreburghMine': {
        'roark': (18, 28),
        'corridor_exit_start': (14, 10),
        #   The first open row below the warp tile, where the walk in from
        #   the hall leaves us.
        'corridor_top': box(14, 2, 16, 2),
    },
    'RavagedPath': {
        'entrance': (19, 50),
        'east_end': (29, 44),
        'above_exit': (28, 44),
        #   Rock Smash rocks, open once smashed.
        'rocks': [(11, 36), (7, 37), (12, 37), (8, 38), (10, 38), (10, 39), (13, 39),
                  (7, 40), (9, 40), (17, 40), (12, 41), (22, 42), (22, 43), (23, 44)],
        #   West of the two rocks on rows 42 and 43.
        'before_rocks': [(21, 42), (21, 43)],
    },
    'FloaromaTown': {
        'route_204_arrival': (11, 29),
        'old_woman': (3, 25),
        'old_man': (2, 25),
        'pokemon_center_door': (16, 26),
    },
    'TwinleafTown': {
        'house_doorstep': (20, 22),
        #   Barry's scene fires here, below his door.
        'barry_doorstep': (9, 12),
        'twinleaf_north_exit': (15, 0),
        'player_house_door': (20, 21),
        'rival_house_door': (9, 11),
    },
    'VerityLakefront': {
        'lakefront_arrival': (31, 23),
        'below_lake_entrance': (17, 14),
        #   The lake's scene fires here; it is assumed to leave us here too.
        'lake_entrance': (17, 12),
    },
    'PlayerHouse1F': {'stairs': (10, 3), 'mom': (7, 8), 'door': (6, 10)},
    #   A new game starts on (4, 6); the TV is the signs at (3, 4) and (4, 4).
    'PlayerHouse2F': {'stairs': (8, 4), 'start': (4, 6)},
    'RivalHouse1F': {'door': (6, 10), 'stairs': (2, 3)},
    #   Barry stands at (9, 5) facing his PC; he is talked to from the west.
    'RivalHouse2F': {'stairs': (3, 4), 'barry_talk': (8, 5)},
    #   Rowan is talked to from below, facing north.
    'RowansLab': {'door': (7, 15), 'rowan_talk': (7, 6)},
}


#   Overrides by events-file object index. `show`: a person hidden by a
#   story flag who is there when the story walks the map. `hide`: someone
#   BDSP doesn't have, or the story never meets. `still`: a wanderer who
#   stood still on the console. `at`: where the console found them. `add`:
#   people Diamond doesn't have, from the console.
OBJECTS = {
    'Route201': {
        #   The Potion lady stands a tile off her spawn on the console, and
        #   her talk is walked to, not planned (POINTS).
        1: {'hide': 'her talk is walked to, not planned'},
    },
    'Route203': {
        #   Stands still in Diamond, wanders near the Gate in BDSP (console).
        0: {'hide': 'wanders in BDSP'},
    },
    'Route204North': {
        #   Bug Catcher Brandon paces in Diamond from (22, 13); he stopped us
        #   on row 12 (r246a), and the DP map draws him at (20, 12).
        0: {'at': (20, 12), 'still': 'r246a', 'name': 'bug_catcher_brandon'},
    },
    'SandgemTown': {
        #   Stand still in Diamond, wander on the console.
        1: {'hide': 'wanders in BDSP'},
        2: {'hide': 'wanders in BDSP'},
    },
    'JubilifeCity': {
        #   00.05's story people, hidden before and after it.
        24: {'show': '00.05', 'name': 'poketch_president'},
        28: {'show': '00.05', 'name': 'clown_tv_station'},
        #   Wander in Diamond; stood still on the console (00.05, three runs).
        25: {'show': '00.05', 'still': '00.05, three runs', 'name': 'clown_mart'},
        26: {'show': '00.05', 'still': '00.05, three runs', 'name': 'clown_poketch_company'},
    },
    'FloaromaTown': {
        #   Grunts at the Meadow until the Valley Windworks is cleared, and
        #   the man with Cyrus's sprite (whether he is there is for the console).
        7: {'show': 'until the Valley Windworks'},
        8: {'show': 'until the Valley Windworks'},
        3: {'show': 'not yet checked on the console'},
        'add': [
            #   Not in Diamond's data. Walking north from (3, 29) stopped 3
            #   tiles up at the old woman (r251a).
            {'name': 'old_woman', 'x': 3, 'y': 25, 'measured_by': 'r251a, 2026-10-31'},
            {'name': 'old_man', 'x': 2, 'y': 25, 'measured_by': 'r251a, 2026-10-31'},
        ],
    },
    'OreburghMine': {
        #   Hidden once his scene here is over; the story walks to him.
        0: {'show': '01.03', 'name': 'roark'},
    },
    'RivalHouse2F': {
        #   Barry in his room, until he leaves for the lake.
        0: {'show': '00.01', 'name': 'barry'},
    },
    'RowansLab': {
        #   Rowan at his desk when the story walks in.
        0: {'show': '00.03', 'name': 'rowan'},
    },
    'LakeVerity': {
        #   The starter scene's actors, where the walk in ends.
        0: {'show': 'the starter scene', 'name': 'rowan'},
        1: {'show': 'the starter scene', 'name': 'counterpart'},
        3: {'show': 'the starter scene'},
    },
}


WARP_DOORS = {
    #   The Ravaged Path's door: its warp tile is the mouth.
    'Route204South': [(11, 1)],
    #   The Route 218 gate, and the doorways of the School and the Global
    #   Terminal: entered, never stood on by a route.
    'JubilifeCity': [(0, 22), (0, 23), (40, 40), (21, 42)],
    #   The way up to the entrance hall: stepping on it leaves the room.
    'OreburghMine': [(15, 1)],
}


#   Stepping onto (x, y) puts the player at a point of another map.
WARPS = {
    'PlayerHouse1F': [(6, 10, 'TwinleafTown', 'player_house_door')],
    'RivalHouse1F': [(6, 10, 'TwinleafTown', 'rival_house_door')],
    'RowansLab': [(7, 15, 'SandgemTown', 'lab_door')],
}


PATCHES = {
    'Route202': (
        #   BDSP has trees where Diamond's catch patch has its two bottom rows:
        #   west along row 29 stopped at column 21, beside them.
        [(x, y, '#', 'r253, 2026-11-01') for y in (28, 29) for x in range(15, 21)]
        #   South from the pocket (21, 29), where the catch's save loads,
        #   was blocked at once.
        + [(21, 30, '#', 'r254, 2026-11-01')]
    ),
    'Route204North': [
        #   Grass in the data, blocked on the console twice.
        (12, 6, '#', 'r248a, 2026-10-31'),
    ],
}


def no_fetch(repo, path):
    raise SystemExit(f'{repo}/{path} is not in {CACHE}. Fetch it with `python3 -m devtools.maps decomp ...` first.')


def glyph(attr):
    behaviour = attr & 0xff
    if behaviour in decomp.JUMP:
        return LEDGES[decomp.JUMP[behaviour]]
    if behaviour in decomp.JUMP_TWICE:
        return LEDGES[decomp.JUMP_TWICE[behaviour]]
    if behaviour == decomp.DOOR or behaviour in decomp.CAVE_MOUTHS:
        return 'D'
    if behaviour in decomp.WARP_TILES or behaviour in decomp.WARP_STAIRS:
        return 'W'
    if behaviour in decomp.WATER:
        return 'w'
    if attr & decomp.BLOCKED:
        return '#'
    if behaviour in decomp.TALL_GRASS:
        return '~'
    return '.'


def wander_box(obj, x, y, width, height):
    return {'x0': max(0, x - obj.range_x), 'y0': max(0, y - obj.range_z),
            'x1': min(width - 1, x + obj.range_x), 'y1': min(height - 1, y + obj.range_z)}


def place_objects(name, events, rect, rows):
    """Writes signs and obstacles into `rows`; returns the objects."""
    x0, z0, width, height = rect
    overrides = OBJECTS.get(name, {})
    objects = []
    for index, obj in enumerate(events.objects):
        x, y = obj.x - x0, obj.z - z0
        if not (0 <= x < width and 0 <= y < height):
            continue
        override = overrides.get(index, {})
        if obj.gfx in OBSTACLE_GFX:
            rows[y][x] = '#'
            continue
        if obj.gfx == decomp.ITEM:
            objects.append({'name': f'item_{index}', 'kind': 'item', 'x': x, 'y': y})
            continue
        if obj.hidden and 'show' not in override or 'hide' in override:
            continue
        if 'SIGN' in obj.gfx or obj.gfx in SIGN_GFX:
            rows[y][x] = 'S'
            continue
        x, y = override.get('at', (x, y))
        entry = {'name': override.get('name', f'{obj.gfx.lower()}_{index}'), 'kind': 'npc', 'x': x, 'y': y}
        if obj.movement in FACING:
            entry['facing'] = FACING[obj.movement]
        if obj.movement.startswith(('WANDER', 'WALK')) and 'still' not in override:
            entry['wander'] = wander_box(obj, x, y, width, height)
        objects.append(entry)
    for added in overrides.get('add', []):
        objects.append({'name': added['name'], 'kind': 'npc', 'x': added['x'], 'y': added['y']})
    return objects


def decode_map(name):
    spec = MAPS[name]
    repo = spec.get('repo', decomp.Diamond.REPO)
    game = decomp.game_for(repo, CACHE, None, no_fetch)
    header = game.header(spec['header'])
    matrix = game.matrix(header.matrix_id)
    world = decomp.World(game, matrix)
    x0, z0, width, height = spec['rect']
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            attr = world.attr(x0 + x, z0 + y)
            if attr is None:
                raise SystemExit(f'{name}: ({x}, {y}) is outside matrix {header.matrix_id}.')
            row.append(glyph(attr))
        rows.append(row)
    events = game.events(header.events_id)
    for sign in events.signs:
        x, y = sign.x - x0, sign.z - z0
        if 0 <= x < width and 0 <= y < height and rows[y][x] == '#':
            rows[y][x] = 'S'
    objects = place_objects(name, events, spec['rect'], rows)
    doors = set(WARP_DOORS.get(name, []))
    for y, row in enumerate(rows):
        for x, tile in enumerate(row):
            if tile == 'W':
                row[x] = 'D' if (x, y) in doors else '.'
    lands = sorted({world.land_id(x, z) for z in range(z0, z0 + height, decomp.CELL)
                    for x in range(x0, x0 + width, decomp.CELL)} - {None})
    unverified = '; decomp only, not checked on the console' if spec.get('unverified') else ''
    source = {
        'decomp_file': f'{repo}: ' + ', '.join(game.land_path(land) for land in lands)
                       + f'; {game.events_name(header.events_id)}{unverified}',
        'revision': spec.get('revision', 'default branch'),
        'date': spec.get('read', READ),
    }
    return [''.join(row) for row in rows], objects, source


def points_of(name):
    points = {}
    for point, value in POINTS.get(name, {}).items():
        if isinstance(value, list):
            for i, pos in enumerate(value, 1):
                points[f'{point}.{i}'] = pos
        else:
            points[point] = value
    return points


def map_text(name):
    """The map's JSON: one line per row, point, object and patch."""
    tiles, objects, source = decode_map(name)
    lines = ['{', f'    "name": {json.dumps(name)},', f'    "source": {json.dumps(source)},', '    "tiles": [']
    lines += [f'        {json.dumps(row)},' for row in tiles]
    lines[-1] = lines[-1].rstrip(',')
    lines.append('    ]')

    def section(key, items):
        if not items:
            return
        lines[-1] += ','
        lines.append(f'    "{key}": ' + ('{' if key == 'points' else '['))
        lines.extend(f'        {item},' for item in items)
        lines[-1] = lines[-1].rstrip(',')
        lines.append('    ' + ('}' if key == 'points' else ']'))

    section('points', [f'{json.dumps(p)}: [{x}, {y}]' for p, (x, y) in points_of(name).items()])
    section('warps', [json.dumps({'x': x, 'y': y, 'to_map': m, 'to_point': p}) for x, y, m, p in WARPS.get(name, [])])
    section('objects', [json.dumps(o) for o in objects])
    section('patches', [json.dumps({'x': x, 'y': y, 'glyph': g, 'measured_by': m})
                        for x, y, g, m in PATCHES.get(name, [])])
    lines.append('}')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--out', default=OUT, help='folder of the <map>.json files')
    parser.add_argument('--check', action='store_true', help='check the files in --out instead of writing them')
    parser.add_argument('maps', nargs='*', help='default: every map in MAPS')
    args = parser.parse_args()
    names = args.maps or sorted(MAPS)
    for name in names:
        if name not in MAPS:
            parser.error(f'{name}: not in MAPS')
    texts = {name: map_text(name) for name in names}
    if args.check:
        differ = [n for n, text in texts.items()
                  if not os.path.exists(os.path.join(args.out, n + '.json'))
                  or open(os.path.join(args.out, n + '.json'), newline='').read() != text]
        if not args.maps:
            extra = sorted(f[:-5] for f in os.listdir(args.out) if f.endswith('.json') and f[:-5] not in MAPS)
            differ += [f'{n} (not in MAPS)' for n in extra]
        if differ:
            sys.exit(f'Differ from the generator: {", ".join(differ)}. Regenerate them without --check.')
        print(f'{len(texts)} maps in {args.out} match the generator.')
        return
    os.makedirs(args.out, exist_ok=True)
    for name, text in texts.items():
        with open(os.path.join(args.out, name + '.json'), 'w', newline='') as file:
            file.write(text)
    print(f'Wrote {len(texts)} maps to {args.out}.')


if __name__ == '__main__':
    main()
