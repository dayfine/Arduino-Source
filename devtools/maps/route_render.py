"""Draws each route walk of a trace on its map(s): the plan thin and dashed, the walk solid.

One picture per walk and map it touched, as PNG and SVG, and an index.html
per run linking them in order. The walked line is as wide as the number of
times the player crossed that tile edge, so walking back and forth shows as
a fat line. Where the player's tile wasn't known (a leg stopped by a battle,
the holds that found them again) the line is dotted and isn't counted.
"""
import json
import re
from dataclasses import replace
from html import escape
from pathlib import Path

from . import route_maps
from .picture import Picture
from .route_trace import DIRECTIONS, to_json

TILE = 16
MARGIN = 22         # room for the coordinate labels
HEADER = 46
FOOTER = 30

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
    """([(map, tiles, certain)], first tile, last known tile) for the steps as walked."""
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
            out.extend((m, t, False) for m, t in pieces)
            at = None
            continue
        if step.end and step.outcome == 'stopped' and start is not None:
            pieces, end, certain = leg_path(maps, start, _with_ends(step.leg, step.end))
        else:
            pieces, end, certain = leg_path(maps, start, step.leg)
        out.extend((m, t, certain) for m, t in pieces)
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


def _centre(x, y):
    return MARGIN + x * TILE + TILE / 2, HEADER + y * TILE + TILE / 2


def _draw_tiles(pic, tmap):
    for y, row in enumerate(tmap.rows):
        for x in range(len(row)):
            kind = tmap.kind(x, y)
            pic.rect(MARGIN + x * TILE, HEADER + y * TILE, TILE, TILE, TILE_FILL.get(kind, '#ffffff'))
            g = row[x]
            if kind in (route_maps.LEDGE, route_maps.MAT, route_maps.ONE_WAY) and g not in '#.':
                pic.text(MARGIN + x * TILE + 5, HEADER + y * TILE + 4, g if g != 'J' else 'v', '#ffffff')
    for x, y, _ in tmap.objects:
        if tmap.contains(x, y):
            cx, cy = _centre(x, y)
            pic.circle(cx, cy, TILE * 0.3, '#ff4fd8', '#7a0060')
    for x, y, _ in tmap.warps:
        if tmap.contains(x, y):
            pic.rect(MARGIN + x * TILE + 1, HEADER + y * TILE + 1, TILE - 2, TILE - 2, None, stroke='#00e0ff')
    #  Coordinates every fifth tile.
    for x in range(0, tmap.width, 5):
        pic.text(MARGIN + x * TILE + 1, HEADER - 10, str(x), '#333333')
        pic.line(MARGIN + x * TILE, HEADER, MARGIN + x * TILE, HEADER + tmap.height * TILE, '#000000', 0.6, alpha=0.25)
    for y in range(0, tmap.height, 5):
        pic.text(1, HEADER + y * TILE + 4, str(y), '#333333')
        pic.line(MARGIN, HEADER + y * TILE, MARGIN + tmap.width * TILE, HEADER + y * TILE, '#000000', 0.6, alpha=0.25)


def _polyline(pic, tiles, colour, width, dash=None):
    for (ax, ay), (bx, by) in zip(tiles, tiles[1:]):
        x0, y0 = _centre(ax, ay)
        x1, y1 = _centre(bx, by)
        pic.line(x0, y0, x1, y1, colour, width, dash)


def edge_counts(paths, name):
    """Times each tile edge of map `name` was walked (certain paths only)."""
    counts = {}
    for m, tiles, certain in paths:
        if not certain or route_maps.key(m) != route_maps.key(name):
            continue
        for a, b in zip(tiles, tiles[1:]):
            if abs(a[0] - b[0]) + abs(a[1] - b[1]) != 1:
                continue
            e = (min(a, b), max(a, b))
            counts[e] = counts.get(e, 0) + 1
    return counts


