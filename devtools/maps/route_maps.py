"""Reads the JSON maps the walkers plan on, in either game's schema.

BDSP's maps use the shared schema (Pokemon/Navigation/Pokemon_MapData.h):
`name`, `source`, `tiles`, and optional `points` (a dict), `warps`
(`to_map`), `objects` (`kind` npc/item/sign, `wander` box). FRLG's are
generated from pokefirered (devtools/maps/frlg_maps.py) with their own
legend (PokemonFRLG_OverworldMap.h): `width`, `height`, `tiles`, optional
`elevation`, `warps` (`dest_map`), `objects` (`kind` is the sprite,
`wander_x`/`wander_y`), `points` as a list. The two legends disagree on a
few glyphs ('~' is tall grass in one and water in the other, 'v' a ledge or
an exit mat), so every tile is turned into a kind here, by the schema.
"""
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

#  Tile kinds, as the pictures colour them.
WALL, FLOOR, GRASS, WATER, LEDGE, DOOR, SIGN, WARP, MAT, ONE_WAY = (
    'wall', 'floor', 'grass', 'water', 'ledge', 'door', 'sign', 'warp', 'mat', 'one_way')

SHARED_LEGEND = {
    '#': WALL, '.': FLOOR, '~': GRASS, 'w': WATER,
    '^': LEDGE, 'v': LEDGE, '<': LEDGE, '>': LEDGE,
    'D': DOOR, 'S': SIGN,
}
FRLG_LEGEND = {
    '#': WALL, 'J': LEDGE, '.': FLOOR, '"': GRASS, '~': WATER,
    'D': DOOR, 'S': SIGN, 'C': WARP, 'L': WARP,
    '^': MAT, 'v': MAT, '<': MAT, '>': MAT,
    'n': ONE_WAY, 's': ONE_WAY, 'e': ONE_WAY, 'w': ONE_WAY,
}


@dataclass
class TileMap:
    name: str
    rows: list
    legend: dict
    objects: list = field(default_factory=list)     # [(x, y, label)]
    warps: list = field(default_factory=list)       # [(x, y, to_map)]

    @property
    def width(self):
        return max((len(r) for r in self.rows), default=0)

    @property
    def height(self):
        return len(self.rows)

    def contains(self, x, y):
        return 0 <= y < len(self.rows) and 0 <= x < len(self.rows[y])

    def kind(self, x, y):
        if not self.contains(x, y):
            return WALL
        return self.legend.get(self.rows[y][x], FLOOR)

    def glyph(self, x, y):
        return self.rows[y][x] if self.contains(x, y) else '#'


def key(name):
    """The name a map is looked up by: the logs write "Route 202", the files "Route202"."""
    return re.sub(r'[^0-9a-z_]', '', name.lower())


def parse(data):
    """A TileMap from one map file's JSON object."""
    rows = list(data['tiles'])
    if 'width' in data and 'source' not in data:
        objects = [(o['x'], o['y'], o.get('kind', '')) for o in data.get('objects', [])]
        warps = [(w['x'], w['y'], w.get('dest_map', '')) for w in data.get('warps', [])]
        return TileMap(data['name'], rows, FRLG_LEGEND, objects, warps)
    objects = [(o['x'], o['y'], o.get('name', o.get('kind', ''))) for o in data.get('objects', [])]
    warps = [(w['x'], w['y'], w.get('to_map', '')) for w in data.get('warps', [])]
    return TileMap(data['name'], rows, SHARED_LEGEND, objects, warps)


def load_dir(*dirs):
    """Every map in the given directories, by key(name). Later directories win."""
    maps = {}
    for d in dirs:
        for path in sorted(Path(d).glob('*.json')):
            try:
                data = json.loads(path.read_text(encoding='utf-8'))
            except ValueError:
                continue
            if not isinstance(data, dict) or 'tiles' not in data or 'name' not in data:
                continue
            m = parse(data)
            maps[key(m.name)] = m
            maps.setdefault(key(path.stem), m)
    return maps
