"""Writes FRLG overworld maps as JSON, from the pret/pokefirered decompilation.

TODO: move the maps to PokemonAutomation/Packages (Resources/PokemonFRLG/Maps) when ready.

Usage:
    python3 devtools/maps/frlg_maps.py --out SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/Maps \
        CeruleanCity_PokemonCenter_1F CeruleanCity Route24 \
        CeruleanCave_1F CeruleanCave_2F CeruleanCave_B1F
    python3 devtools/maps/frlg_maps.py --out <folder>
    python3 devtools/maps/frlg_maps.py --out <folder> --check

Downloads each map's data/maps/<map>/map.json, its layout's map.bin and its
tilesets' metatile_attributes.bin at --commit into --cache, then writes
<out>/<map>.json in the schema of Pokemon::MapData (Pokemon_MapData.h).
Warps and connections lead only to maps in the same run: a warp to another map
has no destination, and a connection to one is left out. A warp arrives at
point "warp_<n>" of its map, the tile of that map's warp n. Other named points
and flag sets come from POINTS and FLAG_SETS below.

Without map names, the maps are the ones already in --out. --check writes
nothing: it regenerates them and fails if any file differs. The program
builds in the files of SerialPrograms/Source/VideoGames/PokemonFRLG/
Overworld/Maps, the folder to write and check.
"""
import argparse
import json
import os
import re
import struct
import subprocess
import sys

COMMIT = 'ce6406d8c2eaf46d543c8a2606de0c3cb1951982'
#   When COMMIT was pinned, for each map's "source".
COMMIT_DATE = '2026-10-31'

#   Behaviors with a glyph of their own (Pokemon_MapData.h has the legend).
TERRAIN = {
    'MB_TALL_GRASS': '~', 'MB_WARP_DOOR': 'D',
    'MB_IMPASSABLE_NORTH': '8', 'MB_IMPASSABLE_SOUTH': '2',
    'MB_IMPASSABLE_EAST': '6', 'MB_IMPASSABLE_WEST': '4',
}
#   Open tiles whose warp is taken other than by walking into it: how.
WARP_ENTRIES = {
    'MB_CAVE_DOOR': 'cave_entrance',
    'MB_LADDER': 'step_on', 'MB_UP_ESCALATOR': 'step_on', 'MB_DOWN_ESCALATOR': 'step_on',
    'MB_SOUTH_ARROW_WARP': 'walk_south', 'MB_NORTH_ARROW_WARP': 'walk_north',
    'MB_EAST_ARROW_WARP': 'walk_east', 'MB_WEST_ARROW_WARP': 'walk_west',
    #   Stairs at the side of a room warp like an exit mat, the way the stairs
    #   go up or down (IsDirectionalStairWarpMetatileBehavior()).
    'MB_UP_RIGHT_STAIR_WARP': 'walk_east', 'MB_DOWN_RIGHT_STAIR_WARP': 'walk_east',
    'MB_UP_LEFT_STAIR_WARP': 'walk_west', 'MB_DOWN_LEFT_STAIR_WARP': 'walk_west',
}
#   Ledges, by the way the player hops over them.
LEDGES = {'MB_JUMP_NORTH': '^', 'MB_JUMP_SOUTH': 'v', 'MB_JUMP_WEST': '<', 'MB_JUMP_EAST': '>'}
FLOOR = ('MB_NORMAL', 'MB_CAVE', 'MB_ROCK_STAIRS', 'MB_TALL_GRASS')
DIRECTIONS = {'up': 'north', 'down': 'south', 'left': 'west', 'right': 'east'}

#   Places and flag sets the decompilation doesn't name, written into the map
#   they belong to.
#   A `*_front` point is where the player stands to talk to someone across a
#   counter, or to walk north through a door.
POINTS = {
    #   Where a new game starts (WarpToPlayersRoom() in src/new_game.c).
    'PalletTown_PlayersHouse_2F': {'start': (6, 6)},
    'PalletTown': {'oaks_lab_front': (16, 14)},
    #   The clerk is across the counter to the west.
    'ViridianCity_Mart': {'clerk_front': (4, 3)},
    'ViridianCity_PokemonCenter_1F': {'nurse_front': (7, 4)},
    'PewterCity': {'gym_front': (15, 17)},
    'PewterCity_PokemonCenter_1F': {'nurse_front': (7, 4)},
    'PewterCity_Gym': {'brock_front': (6, 6)},
    #   The tile in front of the PC.
    'CeruleanCity_PokemonCenter_1F': {'pc': (11, 2), 'nurse_front': (7, 4)},
    #   Where the player stands after entering from Cerulean City.
    'CeruleanCave_1F': {'entrance': (33, 21)},
    #   Mewtwo's tile. Its battle starts by talking to it from the south.
    'CeruleanCave_B1F': {'mewtwo': (7, 12)},
}
FLAG_SETS = {
    #   Set once the player has entered the Hall of Fame. They clear the way
    #   to Cerulean Cave: its guard, the rival and the Rocket.
    'CeruleanCity': {'after_hall_of_fame': [
        'FLAG_HIDE_CERULEAN_CAVE_GUARD',
        'FLAG_HIDE_CERULEAN_RIVAL',
        'FLAG_HIDE_CERULEAN_ROCKET',
    ]},
}


