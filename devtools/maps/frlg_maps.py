"""Writes FRLG overworld maps as JSON, from the pret/pokefirered decompilation.

Usage:
    python3 devtools/maps/frlg_maps.py --out Packages/Resources/PokemonFRLG/Maps \
        CeruleanCity_PokemonCenter_1F CeruleanCity Route24 \
        CeruleanCave_1F CeruleanCave_2F CeruleanCave_B1F

Downloads each map's data/maps/<map>/map.json, its layout's map.bin and its
tilesets' metatile_attributes.bin at --commit into --cache, then writes
<out>/<map>.json in the schema of Packages/Resources/PokemonFRLG/Maps/README.md.
Warps and connections lead only to maps in the same run: others get an empty
dest_map, or are left out. Named points and flag sets come from POINTS and
FLAG_SETS below.
"""
import argparse
import json
import os
import re
import struct
import subprocess

COMMIT = 'ce6406d8c2eaf46d543c8a2606de0c3cb1951982'

#   Behaviors with their own tile character; see the README for the legend.
TERRAIN = {
    'MB_TALL_GRASS': '"', 'MB_WARP_DOOR': 'D', 'MB_CAVE_DOOR': 'C',
    'MB_LADDER': 'L', 'MB_UP_ESCALATOR': 'L', 'MB_DOWN_ESCALATOR': 'L',
    'MB_SOUTH_ARROW_WARP': 'v', 'MB_NORTH_ARROW_WARP': '^',
    'MB_EAST_ARROW_WARP': '>', 'MB_WEST_ARROW_WARP': '<',
    #   Stairs at the side of a room warp like an exit mat, the way the stairs
    #   go up or down (IsDirectionalStairWarpMetatileBehavior()).
    'MB_UP_RIGHT_STAIR_WARP': '>', 'MB_DOWN_RIGHT_STAIR_WARP': '>',
    'MB_UP_LEFT_STAIR_WARP': '<', 'MB_DOWN_LEFT_STAIR_WARP': '<',
    'MB_IMPASSABLE_NORTH': 'n', 'MB_IMPASSABLE_SOUTH': 's',
    'MB_IMPASSABLE_EAST': 'e', 'MB_IMPASSABLE_WEST': 'w',
}
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


def tile_character(behavior, collision, unhandled, sign_event=False):
    if behavior in SIGN_BEHAVIORS or (behavior == 'MB_SIGNPOST' and sign_event):
        return 'S'
    if behavior == 'MB_WARP_DOOR':
        return 'D'      # doors have collision
    if behavior.startswith('MB_JUMP_'):
        return 'J'      # ledges: hopped down, never stopped at
    if collision:
        return '#'
    if behavior in TERRAIN:
        return TERRAIN[behavior]
    if 'WATER' in behavior:
        return '~'
    if behavior not in FLOOR:
        unhandled[behavior] = unhandled.get(behavior, 0) + 1
    return '.'


def grid(decomp, layout, names, unhandled, signs=frozenset()):
    width, height = layout['width'], layout['height']
    blocks = decomp.read(layout['blockdata_filepath'])
    primary = tileset_attributes(decomp, layout['primary_tileset'], 'primary')
    secondary = tileset_attributes(decomp, layout['secondary_tileset'], 'secondary')
    tiles, elevation = [], []
    for y in range(height):
        row, levels = '', ''
        for x in range(width):
            value = struct.unpack_from('<H', blocks, 2 * (y * width + x))[0]
            metatile, collision, level = value & 0x3ff, (value >> 10) & 3, value >> 12
            attributes = primary[metatile] if metatile < 640 else secondary[metatile - 640]
            behavior = names.get(attributes & 0x1ff, hex(attributes & 0x1ff))
            row += tile_character(behavior, collision, unhandled, (x, y) in signs)
            levels += '%x' % level
        tiles.append(row)
        elevation.append(levels)
    return width, height, tiles, elevation


def wander(obj):
    movement = obj.get('movement_type', '')
    if 'WANDER' not in movement:
        return 0, 0
    x = 0 if 'UP_AND_DOWN' in movement else int(obj.get('movement_range_x') or 0)
    y = 0 if 'LEFT_AND_RIGHT' in movement else int(obj.get('movement_range_y') or 0)
    return x, y


def map_json(decomp, name, layouts, names, loaded, unhandled):
    source = json.loads(decomp.read(f'data/maps/{name}/map.json'))
    signs = frozenset((e['x'], e['y']) for e in source.get('bg_events', []) if e.get('type') == 'sign')
    width, height, tiles, elevation = grid(decomp, layouts[source['layout']], names, unhandled, signs)
    objects = []
    for obj in source.get('object_events', []):
        flag = obj.get('flag') or ''
        wander_x, wander_y = wander(obj)
        objects.append({
            'x': obj['x'], 'y': obj['y'],
            'kind': obj['graphics_id'].replace('OBJ_EVENT_GFX_', '').lower(),
            'flag': '' if flag == '0' else flag,
            'wander_x': wander_x, 'wander_y': wander_y,
        })
    return {
        'name': name, 'width': width, 'height': height,
        'tiles': tiles, 'elevation': elevation,
        'warps': [
            {'x': w['x'], 'y': w['y'], 'dest_map': loaded.get(w['dest_map'], ''), 'dest_warp': int(w['dest_warp_id'])}
            for w in source.get('warp_events', [])
        ],
        'objects': objects,
        'connections': [
            {'direction': DIRECTIONS[c['direction']], 'offset': c['offset'], 'map': loaded[c['map']]}
            for c in (source.get('connections') or []) if c['map'] in loaded
        ],
        'points': [{'name': n, 'x': x, 'y': y} for n, (x, y) in POINTS.get(name, {}).items()],
        'flag_sets': [{'name': n, 'flags': flags} for n, flags in FLAG_SETS.get(name, {}).items()],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--commit', default=COMMIT)
    parser.add_argument('--cache', default=os.path.expanduser('~/.cache/pokefirered'))
    parser.add_argument('--out', required=True)
    parser.add_argument('maps', nargs='+')
    args = parser.parse_args()

    decomp = Decomp(args.commit, args.cache)
    names = behaviors(decomp)
    layouts = {l['id']: l for l in json.loads(decomp.read('data/layouts/layouts.json'))['layouts'] if 'id' in l}
    loaded = {json.loads(decomp.read(f'data/maps/{m}/map.json'))['id']: m for m in args.maps}
    unhandled = {}
    os.makedirs(args.out, exist_ok=True)
    for name in args.maps:
        data = map_json(decomp, name, layouts, names, loaded, unhandled)
        with open(os.path.join(args.out, name + '.json'), 'w', newline='\n') as out:
            json.dump(data, out, indent=4)
            out.write('\n')
    if unhandled:
        print(f'Non-collision behaviors written as floor: {unhandled}')


if __name__ == '__main__':
    main()
