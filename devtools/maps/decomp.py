"""Decodes a map's tile grid from the Pokemon Platinum or Diamond decompilation.

The pret decompilations hold each 32 x 32 map cell's collision as a land data
file: a 16-byte header of four u32 sizes (attributes, movement models, map
models, height data), then 1024 little-endian u16 tile attributes, row-major,
index z * 32 + x. Bit 0x8000 blocks the tile; the low byte is its behaviour
(tall grass, water, ledges, doors, warps). Cells sit in a matrix: the
overworld's is 30 x 30 (matrix 000), caves have their own; world
x = 32 * column + lx, z = 32 * row + lz, and the events files place objects
in these world coordinates.

pokeplatinum keeps matrices and events as JSON and its map headers in
include/data/map_headers.h. pokediamond keeps them as binary NARC entries
(the layout pokeplatinum's tools/jsoncnv packs) with the header table in
arm9/src/map_header.c, and gives sprites, movement types and trainer types
by number; pokeplatinum's generated/*.txt tables name them. BDSP keeps
Diamond/Pearl's layout; Platinum's few changes show up on the console.
"""
import datetime
import json
import re
import struct
import subprocess
import textwrap
from dataclasses import dataclass, field
from pathlib import Path

CELL = 32
#  Tile attribute bits and behaviours (include/constants/field/map_tile_behaviors.h).
BLOCKED = 0x8000
TALL_GRASS = {0x02, 0x03}
WATER = {0x10, 0x13, 0x15}                                   # river, waterfall, sea
JUMP = {0x38: 'east', 0x39: 'west', 0x3a: 'north', 0x3b: 'south'}
JUMP_TWICE = {0x5a: 'north', 0x5b: 'south', 0x5c: 'west', 0x5d: 'east'}
WARP_STAIRS = {0x5e, 0x5f}
CAVE_MOUTHS = {0x62, 0x63, 0x64, 0x65}                       # blocked, walked into
DOOR = 0x69
WARP_TILES = {0x6c, 0x6d, 0x6e, 0x6f}                        # walkable, stood on
#  Objects that stand on the ground and block it.
OBSTACLES = {'CUT_TREE', 'ROCK_SMASH', 'STRENGTH_BOULDER'}
ITEM = 'POKEBALL'
#  bg events: 0 facing, 1 wall sign, 2 hidden item (generated/bg_event_types.txt).
BG_SIGNS = {0, 1}


def tile_char(attr):
    """Our legend for one tile attribute: (char, note or None)."""
    behaviour = attr & 0xff
    if behaviour in JUMP or behaviour in JUMP_TWICE:
        twice = behaviour in JUMP_TWICE
        side = JUMP_TWICE[behaviour] if twice else JUMP[behaviour]
        if side == 'south':
            return 'v', 'double ledge' if twice else None
        return '#', f'ledge jumped {side}{" twice" if twice else ""}, shown as a wall'
    if behaviour == DOOR or behaviour in CAVE_MOUTHS:
        return 'D', None
    if behaviour in WARP_TILES or behaviour in WARP_STAIRS:
        return 'W', None
    if behaviour in WATER:
        return '#', 'water'
    if attr & BLOCKED:
        return '#', None
    if behaviour in TALL_GRASS:
        return '"', None
    return '.', None


def decode_land_data(data):
    """The 1024 tile attributes of a land data file (map_data_NNN.bin / narc_NNNN.bin)."""
    sizes = struct.unpack('<4I', data[:16])
    if sizes[0] != 2 * CELL * CELL:
        raise ValueError(f'land data: attribute block is {sizes[0]} bytes, expected {2 * CELL * CELL}')
    return list(struct.unpack(f'<{CELL * CELL}H', data[16:16 + sizes[0]]))


def gh_fetch(repo, path):
    """One file of a GitHub repository, raw; binary works."""
    result = subprocess.run(
        ['gh', 'api', f'repos/{repo}/contents/{path}', '-H', 'Accept: application/vnd.github.raw'],
        check=True, stdout=subprocess.PIPE)
    return result.stdout


class Source:
    """Files of one decompilation: a local checkout, or fetched once into the cache."""

    def __init__(self, repo, cache, local=None, fetch=gh_fetch):
        self.repo = repo
        self.cache = Path(cache)
        self.local = Path(local) if local else None
        self.fetch = fetch

    def read(self, path):
        if self.local:
            return (self.local / path).read_bytes()
        cached = self.cache / self.repo / path
        if not cached.exists():
            data = self.fetch(self.repo, path)
            cached.parent.mkdir(parents=True, exist_ok=True)
            cached.write_bytes(data)
        return cached.read_bytes()

    def text(self, path):
        return self.read(path).decode('utf-8', errors='replace')