class Decomp:
    def __init__(self, commit, cache):
        self.raw = 'https://raw.githubusercontent.com/pret/pokefirered/' + commit
        self.cache = os.path.join(cache, commit)

    def path(self, rel):
        local = os.path.join(self.cache, rel)
        if not os.path.exists(local):
            os.makedirs(os.path.dirname(local), exist_ok=True)
            subprocess.run(['curl', '-sfL', f'{self.raw}/{rel}', '-o', local], check=True)
        return local

    def read(self, rel):
        with open(self.path(rel), 'rb') as file:
            return file.read()


def behaviors(decomp):
    names = {}
    for line in decomp.read('include/constants/metatile_behaviors.h').decode().splitlines():
        match = re.match(r'#define (MB_\w+)\s+(0x[0-9A-Fa-f]+|\d+)', line)
        if match:
            names[int(match.group(2), 0)] = match.group(1)
    return names


def tileset_attributes(decomp, symbol, kind):
    name = re.sub(r'([a-z])([A-Z0-9])', r'\1_\2', symbol.replace('gTileset_', '')).lower()
    data = decomp.read(f'data/tilesets/{kind}/{name}/metatile_attributes.bin')
    return [struct.unpack_from('<I', data, i)[0] for i in range(0, len(data), 4)]


#   Signs the player reads by walking into them moving north or south
#   (TrySetUpWalkIntoSignpostScript() in src/field_control_avatar.c). A
#   signpost needs a sign event on its tile; the others don't.
SIGN_BEHAVIORS = ('MB_POKEMON_CENTER_SIGN', 'MB_POKEMART_SIGN', 'MB_INDIGO_PLATEAU_SIGN_1', 'MB_INDIGO_PLATEAU_SIGN_2')


def tile_glyph(behavior, collision, unhandled, sign_event=False):
    """The tile's glyph, and how its warp (if it has one) is entered: None for
    walking into it, the default."""
    if behavior in SIGN_BEHAVIORS or (behavior == 'MB_SIGNPOST' and sign_event):
        return 'S', None
    if behavior == 'MB_WARP_DOOR':
        return 'D', None    # doors have collision
    if behavior in LEDGES:
        return LEDGES[behavior], None
    if collision:
        return '#', None
    if behavior in WARP_ENTRIES:
        return '.', WARP_ENTRIES[behavior]
    if behavior in TERRAIN:
        return TERRAIN[behavior], None
    if 'WATER' in behavior:
        return 'w', None
    if behavior not in FLOOR:
        unhandled[behavior] = unhandled.get(behavior, 0) + 1
    return '.', None


def grid(decomp, layout, names, unhandled, signs=frozenset()):
    """The map's tile rows, elevation rows, and how the warp on each tile is
    entered, by (x, y), where it isn't by walking into it."""
    width, height = layout['width'], layout['height']
    blocks = decomp.read(layout['blockdata_filepath'])
    primary = tileset_attributes(decomp, layout['primary_tileset'], 'primary')
    secondary = tileset_attributes(decomp, layout['secondary_tileset'], 'secondary')
    tiles, elevation, entries = [], [], {}
    for y in range(height):
        row, levels = '', ''
        for x in range(width):
            value = struct.unpack_from('<H', blocks, 2 * (y * width + x))[0]
            metatile, collision, level = value & 0x3ff, (value >> 10) & 3, value >> 12
            attributes = primary[metatile] if metatile < 640 else secondary[metatile - 640]
            behavior = names.get(attributes & 0x1ff, hex(attributes & 0x1ff))
            glyph, entry = tile_glyph(behavior, collision, unhandled, (x, y) in signs)
            row += glyph
            if entry:
                entries[(x, y)] = entry
            levels += '%x' % level
        tiles.append(row)
        elevation.append(levels)
    return tiles, elevation, entries


def wander_box(obj, tiles):
    """The tiles a wandering person may stand on, kept inside the map, or None
    if they stand still."""
    movement = obj.get('movement_type', '')
    if 'WANDER' not in movement:
        return None
    x = 0 if 'UP_AND_DOWN' in movement else int(obj.get('movement_range_x') or 0)
    y = 0 if 'LEFT_AND_RIGHT' in movement else int(obj.get('movement_range_y') or 0)
    if x == 0 and y == 0:
        return None
    return {
        'x0': max(obj['x'] - x, 0), 'y0': max(obj['y'] - y, 0),
        'x1': min(obj['x'] + x, len(tiles[0]) - 1), 'y1': min(obj['y'] + y, len(tiles) - 1),
    }


def map_object(obj, tiles):
    kind = obj['graphics_id'].replace('OBJ_EVENT_GFX_', '').lower()
    data = {'name': kind, 'kind': 'item' if kind == 'item_ball' else 'npc', 'x': obj['x'], 'y': obj['y']}
    box = wander_box(obj, tiles)
    if box:
        data['wander'] = box
    flag = obj.get('flag') or '0'
    if flag != '0':
        data['hide_flag'] = flag
    return data


