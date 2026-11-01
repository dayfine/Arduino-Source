"""Draws the route walks of a trace on their maps: the walked line, and the plan dashed on top.

Per checkpoint attempt, one picture per map joining all its walks in order,
each in its own colour and numbered; then one picture per walk and map, as
PNG and SVG, and an index.html per run with the map pictures first. Each
picture is cropped to the walked area plus a margin and zoomed to fit. The
walked line is as wide as the times the player crossed that tile edge, so
back and forth shows as a fat line. A leg taken to have ended as planned
after an interrupt (a battle on the way to a wall) is drawn pale and
striped, labelled "assumed after ..."; where the player's tile isn't known
(a leg stopped by a battle, the hold that found them again) it is dotted.
"""
import json
import re
from dataclasses import replace
from html import escape
from pathlib import Path

from . import route_maps
from .picture import Picture
from .route_trace import DIRECTIONS, to_json

MARGIN = 22         # room for the coordinate labels
HEADER = 50

TILE_FILL = {
    route_maps.WALL: '#5b5b5b', route_maps.FLOOR: '#f4f1e8', route_maps.GRASS: '#9fd38a',
    route_maps.WATER: '#86b8e8', route_maps.LEDGE: '#b48be0', route_maps.DOOR: '#2f6fd6',
    route_maps.SIGN: '#e89a2c', route_maps.WARP: '#8a5a2b', route_maps.MAT: '#58c7c7',
    route_maps.ONE_WAY: '#d8d2bd',
}
PLAN_COLOUR = {'plan': '#1f4fd1', 'replan': '#9b30c8'}
WALK_COLOUR = '#e0301e'
UNSURE_COLOUR = '#f08a24'
MARK_STYLE = {      # fill, letter
    'battle': ('#d0102a', 'B'), 'message': ('#7a4dd8', 'M'), 'trainer': ('#a01070', 'T'),
    'stop': ('#f08a24', '!'), 'wait': ('#c0a000', 'W'), 'reanchor': ('#10a0b0', 'R'),
}


def slug(text):
    return re.sub(r'[^0-9A-Za-z]+', '_', text).strip('_')[:40] or 'walk'


def _pick(places, prefer=None):
    """One tile of a set: `prefer` if it is in it, else the first."""
    if not places:
        return None
    if prefer is not None and prefer in places:
        return prefer
    return places[0]


def _inside(maps, place):
    m = maps.get(route_maps.key(place[0]))
    return m is None or m.contains(place[1], place[2])


def leg_path(maps, start, leg):
    """Where `leg` from `start` went: ([(map, [(x, y), ...]), ...], end, certain).

    `start` may be None: the path is then worked back from the end, and
    isn't certain. A leg onto another map is drawn on both: on the first
    up to its edge or warp, on the second from its edge to the end.
    """
    if not leg.ends:
        return [], start, False
    dx, dy = DIRECTIONS[leg.direction]
    if leg.action == 'face':
        return [], _pick(leg.ends, start) if start is None or start not in leg.ends else start, True
    if start is None:
        end = leg.ends[0]
        tiles = [(end[1] - dx * k, end[2] - dy * k) for k in range(leg.tiles, -1, -1)]
        tiles = [t for t in tiles if _inside(maps, (end[0], t[0], t[1]))]
        return [(end[0], tiles)], end, False
    name, sx, sy = start
    #  On the same map: the end on the leg's line nearest to its length.
    best = None
    for end in leg.ends:
        if route_maps.key(end[0]) != route_maps.key(name):
            continue
        ex, ey = end[1] - sx, end[2] - sy
        k = ex * dx + ey * dy
        if k < 0 or ex * dy - ey * dx != 0:
            continue
        score = (0 if k <= leg.tiles else 1, abs(leg.tiles - k))
        if best is None or score < best[0]:
            best = (score, end, k)
    if best is not None:
        _, end, k = best
        return [(name, [(sx + dx * i, sy + dy * i) for i in range(k + 1)])], end, True
    end = leg.ends[0]
    if route_maps.key(end[0]) == route_maps.key(name):
        #  Off the line: a straight jump, drawn but not counted.
        return [(name, [(sx, sy), (end[1], end[2])])], end, False
    #  Onto another map, through a warp or across its edge.
    first = [(sx, sy)]
    for i in range(1, leg.tiles + 1):
        t = (sx + dx * i, sy + dy * i)
        if not _inside(maps, (name, t[0], t[1])):
            break
        first.append(t)
    pieces = [(name, first)]
    if not leg.warp:
        rest = leg.tiles - (len(first) - 1)
        second = [(end[1] - dx * k, end[2] - dy * k) for k in range(max(0, rest - 1), -1, -1)]
        second = [t for t in second if _inside(maps, (end[0], t[0], t[1]))]
        pieces.append((end[0], second))
    else:
        pieces.append((end[0], [(end[1], end[2])]))
    return pieces, end, True