@dataclass
class MapHeader:
    name: str
    matrix_id: int
    events_id: object       # Platinum: events archive name; Diamond: NARC index


@dataclass
class Matrix:
    matrix_id: int
    width: int
    height: int
    maps: list              # land data id per cell, row-major, None for MAP_NONE
    headers: list = None    # map header name per cell, or None when the matrix has none

    def cells_of(self, header_name):
        """(row, col) of every cell carrying this header; all cells when the matrix has no headers."""
        cells = [(i // self.width, i % self.width) for i in range(self.width * self.height)
                 if self.maps[i] is not None and (self.headers is None or self.headers[i] == header_name)]
        return cells


@dataclass
class ObjectEvent:
    name: str
    gfx: str
    movement: str
    trainer: bool
    sight: int
    range_x: int
    range_z: int
    x: int
    z: int
    hidden: str = ''


@dataclass
class Warp:
    x: int
    z: int
    dest: str


@dataclass
class Sign:
    x: int
    z: int


@dataclass
class Events:
    objects: list = field(default_factory=list)
    warps: list = field(default_factory=list)
    signs: list = field(default_factory=list)


def strip_prefix(name, prefix):
    return name[len(prefix):] if name.startswith(prefix) else name


class Platinum:
    """pret/pokeplatinum: JSON matrices and events, map headers in include/data/map_headers.h."""
    REPO = 'pret/pokeplatinum'
    GAME = 'Pokemon Platinum'
    HEADER = re.compile(r'\[MAP_HEADER_(\w+)\]\s*=\s*\{(.*?)\}', re.S)

    def __init__(self, source):
        self.source = source
        self._headers = None

    def header(self, name):
        if self._headers is None:
            text = self.source.text('include/data/map_headers.h')
            self._headers = {m.group(1): m.group(2) for m in self.HEADER.finditer(text)}
        body = self._headers.get(name)
        if body is None:
            raise KeyError(f'{name}: no MAP_HEADER_{name} in include/data/map_headers.h')
        matrix = re.search(r'\.mapMatrixID\s*=\s*map_matrix_(\d+)', body)
        events = re.search(r'\.eventsArchiveID\s*=\s*(\w+)', body)
        return MapHeader(name, int(matrix.group(1)), events.group(1))

    def matrix(self, matrix_id):
        data = json.loads(self.source.text(f'res/field/matrices/map_matrix_{matrix_id:03d}.json'))
        maps = [None if m == 'MAP_NONE' else int(m[len('MAP_'):]) for row in data['maps'] for m in row]
        headers = [strip_prefix(h, 'MAP_HEADER_') for row in data['headers'] for h in row] or None
        return Matrix(matrix_id, len(data['maps'][0]), len(data['maps']), maps, headers)

    def land_name(self, land_id):
        return f'map_data_{land_id:03d}'

    def land_path(self, land_id):
        return f'res/field/maps/data/{self.land_name(land_id)}.bin'

    def land_data(self, land_id):
        return decode_land_data(self.source.read(self.land_path(land_id)))

    def events_name(self, events_id):
        return f'{events_id}.json'

    def events(self, events_id):
        data = json.loads(self.source.text(f'res/field/events/{events_id}.json'))
        events = Events()
        for o in data.get('object_events', []):
            trainer = o['trainer_type'] != 'TRAINER_TYPE_NONE'
            events.objects.append(ObjectEvent(
                name=title(strip_prefix(o['id'], 'LOCALID_')),
                gfx=strip_prefix(o['graphics_id'], 'OBJ_EVENT_GFX_'),
                movement=strip_prefix(o['movement_type'], 'MOVEMENT_TYPE_'),
                trainer=trainer,
                sight=(o.get('data') or [0])[0] if trainer else 0,
                range_x=o.get('movement_range_x', 0), range_z=o.get('movement_range_z', 0),
                x=o['x'], z=o['z'],
                hidden='' if str(o.get('hidden_flag', '0')) == '0' else str(o['hidden_flag'])))
        for w in data.get('warp_events', []):
            events.warps.append(Warp(w['x'], w['z'], strip_prefix(w['dest_header_id'], 'MAP_HEADER_')))
        for b in data.get('bg_events', []):
            if b['type'] in BG_SIGNS:
                events.signs.append(Sign(b['x'], b['z']))
        return events


class Diamond:
    """pret/pokediamond: binary matrices and events, the header table in arm9/src/map_header.c.

    Sprites, movement types and trainer types are numbers there; `names` is a
    Source for pret/pokeplatinum, whose generated/*.txt tables list them in
    order (checked on Route 204 North: the same twelve objects come out).
    """
    REPO = 'pret/pokediamond'
    GAME = 'Pokemon Diamond'
    HEADER_LINE = re.compile(r'\{([^{}]*)\}\s*,?\s*//\s*(MAP_\w+)')
    DEFINE = re.compile(r'#define\s+(MAP_\w+)\s+(\d+)')

    def __init__(self, source, names):
        self.source = source
        self.names = names
        self._headers = None
        self._map_ids = None
        self._tables = {}

    def header(self, name):
        if self._headers is None:
            text = self.source.text('arm9/src/map_header.c')
            self._headers = {m.group(2): m.group(1) for m in self.HEADER_LINE.finditer(text)}
        body = self._headers.get(f'MAP_{name}')
        if body is None:
            raise KeyError(f'{name}: no MAP_{name} row in arm9/src/map_header.c')
        matrix = re.search(r'NARC_map_matrix_narc_(\d+)_bin', body)
        events = re.search(r'NARC_zone_event_release_narc_(\d+)_bin', body)
        return MapHeader(name, int(matrix.group(1)), int(events.group(1)))

    def map_name(self, map_id):
        """MAP_ constant name (without the prefix) for a numeric map header id."""
        if self._map_ids is None:
            text = self.source.text('include/constants/maps.h')
            self._map_ids = {int(m.group(2)): m.group(1) for m in self.DEFINE.finditer(text)}
        return strip_prefix(self._map_ids.get(map_id, f'MAP_{map_id}'), 'MAP_')

    def table(self, name):
        if name not in self._tables:
            self._tables[name] = self.names.text(f'generated/{name}.txt').split()
        return self._tables[name]

    def named(self, table, prefix, value):
        entries = self.table(table)
        return strip_prefix(entries[value], prefix) if value < len(entries) else str(value)

    def matrix(self, matrix_id):
        data = self.source.read(f'files/fielddata/mapmatrix/map_matrix/narc_{matrix_id:04d}.bin')
        width, height, has_headers, has_altitudes, name_len = struct.unpack_from('<5B', data, 0)
        offset = 5 + name_len
        count = width * height
        headers = None
        if has_headers:
            headers = [self.map_name(h) for h in struct.unpack_from(f'<{count}H', data, offset)]
            offset += 2 * count
        if has_altitudes:
            offset += count
        maps = [None if m == 0xffff else m for m in struct.unpack_from(f'<{count}H', data, offset)]
        return Matrix(matrix_id, width, height, maps, headers)

    def land_name(self, land_id):
        return f'land_data narc_{land_id:04d}'

    def land_path(self, land_id):
        return f'files/fielddata/land_data/land_data_release/narc_{land_id:04d}.bin'

    def land_data(self, land_id):
        return decode_land_data(self.source.read(self.land_path(land_id)))

    def events_name(self, events_id):
        return f'zone_event narc_{events_id:04d}'

    def events(self, events_id):
        data = self.source.read(f'files/fielddata/eventdata/zone_event_release/narc_{events_id:04d}.bin')
        events = Events()
        offset = 0
        (count,) = struct.unpack_from('<I', data, offset)
        offset += 4
        for _ in range(count):
            script, kind, x, z, y, facing = struct.unpack_from('<HHIIIH2x', data, offset)
            offset += 20
            if kind in BG_SIGNS:
                events.signs.append(Sign(x, z))
        (count,) = struct.unpack_from('<I', data, offset)
        offset += 4
        for i in range(count):
            f = struct.unpack_from('<14HI', data, offset)
            offset += 32
            gfx = self.named('object_events_gfx', 'OBJ_EVENT_GFX_', f[1])
            trainer = f[3] != 0
            events.objects.append(ObjectEvent(
                name=f'{title(gfx)} (object {i})', gfx=gfx,
                movement=self.named('movement_types', 'MOVEMENT_TYPE_', f[2]),
                trainer=trainer, sight=f[7] if trainer else 0,
                range_x=f[10], range_z=f[11], x=f[12], z=f[13],
                hidden='' if f[4] == 0 else f'flag {f[4]}'))
        (count,) = struct.unpack_from('<I', data, offset)
        offset += 4
        for _ in range(count):
            x, z, dest, _warp = struct.unpack_from('<4H4x', data, offset)
            offset += 12
            events.warps.append(Warp(x, z, self.map_name(dest)))
        return events


def title(constant):
    """AROMA_LADY_TAYLOR -> Aroma Lady Taylor."""
    return ' '.join(w.capitalize() for w in constant.split('_'))


def game_for(repo, cache, local=None, fetch=gh_fetch):
    if repo == Platinum.REPO:
        return Platinum(Source(repo, cache, local, fetch))
    if repo == Diamond.REPO:
        return Diamond(Source(repo, cache, local, fetch), Source(Platinum.REPO, cache, local, fetch))
    raise ValueError(f'{repo}: not a decompilation this tool reads ({Platinum.REPO}, {Diamond.REPO})')


class World:
    """Tile attributes of a matrix, by world coordinates, cells loaded as needed."""

    def __init__(self, game, matrix):
        self.game = game
        self.matrix = matrix
        self._cells = {}

    def land_id(self, x, z):
        row, col = z // CELL, x // CELL
        if x < 0 or z < 0 or row >= self.matrix.height or col >= self.matrix.width:
            return None
        return self.matrix.maps[row * self.matrix.width + col]

    def attr(self, x, z):
        land_id = self.land_id(x, z)
        if land_id is None:
            return None
        if land_id not in self._cells:
            self._cells[land_id] = self.game.land_data(land_id)
        return self._cells[land_id][(z % CELL) * CELL + (x % CELL)]


@dataclass
class Decoded:
    game: str
    repo: str
    map_name: str
    matrix_id: int
    rect: tuple                 # x0, z0, w, h in world coordinates
    cells: list                 # (row, col, land name) covering the rect
    events_name: str
    rows: list
    notes: dict                 # note -> [(x, y)] local
    placed: list                # lines for objects drawn on the grid
    off_grid: list              # lines for wanderers, listed only
    date: str = ''

    @property
    def source_line(self):
        lands = ', '.join(name for _, _, name in self.cells)
        return f'Map source: the decomp ({lands}, {self.date}) with the objects from its events file.'


def default_rect(matrix, cells):
    rows = [r for r, _ in cells]
    cols = [c for _, c in cells]
    return (CELL * min(cols), CELL * min(rows), CELL * (max(cols) - min(cols) + 1), CELL * (max(rows) - min(rows) + 1))


def facing(movement):
    if movement == 'NONE':
        return ''
    if movement.startswith('LOOK_'):
        return 'looks ' + movement[len('LOOK_'):].lower().replace('_', ' ')
    return movement.lower().replace('_', ' ')


def place_object(obj, grid, x, y):
    """Marks a static object on the grid; returns its line, or None and the line for a wanderer."""
    where = f'({x}, {y})'
    hidden = f', hidden by {obj.hidden}' if obj.hidden else ''
    if obj.gfx == ITEM:
        grid[y][x] = '#'
        return f"item on the ground {where}, as '#'{hidden}", None
    if 'SIGN' in obj.gfx:
        grid[y][x] = 'S'
        return f'{title(obj.gfx).lower()} {where}', None
    if obj.gfx in OBSTACLES:
        grid[y][x] = '#'
        return f"{title(obj.gfx).lower()} {where}, as '#'{hidden}", None
    seen = f', sees {obj.sight}' if obj.trainer else ', not a trainer'
    if obj.movement.startswith(('WANDER', 'WALK')):
        span = f'columns {x - obj.range_x} to {x + obj.range_x}' if obj.range_x else f'column {x}'
        rows = f'rows {y - obj.range_z} to {y + obj.range_z}' if obj.range_z else f'row {y}'
        return None, f'{obj.name} {facing(obj.movement)} from {where}, {span}, {rows}{seen}{hidden}: off the grid'
    grid[y][x] = 'N'
    look = f', {facing(obj.movement)}' if facing(obj.movement) else ''
    return f'{obj.name} {where}{look}{seen}{hidden}', None


def decode(game, map_name, rect=None, date=None):
    header = game.header(map_name)
    matrix = game.matrix(header.matrix_id)
    cells = matrix.cells_of(map_name)
    if not cells:
        raise KeyError(f'{map_name}: no cell of matrix {header.matrix_id:03d} carries it')
    if rect is None:
        rect = default_rect(matrix, cells)
    x0, z0, w, h = rect
    world = World(game, matrix)
    grid = [[' '] * w for _ in range(h)]
    notes = {}
    for y in range(h):
        for x in range(w):
            attr = world.attr(x0 + x, z0 + y)
            if attr is None:
                grid[y][x] = '?'
                continue
            ch, note = tile_char(attr)
            grid[y][x] = ch
            if note:
                notes.setdefault(note, []).append((x, y))
    events = game.events(header.events_id)
    inside = lambda ex, ez: x0 <= ex < x0 + w and z0 <= ez < z0 + h
    placed, off_grid = [], []
    #  A warp event sits on the tile that triggers it (a door or warp tile) or,
    #  for a cave mouth, on the tile in front of it, where a walk arrives.
    for warp in events.warps:
        if inside(warp.x, warp.z):
            x, y = warp.x - x0, warp.z - z0
            kind = {'D': 'door', 'W': 'warp tile'}.get(grid[y][x], 'arrival tile')
            placed.append(f'{kind} ({x}, {y}) to {title(warp.dest)}')
    for sign in events.signs:
        if inside(sign.x, sign.z):
            x, y = sign.x - x0, sign.z - z0
            if grid[y][x] == '#':
                grid[y][x] = 'S'
                placed.append(f'sign script on the wall ({x}, {y})')
            else:
                placed.append(f'sign script on open ground ({x}, {y}), not marked')
    for obj in events.objects:
        if inside(obj.x, obj.z):
            line, wander = place_object(obj, grid, obj.x - x0, obj.z - z0)
            if line:
                placed.append(line)
            if wander:
                off_grid.append(wander)
    covered = [(r, c, game.land_name(world.land_id(CELL * c, CELL * r)))
               for r in range(z0 // CELL, (z0 + h - 1) // CELL + 1)
               for c in range(x0 // CELL, (x0 + w - 1) // CELL + 1)
               if world.land_id(CELL * c, CELL * r) is not None]
    return Decoded(
        game=game.GAME, repo=game.REPO, map_name=map_name, matrix_id=header.matrix_id, rect=rect,
        cells=covered, events_name=game.events_name(header.events_id),
        rows=[''.join(r) for r in grid], notes=notes, placed=placed, off_grid=off_grid,
        date=date or datetime.date.today().isoformat())


def comment(text, indent='//  ', width=78):
    return textwrap.wrap(text, width=width, initial_indent=indent, subsequent_indent=indent)


def header_comment(decoded):
    x0, z0, w, h = decoded.rect
    matrix = 'the Sinnoh matrix' if decoded.matrix_id == 0 else f'matrix {decoded.matrix_id:03d}'
    #  The three status lines Data/MAPS.md asks for, continuation lines indented as there.
    lines = textwrap.wrap(decoded.source_line, width=78, initial_indent='//  ', subsequent_indent='//      ')
    lines += ['//  Console checks: none yet.', '//  Status: no checkpoint passes on this grid yet.', '//']
    if len(decoded.cells) == 1:
        row, col, land = decoded.cells[0]
        where = f'{land}: one cell of {matrix} at column {col}, row {row}'
    else:
        where = 'cells of ' + matrix + ': ' + ', '.join(f'{land} (column {c}, row {r})' for r, c, land in decoded.cells)
    lines += comment(
        f"Decoded from {decoded.game}'s collision table for this map (the {decoded.repo} decompilation, "
        f'{where}), with the objects from its events file ({decoded.events_name}); BDSP keeps '
        f"Diamond/Pearl's layout. Local (x, y) = world (x - {x0}, z - {z0}); {w} x {h} tiles.")
    lines += comment("Water and the items on the ground are '#'. Objects, from the events file:")
    for line in decoded.placed:
        lines += comment(line + ';', indent='//    ', width=78)
    for line in decoded.off_grid:
        lines += comment(line + ';', indent='//    ', width=78)
    if not decoded.placed and not decoded.off_grid:
        lines += comment('none in this rectangle.', indent='//    ')
    for note, tiles in decoded.notes.items():
        shown = ', '.join(f'({x}, {y})' for x, y in tiles[:12]) + (f' and {len(tiles) - 12} more' if len(tiles) > 12 else '')
        lines += comment(f'{note}: {shown}.', indent='//  ')
    return lines


def cpp_rows(rows):
    width = max(len(r) for r in rows)
    ruler = ''.join(str(c // 10) if c % 10 == 0 else ' ' for c in range(width)).rstrip()
    lines = [f'    //     {ruler}']
    lines += [f'        R"({row})",  // {y:3d}' for y, row in enumerate(rows)]
    return lines


def render_cpp(decoded):
    return '\n'.join(header_comment(decoded) + cpp_rows(decoded.rows))


def render_text(decoded):
    return '\n'.join(decoded.rows)