#   Tiles the player never walks onto: their elevation only matters for doors,
#   which the player is put on by a warp and leaves at once.
UNWALKABLE = '#^v<>DS'
GROUND = '3'


def ground_only(tiles, elevation):
    """Whether every walkable tile is at ground level, so that the map can
    leave out its elevation grid (the loader reads ground everywhere)."""
    return all(level == GROUND for row, levels in zip(tiles, elevation)
               for tile, level in zip(row, levels) if tile not in UNWALKABLE)


def warp_point(index):
    return f'warp_{index}'


def warp(event, entries, loaded, sources):
    """A warp event as MapData's warp: how it is entered, and the point it
    leads to, if its map is loaded and has that warp."""
    data = {'x': event['x'], 'y': event['y'], 'enter': entries.get((event['x'], event['y']), 'walk_into')}
    to_map, index = loaded.get(event['dest_map']), int(event['dest_warp_id'])
    if to_map is not None and index < len(sources[to_map].get('warp_events', [])):
        data['to_map'] = to_map
        data['to_point'] = warp_point(index)
    return data


def arrivals(sources, loaded):
    """For each map, the indices of its warps that a warp leads to."""
    arriving = {name: set() for name in sources}
    for source in sources.values():
        for event in source.get('warp_events', []):
            to_map = loaded.get(event['dest_map'])
            if to_map is not None and int(event['dest_warp_id']) < len(sources[to_map].get('warp_events', [])):
                arriving[to_map].add(int(event['dest_warp_id']))
    return arriving


def map_json(decomp, name, sources, layouts, names, loaded, unhandled):
    source = sources[name]
    signs = frozenset((e['x'], e['y']) for e in source.get('bg_events', []) if e.get('type') == 'sign')
    tiles, elevation, entries = grid(decomp, layouts[source['layout']], names, unhandled, signs)
    warp_events = source.get('warp_events', [])
    points = {n: [x, y] for n, (x, y) in POINTS.get(name, {}).items()}
    for index in sorted(arrivals(sources, loaded)[name]):
        points[warp_point(index)] = [warp_events[index]['x'], warp_events[index]['y']]
    data = {
        'name': name,
        'source': {'decomp_file': f'data/maps/{name}/map.json', 'revision': COMMIT, 'date': COMMIT_DATE},
        'tiles': tiles, 'elevation': elevation,
        'points': points,
        'warps': [warp(event, entries, loaded, sources) for event in warp_events],
        #   A few objects stand off the map, where no route meets them.
        'objects': [
            map_object(obj, tiles) for obj in source.get('object_events', [])
            if 0 <= obj['x'] < len(tiles[0]) and 0 <= obj['y'] < len(tiles)
        ],
        'flag_sets': FLAG_SETS.get(name, {}),
        'connections': [
            {'side': DIRECTIONS[c['direction']], 'to_map': loaded[c['map']], 'offset': c['offset']}
            for c in (source.get('connections') or []) if c['map'] in loaded
        ],
    }
    if ground_only(tiles, elevation):
        del data['elevation']
    return data


def json_text(data):
    text = json.dumps(data, indent=4)
    #   A point's [x, y] on one line.
    return re.sub(r'\[\s*(-?\d+),\s*(-?\d+)\s*\]', r'[\1, \2]', text) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--commit', default=COMMIT)
    parser.add_argument('--cache', default=os.path.expanduser('~/.cache/pokefirered'))
    parser.add_argument('--out', required=True, help='folder of the <map>.json files')
    parser.add_argument('--check', action='store_true', help='check the files in --out instead of writing them')
    parser.add_argument('maps', nargs='*', help='default: the maps already in --out')
    args = parser.parse_args()
    if not args.maps and os.path.isdir(args.out):
        args.maps = sorted(f[:-5] for f in os.listdir(args.out) if f.endswith('.json'))
    if not args.maps:
        parser.error('no maps given, and none in --out')

    decomp = Decomp(args.commit, args.cache)
    names = behaviors(decomp)
    layouts = {l['id']: l for l in json.loads(decomp.read('data/layouts/layouts.json'))['layouts'] if 'id' in l}
    sources = {m: json.loads(decomp.read(f'data/maps/{m}/map.json')) for m in args.maps}
    loaded = {source['id']: m for m, source in sources.items()}
    unhandled = {}
    maps = [(name, json_text(map_json(decomp, name, sources, layouts, names, loaded, unhandled))) for name in args.maps]
    if unhandled:
        print(f'Non-collision behaviors written as floor: {unhandled}')
    if args.check:
        differ = []
        for name, text in maps:
            path = os.path.join(args.out, name + '.json')
            if not os.path.exists(path) or open(path, newline='').read() != text:
                differ.append(name)
        if differ:
            sys.exit(f'Differ from the generator: {" ".join(differ)}. Regenerate them without --check.')
        print(f'{len(maps)} maps in {args.out} match the generator.')
        return
    os.makedirs(args.out, exist_ok=True)
    for name, text in maps:
        with open(os.path.join(args.out, name + '.json'), 'w', newline='') as out:
            out.write(text)


if __name__ == '__main__':
    main()