def plan_paths(maps, plan):
    """The planned legs as tile paths, from the plan's first start tile."""
    at = _pick(plan.start)
    out = []
    for leg in plan.legs:
        pieces, end, certain = leg_path(maps, at, leg)
        out.extend(pieces)
        at = end
    return out, _pick(plan.start), at


def walked_paths(maps, walk):
    """([(map, tiles, basis, why)], first tile, last known tile) for the steps as walked.

    `basis` is walked, assumed (taken to have ended as planned after an
    interrupt) or unknown (the tile the player stopped on isn't known).
    """
    at = None
    first = None
    out = []
    for step in walk.steps:
        start = _pick(step.start, at) if step.start else None
        if first is None:
            first = start
        if step.role == 'hold' and not step.certain:
            #  A hold from an unknown tile, to a tile that depends on it: not drawn.
            at = None
            continue
        if step.outcome in ('stopped', 'failed') and not step.end:
            #  Stopped somewhere along the leg.
            pieces, end, _ = leg_path(maps, start, step.leg)
            out.extend((m, t, 'unknown', step.why or 'tile unknown') for m, t in pieces)
            at = None
            continue
        if step.end and step.outcome == 'stopped' and start is not None:
            pieces, end, certain = leg_path(maps, start, _with_ends(step.leg, step.end))
        else:
            pieces, end, certain = leg_path(maps, start, step.leg)
        basis = step.basis if certain else 'unknown'
        why = step.why or ('' if certain else 'start unknown')
        out.extend((m, t, basis, why) for m, t in pieces)
        at = _pick(step.end, end) if step.end else end
    return out, first, at


def _with_ends(leg, ends):
    return replace(leg, ends=list(ends), tiles=max(leg.tiles, 1))


def maps_of(walk):
    """The maps a walk touched, in order."""
    names = []

    def add(place):
        if place and place[0] not in names:
            names.append(place[0])
    for plan in walk.plans:
        for p in plan.start[:1]:
            add(p)
    for step in walk.steps:
        for p in step.start[:1] + step.leg.ends[:1]:
            add(p)
    for plan in walk.plans:
        for leg in plan.legs:
            for p in leg.ends[:1]:
                add(p)
    return names


def edge_counts(paths, name, basis='walked'):
    """Times each tile edge of map `name` was walked, for paths of `basis`."""
    counts = {}
    for m, tiles, b, _ in paths:
        if b != basis or route_maps.key(m) != route_maps.key(name):
            continue
        for a, c in zip(tiles, tiles[1:]):
            if abs(a[0] - c[0]) + abs(a[1] - c[1]) != 1:
                continue
            e = (min(a, c), max(a, c))
            counts[e] = counts.get(e, 0) + 1
    return counts


def _mix(colour, amount):
    """`colour` mixed with white: 0 keeps it, 1 is white."""
    c = colour.lstrip('#')
    rgb = [int(c[i:i + 2], 16) for i in (0, 2, 4)]
    return '#' + ''.join(f'{int(v + (255 - v) * amount):02x}' for v in rgb)