def _wrap(text, width_px):
    chars = max(20, width_px // 6)
    out = []
    while text:
        out.append(text[:chars])
        text = text[chars:]
    return out


def draw(walk, name, tmap, maps, run_id, index):
    """One picture of `walk` on map `name` (`tmap` may be None: no grid, routes only)."""
    if tmap is None:
        tmap = _bounds_map(walk, name, maps)
    w = max(MARGIN + tmap.width * TILE + 4, 560)
    h = HEADER + tmap.height * TILE + FOOTER
    pic = Picture(w, h)
    _draw_tiles(pic, tmap)
    here = route_maps.key(name)

    #  The walk: dotted where unsure, solid and as wide as the times walked.
    paths, first, last = walked_paths(maps, walk)
    for m, tiles, certain in paths:
        if not certain and route_maps.key(m) == here:
            _polyline(pic, tiles, UNSURE_COLOUR, 2.5, dash=(2, 3))
    counts = edge_counts(paths, name)
    for (a, b), n in sorted(counts.items(), key=lambda kv: kv[1]):
        _polyline(pic, [a, b], WALK_COLOUR, min(TILE * 0.85, 4 + 3 * (n - 1)))
    for (a, b), n in counts.items():
        if n > 1:
            x0, y0 = _centre(*a)
            x1, y1 = _centre(*b)
            if n >= 3:
                pic.text((x0 + x1) / 2 - 2, (y0 + y1) / 2 - 3, str(min(n, 9)), '#ffffff')

    #  Plans, thin and dashed.
    for plan in walk.plans:
        pieces, start, end = plan_paths(maps, plan)
        colour = PLAN_COLOUR.get(plan.reason, '#1f4fd1')
        for m, tiles in pieces:
            if route_maps.key(m) == here:
                _polyline(pic, tiles, colour, 1.5, dash=(4, 3))
        if end is not None and route_maps.key(end[0]) == here:
            cx, cy = _centre(end[1], end[2])
            pic.circle(cx, cy, TILE * 0.42, '#ffffff', colour)
            pic.text(cx - 2, cy - 3, 'E', colour)

    start = first or next((_pick(p.start) for p in walk.plans if p.start), None)
    if start is not None and route_maps.key(start[0]) == here:
        cx, cy = _centre(start[1], start[2])
        pic.circle(cx, cy, TILE * 0.45, '#18a03a', '#0a4a1a')
        pic.text(cx - 2, cy - 3, 'S', '#ffffff')

    #  What happened on the way.
    for mark in walk.marks:
        at = next((p for p in mark.at if route_maps.key(p[0]) == here), None)
        if at is None:
            continue
        cx, cy = _centre(at[1], at[2])
        if mark.kind == 'failed':
            pic.cross(cx, cy, TILE * 0.55, '#000000', 5)
            pic.cross(cx, cy, TILE * 0.5, '#ff1010', 3)
        elif mark.kind == 'arrived':
            pic.circle(cx, cy, TILE * 0.45, '#ffffff', '#000000')
            pic.text(cx - 2, cy - 3, 'A', '#000000')
        elif mark.kind in MARK_STYLE:
            fill, letter = MARK_STYLE[mark.kind]
            ox = TILE * 0.4
            pic.circle(cx + ox, cy - ox, 6.5, fill, '#ffffff')
            pic.text(cx + ox - 2, cy - ox - 3, letter, '#ffffff')
    if last is not None and route_maps.key(last[0]) == here and walk.outcome != 'done':
        cx, cy = _centre(last[1], last[2])
        pic.circle(cx, cy, TILE * 0.25, '#000000')

    #  Title and legend.
    plans = len(walk.plans)
    event = walk.event if walk.event.startswith(walk.checkpoint) else f'{walk.checkpoint} {walk.event}'
    title = f'{run_id}  #{index:02d}  {event}'.strip() + (f'  (attempt {walk.attempt})' if walk.attempt > 1 else '')
    sub = f'{walk.label} on {name}: {walk.outcome}, {len(walk.steps)} step(s), {plans} plan(s)'
    pic.text(4, 3, title, '#000000', 1)
    pic.text(4, 14, sub, '#b00000' if walk.outcome == 'failed' else '#000000', 1)
    if walk.error:
        pic.text(4, 25, _wrap(walk.error, w - 8)[0], '#b00000')
    legend = 'plan: blue dashed (replan purple)  walked: red, wider = more times; orange dotted = unsure'
    pic.text(4, h - FOOTER + 6, legend, '#333333')
    pic.text(4, h - FOOTER + 17, 'S start  E plan end  A arrived  X failed  B battle  M message  T trainer  R found again', '#333333')
    return pic


def _bounds_map(walk, name, maps):
    """A blank map just big enough for the walk's tiles on `name` (no grid loaded for it)."""
    xs, ys = [0], [0]
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


def render_run(run, maps, out_dir):
    """Every walk of `run` into `out_dir`: pictures, trace.json and index.html. Returns the pictures."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    pictures = []
    for index, walk in enumerate(run.walks, 1):
        for name in maps_of(walk):
            tmap = maps.get(route_maps.key(name))
            pic = draw(walk, name, tmap, maps, run.run_id, index)
            what = walk.event if walk.label == walk.event else walk.label
            stem = f'{index:03d}_{slug(what)}_{slug(name)}'
            (out_dir / f'{stem}.png').write_bytes(pic.png())
            (out_dir / f'{stem}.svg').write_text(pic.svg(), encoding='utf-8')
            pictures.append((index, walk, name, stem, tmap is not None))
    (out_dir / 'trace.json').write_text(json.dumps(to_json([run]), indent=1), encoding='utf-8')
    (out_dir / 'index.html').write_text(_index(run, pictures), encoding='utf-8')
    return pictures


def _index(run, pictures):
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
<p>{len(run.walks)} walk(s), {failed} failed, {len(pictures)} picture(s). Plan: blue dashed (replans purple).
Walked: red, wider where walked more often; orange dotted where the tile isn't known.</p>
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