class View:
    """The part of a map a picture shows, and its scale: the walked area plus a margin, zoomed."""

    def __init__(self, tmap, tiles, margin=4, min_size=(14, 10), width_px=1000, max_height_px=1300):
        w, h = tmap.width, tmap.height
        if tiles:
            x0, x1 = min(t[0] for t in tiles) - margin, max(t[0] for t in tiles) + margin
            y0, y1 = min(t[1] for t in tiles) - margin, max(t[1] for t in tiles) + margin
        else:
            x0, x1, y0, y1 = 0, w - 1, 0, h - 1
        x0, x1 = self._grow(x0, x1, min_size[0], w)
        y0, y1 = self._grow(y0, y1, min_size[1], h)
        self.x0, self.y0, self.w, self.h = x0, y0, x1 - x0 + 1, y1 - y0 + 1
        tile = min(40, width_px // self.w, max_height_px // self.h)
        self.tile = max(12, tile)
        self.ox, self.oy = MARGIN, HEADER

    @staticmethod
    def _grow(a, b, size, limit):
        """[a, b] widened to `size` around its middle, then kept inside [0, limit)."""
        size = min(size, limit)
        need = size - (b - a + 1)
        if need > 0:
            a, b = a - need // 2, b + need - need // 2
        if a < 0:
            a, b = 0, b - a
        if b > limit - 1:
            a, b = max(0, a - (b - limit + 1)), limit - 1
        return a, b

    def contains(self, x, y):
        return self.x0 <= x < self.x0 + self.w and self.y0 <= y < self.y0 + self.h

    def box(self, x, y):
        return self.ox + (x - self.x0) * self.tile, self.oy + (y - self.y0) * self.tile

    def centre(self, x, y):
        px, py = self.box(x, y)
        return px + self.tile / 2, py + self.tile / 2

    @property
    def px_width(self):
        return self.ox + self.w * self.tile

    @property
    def px_height(self):
        return self.oy + self.h * self.tile


def _draw_tiles(pic, view, tmap):
    t = view.tile
    for y in range(view.y0, view.y0 + view.h):
        for x in range(view.x0, view.x0 + view.w):
            kind = tmap.kind(x, y)
            px, py = view.box(x, y)
            pic.rect(px, py, t, t, TILE_FILL.get(kind, '#ffffff'))
            g = tmap.glyph(x, y)
            if kind in (route_maps.LEDGE, route_maps.MAT, route_maps.ONE_WAY) and g not in '#.':
                pic.text(px + t / 2 - 2, py + t / 2 - 4, g if g != 'J' else 'v', '#ffffff')
    for x, y, _ in tmap.objects:
        if view.contains(x, y):
            cx, cy = view.centre(x, y)
            pic.circle(cx, cy, t * 0.28, '#ff4fd8', '#7a0060')
    for x, y, _ in tmap.warps:
        if view.contains(x, y):
            px, py = view.box(x, y)
            pic.rect(px + 1, py + 1, t - 2, t - 2, None, stroke='#00e0ff')
    #  Coordinates every fifth tile.
    for x in range(view.x0, view.x0 + view.w):
        if x % 5 == 0:
            px, _ = view.box(x, 0)
            pic.text(px + 1, view.oy - 10, str(x), '#333333')
            pic.line(px, view.oy, px, view.px_height, '#000000', 0.6, alpha=0.3)
    for y in range(view.y0, view.y0 + view.h):
        if y % 5 == 0:
            _, py = view.box(0, y)
            pic.text(1, py + 2, str(y), '#333333')
            pic.line(view.ox, py, view.px_width, py, '#000000', 0.6, alpha=0.3)


def _polyline(pic, view, tiles, colour, width, dash=None, offset=0.0):
    """Lines through the tiles' centres; `offset` shifts them right of (or below) the centre."""
    for (ax, ay), (bx, by) in zip(tiles, tiles[1:]):
        x0, y0 = view.centre(ax, ay)
        x1, y1 = view.centre(bx, by)
        if ax == bx:
            x0, x1 = x0 + offset, x1 + offset
        else:
            y0, y1 = y0 + offset, y1 + offset
        pic.line(x0, y0, x1, y1, colour, width, dash)


def _label(pic, view, tiles, text, colour, placed):
    """`text` beside the middle of a stretch of tiles, moved down clear of the labels in `placed`."""
    if not tiles or not text:
        return
    x, y = tiles[len(tiles) // 2]
    cx, cy = view.centre(x, y)
    lx, ly, w = cx + view.tile * 0.5, cy - 4, len(text) * 6
    while any(lx < px + pw and px < lx + w and abs(ly - py) < 10 for px, py, pw in placed):
        ly += 11
    placed.append((lx, ly, w))
    pic.text(lx, ly, text, colour, halo='#ffffff')


def _badge(pic, view, place, fill, letter, corner=(1, -1)):
    """A small lettered circle at a corner of the tile, so it doesn't hide the line through it."""
    cx, cy = view.centre(place[1], place[2])
    r = max(6.5, view.tile * 0.24)
    bx, by = cx + corner[0] * view.tile * 0.38, cy + corner[1] * view.tile * 0.38
    pic.circle(bx, by, r, fill, '#ffffff')
    pic.text(bx - len(letter) * 3 + 1, by - 3.5, letter, '#ffffff')


def _here(view, name, place):
    return place is not None and route_maps.key(place[0]) == route_maps.key(name) and view.contains(place[1], place[2])


def _tiles_of(maps, walks, name):
    """Every tile the walks' plans and steps cross on `name`, to fit the view to."""
    out = []
    for walk in walks:
        paths, first, last = walked_paths(maps, walk)
        for m, tiles, _, _ in paths:
            if route_maps.key(m) == route_maps.key(name):
                out.extend(tiles)
        for plan in walk.plans:
            pieces, _, _ = plan_paths(maps, plan)
            for m, tiles in pieces:
                if route_maps.key(m) == route_maps.key(name):
                    out.extend(tiles)
        for mark in walk.marks:
            out.extend((p[1], p[2]) for p in mark.at[:1] if route_maps.key(p[0]) == route_maps.key(name))
    return out


def _draw_walk(pic, view, maps, name, walk, colour, plan_colour, number=None, lane=0.0, placed=None):
    """One walk's lines and marks on the picture: walked (and assumed, unknown) first, the plan on top.

    `lane` shifts this walk's lines off the tile centres (in tiles), so that
    walks over the same tiles on one picture lie side by side. `placed`
    collects the labels drawn so far on the picture, to keep them apart.
    """
    placed = [] if placed is None else placed
    t = view.tile
    off = lane * t
    paths, first, last = walked_paths(maps, walk)
    here = route_maps.key(name)
    labels = []
    #  Unknown: dotted. Assumed: a pale band with stripes. Walked: solid, as wide as the times walked.
    for m, tiles, basis, why in paths:
        if route_maps.key(m) != here or basis == 'walked':
            continue
        if basis == 'unknown':
            _polyline(pic, view, tiles, UNSURE_COLOUR, max(3, t * 0.2), dash=(t * 0.25, t * 0.25), offset=off)
            labels.append((tiles, why, '#b05000'))
    for basis, width_of in (('assumed', None), ('walked', None)):
        counts = edge_counts(paths, name, basis)
        for (a, b), n in sorted(counts.items(), key=lambda kv: kv[1]):
            width = min(t * 0.9, t * (0.36 if lane == 0 else 0.24) + t * 0.2 * (n - 1))
            if basis == 'assumed':
                _polyline(pic, view, [a, b], _mix(colour, 0.72), width, offset=off)
                _polyline(pic, view, [a, b], colour, max(1.5, width * 0.3), dash=(2, 4), offset=off)
            else:
                _polyline(pic, view, [a, b], colour, width, offset=off)
            if n >= 3:
                x0, y0 = view.centre(*a)
                x1, y1 = view.centre(*b)
                pic.text((x0 + x1) / 2 - 2, (y0 + y1) / 2 - 3, str(min(n, 9)), '#ffffff')
    for m, tiles, basis, why in paths:
        if route_maps.key(m) == here and basis == 'assumed':
            labels.append((tiles, why, '#7a1a10'))

    #  The plan, on top: thinner, dashed, beside the walked line's centre so both show.
    for plan in walk.plans:
        pieces, start, end = plan_paths(maps, plan)
        pc = plan_colour if plan.reason == 'plan' else PLAN_COLOUR['replan']
        for m, tiles in pieces:
            if route_maps.key(m) == here:
                _polyline(pic, view, tiles, pc, max(2, t * 0.12), dash=(t * 0.45, t * 0.3), offset=off + t * 0.3)
        if _here(view, name, end):
            _badge(pic, view, end, pc, 'E', (1, 1))

    start = first or next((_pick(p.start) for p in walk.plans if p.start), None)
    if _here(view, name, start):
        _badge(pic, view, start, '#18a03a', str(number) if number else 'S', (-1, -1))
    for mark in walk.marks:
        at = next((p for p in mark.at if _here(view, name, p)), None)
        if at is None:
            continue
        if mark.kind == 'failed':
            cx, cy = view.centre(at[1], at[2])
            bx, by = cx - t * 0.38, cy + t * 0.38
            pic.cross(bx, by, max(5, t * 0.25), '#000000', 5)
            pic.cross(bx, by, max(4.5, t * 0.22), '#ff1010', 3)
        elif mark.kind == 'arrived':
            _badge(pic, view, at, '#000000', 'A', (1, 1))
        elif mark.kind in MARK_STYLE:
            fill, letter = MARK_STYLE[mark.kind]
            _badge(pic, view, at, fill, letter)
    seen = set()
    for tiles, why, c in labels:
        if (why, tuple(tiles)) not in seen:
            seen.add((why, tuple(tiles)))
            _label(pic, view, [p for p in tiles if view.contains(*p)], why, c, placed)
    return last


LEGEND_LINES = [
    'solid = walked, wider = more times;  pale striped = assumed (ended as planned after an interrupt)',
    'orange dotted = tile unknown (stopped by a battle, or found again);  dashed, offset = the plan',
    'S/1.. start  E plan end  A arrived  X failed  B battle  M message  T trainer  R found again',
]


def _canvas(view, legend_rows, title_lines):
    w = max(view.px_width + 6, 640)
    h = view.px_height + 10 + 12 * (len(LEGEND_LINES) + legend_rows)
    pic = Picture(w, h)
    for i, (text, colour) in enumerate(title_lines):
        pic.text(4, 3 + 11 * i, text, colour)
    return pic


def _footer(pic, view, extra=()):
    y = view.px_height + 6
    for colour, text in extra:
        if colour:
            pic.rect(4, y + 1, 18, 7, colour)
        pic.text(26, y, text, '#111111')
        y += 12
    for line in LEGEND_LINES:
        pic.text(4, y, line, '#333333')
        y += 12


def draw(walk, name, tmap, maps, run_id, index):
    """One picture of `walk` on map `name` (`tmap` may be None: no grid, routes only)."""
    if tmap is None:
        tmap = _bounds_map([walk], name)
    view = View(tmap, _tiles_of(maps, [walk], name))
    event = walk.event if walk.event.startswith(walk.checkpoint) else f'{walk.checkpoint} {walk.event}'
    title = f'{run_id}  #{index:02d}  {event}'.strip() + (f'  (attempt {walk.attempt})' if walk.attempt > 1 else '')
    sub = f'{walk.label} on {name}: {walk.outcome}, {len(walk.steps)} step(s), {len(walk.plans)} plan(s)'
    lines = [(title, '#000000'), (sub, '#b00000' if walk.outcome == 'failed' else '#000000')]
    if walk.error:
        lines.append((walk.error[:150], '#b00000'))
    pic = _canvas(view, 0, lines)
    _draw_tiles(pic, view, tmap)
    last = _draw_walk(pic, view, maps, name, walk, WALK_COLOUR, PLAN_COLOUR['plan'])
    if _here(view, name, last) and walk.outcome != 'done':
        cx, cy = view.centre(last[1], last[2])
        pic.circle(cx, cy, view.tile * 0.15, '#000000')
    _footer(pic, view)
    return pic


def draw_map(walks, name, tmap, maps, run_id, title):
    """All of `walks` ([(number, walk)]) on map `name`, in order, each in its own colour."""
    plain = [w for _, w in walks]
    if tmap is None:
        tmap = _bounds_map(plain, name)
    view = View(tmap, _tiles_of(maps, plain, name))
    failed = [w for w in plain if w.outcome == 'failed']
    lines = [(f'{run_id}  {title}', '#000000'),
             (f'{name}: {len(walks)} walk(s) in order, {len(failed)} failed', '#b00000' if failed else '#000000')]
    if failed and failed[-1].error:
        lines.append((failed[-1].error[:150], '#b00000'))
    pic = _canvas(view, len(walks), lines)
    _draw_tiles(pic, view, tmap)
    legend, placed = [], []
    for i, (number, walk) in enumerate(walks):
        colour = PALETTE[i % len(PALETTE)]
        lane = (i % 3 - 1) * 0.25 if len(walks) > 1 else 0.0
        _draw_walk(pic, view, maps, name, walk, colour, _darken(colour), i + 1, lane, placed)
        what = walk.event if walk.label == walk.event else f'{walk.event}: {walk.label}'
        legend.append((colour, f'{i + 1}  (walk #{number})  {what}  ({walk.outcome})'))
    _footer(pic, view, legend)
    return pic


PALETTE = ['#e41a1c', '#377eb8', '#ff7f00', '#984ea3', '#a65628', '#f781bf', '#00838f', '#808000', '#000080', '#555555']


def _darken(colour):
    c = colour.lstrip('#')
    return '#' + ''.join(f'{int(int(c[i:i + 2], 16) * 0.55):02x}' for i in (0, 2, 4))


def _bounds_map(walks, name):
    """A blank map just big enough for the walks' tiles on `name` (no grid loaded for it)."""
    xs, ys = [0], [0]
    for walk in walks:
        for plan in walk.plans:
            for p in plan.start + [e for leg in plan.legs for e in leg.ends]:
                if p[0] == name:
                    xs.append(p[1])
                    ys.append(p[2])
        for step in walk.steps:
            for p in step.start + step.leg.ends:
                if p[0] == name:
                    xs.append(p[1])
                    ys.append(p[2])
    rows = ['?' * (max(xs) + 3)] * (max(ys) + 3)
    return route_maps.TileMap(name + ' (no map file)', rows, {})


def attempts(run):
    """The run's walks by checkpoint attempt, in order: [(checkpoint, attempt, [(number, walk)])]."""
    groups = []
    for number, walk in enumerate(run.walks, 1):
        key = (walk.checkpoint, walk.attempt)
        if not groups or (groups[-1][0], groups[-1][1]) != key:
            groups.append((walk.checkpoint, walk.attempt, []))
        groups[-1][2].append((number, walk))
    return groups


def _write(pic, out_dir, stem):
    (out_dir / f'{stem}.png').write_bytes(pic.png())
    (out_dir / f'{stem}.svg').write_text(pic.svg(), encoding='utf-8')


def render_run(run, maps, out_dir):
    """Every walk of `run` into `out_dir`: per map and checkpoint attempt, then per walk; trace.json and index.html.

    Returns the per-walk pictures: [(number, walk, map, stem, has_map)].
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    overviews = []
    for g, (checkpoint, attempt, walks) in enumerate(attempts(run), 1):
        names = []
        for _, walk in walks:
            names.extend(n for n in maps_of(walk) if n not in names)
        for name in names:
            on_map = [(n, w) for n, w in walks if name in maps_of(w)]
            tmap = maps.get(route_maps.key(name))
            title = f'{checkpoint or "route"} attempt {attempt}: walks {on_map[0][0]}-{on_map[-1][0]}'
            pic = draw_map(on_map, name, tmap, maps, run.run_id, title)
            stem = f'map_{g:02d}_{slug(checkpoint or "route")}_a{attempt}_{slug(name)}'
            _write(pic, out_dir, stem)
            overviews.append((checkpoint, attempt, name, on_map, stem))
    pictures = []
    for index, walk in enumerate(run.walks, 1):
        for name in maps_of(walk):
            tmap = maps.get(route_maps.key(name))
            what = walk.event if walk.label == walk.event else walk.label
            stem = f'{index:03d}_{slug(what)}_{slug(name)}'
            _write(draw(walk, name, tmap, maps, run.run_id, index), out_dir, stem)
            pictures.append((index, walk, name, stem, tmap is not None))
    (out_dir / 'trace.json').write_text(json.dumps(to_json([run]), indent=1), encoding='utf-8')
    (out_dir / 'index.html').write_text(_index(run, overviews, pictures), encoding='utf-8')
    return pictures


def _index(run, overviews, pictures):
    maps_html = []
    for checkpoint, attempt, name, on_map, stem in overviews:
        failed = any(w.outcome == 'failed' for _, w in on_map)
        maps_html.append(
            f'<section><h3>{escape(checkpoint or "route")} attempt {attempt} &mdash; {escape(name)}: '
            f'walks {on_map[0][0]}&ndash;{on_map[-1][0]}</h3><p class="{"failed" if failed else ""}">'
            f'{", ".join(escape(w.event or w.label) for _, w in on_map)}</p>'
            f'<a href="{stem}.svg"><img src="{stem}.png" loading="lazy"></a></section>')
    rows = []
    for index, walk, name, stem, has_map in pictures:
        status = escape(walk.outcome) + (': ' + escape(walk.error) if walk.error else '')
        note = '' if has_map else ' (no map file)'
        attempt = f' (attempt {walk.attempt})' if walk.attempt > 1 else ''
        rows.append(
            f'<section><h3>#{index:02d} {escape(walk.event or walk.checkpoint)}{attempt} &mdash; {escape(walk.label)} '
            f'on {escape(name)}{note}</h3><p class="{escape(walk.outcome)}">{escape(walk.time)} &middot; {status}</p>'
            f'<a href="{stem}.svg"><img src="{stem}.png" loading="lazy"></a></section>')
    failed = sum(1 for w in run.walks if w.outcome == 'failed')
    return f'''<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Routes {escape(run.run_id)}</title>
<style>body{{font-family:-apple-system,Helvetica,sans-serif;margin:16px;background:#fafafa;color:#111}}
img{{max-width:100%;border:1px solid #ccc;background:#fff}}section{{margin:0 0 28px}}h3{{margin:4px 0;font-size:15px}}
p{{margin:2px 0 6px;font-size:13px;color:#555}}p.failed{{color:#b00000;font-weight:bold}}</style></head>
<body><h1>{escape(run.program)} &mdash; {escape(run.time)}</h1>
<p>{len(run.walks)} walk(s), {failed} failed. Walked: solid, wider where walked more often; pale striped where
assumed after an interrupt; orange dotted where the tile isn't known. The plan is dashed on top.</p>
<h2>Maps, per checkpoint attempt</h2>
{chr(10).join(maps_html)}
<h2>Walks</h2>
{chr(10).join(rows)}
</body></html>
'''


def render_index(results, out_dir):
    """The top index over several runs: [(run, sub_dir, pictures)]."""
    items = ''.join(
        f'<li><a href="{escape(sub)}/index.html">{escape(run.run_id)}</a> {escape(run.program)}: '
        f'{len(run.walks)} walk(s), {sum(1 for w in run.walks if w.outcome == "failed")} failed</li>'
        for run, sub, _ in results)
    html = (f'<!doctype html><html><head><meta charset="utf-8"><title>Route runs</title></head>'
            f'<body style="font-family:sans-serif;margin:16px"><h1>Route runs</h1><ul>{items}</ul></body></html>\n')
    Path(out_dir, 'index.html').write_text(html, encoding='utf-8')
