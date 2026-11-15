"""maps: decode a map's grid from the decomp, fetch a map image, and draw a grid and logged routes over it.

    python3 -m devtools.maps decomp ROUTE_204_NORTH                       # C++ rows and header, to paste
    python3 -m devtools.maps decomp ROUTE_204_NORTH --text > r204n.txt    # bare rows, for overlay or diff
    python3 -m devtools.maps fetch Sandgem_Town_DP.png
    python3 -m devtools.maps overlay <Data/..._SandgemTown.cpp> [--log SerialPrograms.log [--walk LABEL]] -o out.svg
    python3 -m devtools.maps overlay r204n.txt --image ~/.cache/maps/Sinnoh_Route_204_DP.png --row-px 12 --level 12:24 -o r204n.svg
"""
import argparse
import sys
from pathlib import Path

from . import decomp, fetch, grid_source, route_log, svg

CACHE = Path.home() / '.cache' / 'maps'


def level(text):
    """ROW:PX, rows from ROW down drawn PX further down the image (a cliff level)."""
    row, px = text.split(':')
    return int(row), int(px)


def main(argv=None):
    parser = argparse.ArgumentParser(prog="maps")
    sub = parser.add_subparsers(dest='command', required=True)
    dec = sub.add_parser('decomp', help="decode a map's tile grid and objects from the pret decompilation")
    dec.add_argument('map', help='the map header name, e.g. ROUTE_204_NORTH or RAVAGED_PATH')
    dec.add_argument('--repo', default=decomp.Platinum.REPO, choices=[decomp.Platinum.REPO, decomp.Diamond.REPO],
                     help='which decompilation to read (default: pret/pokeplatinum)')
    dec.add_argument('--rect', nargs=4, type=int, metavar=('X0', 'Z0', 'W', 'H'),
                     help="world rectangle to decode; default: the map's own cell(s)")
    dec.add_argument('--text', action='store_true', help='print the bare rows instead of the C++ block')
    dec.add_argument('--local', type=Path, help='a checkout of the decompilation, instead of fetching with gh')
    dec.add_argument('--cache', type=Path, default=CACHE)
    get = sub.add_parser('fetch', help='download a Bulbapedia map image into the cache')
    get.add_argument('name', help='file name on Bulbapedia, e.g. Sandgem_Town_DP.png')
    get.add_argument('--cache', type=Path, default=CACHE)
    draw = sub.add_parser('overlay', help='draw a grid, and logged routes, over its map image')
    draw.add_argument('source', type=Path, help='the C++ data file with the grid, or a text file of bare rows')
    draw.add_argument('--image', type=Path, help='the map image; default: the one the file names, fetched')
    draw.add_argument('--origin', type=lambda s: tuple(int(v) for v in s.split(',')), metavar='X,Y',
                      help='pixel of tile (0, 0) on the image; default: from the file header, or 0,0')
    draw.add_argument('--row-px', type=int, default=0,
                      help='height of a tile row in pixels; default 16, the DP route images need 12')
    draw.add_argument('--level', type=level, action='append', default=[], metavar='ROW:PX',
                      help='rows from ROW down sit PX lower on the image (a cliff level; repeatable)')
    draw.add_argument('--log', type=Path, help='a SerialPrograms log to take planned walks from')
    draw.add_argument('--walk', action='append', help='only walks with this label (repeatable)')
    draw.add_argument('--last', type=int, default=0, help='only the last N walks')
    draw.add_argument('--cache', type=Path, default=CACHE)
    draw.add_argument('-o', '--out', type=Path, required=True)
    args = parser.parse_args(argv)

    if args.command == 'decomp':
        game = decomp.game_for(args.repo, args.cache / 'decomp', args.local)
        decoded = decomp.decode(game, args.map, tuple(args.rect) if args.rect else None)
        print(decomp.render_text(decoded) if args.text else decomp.render_cpp(decoded))
        return 0
    if args.command == 'fetch':
        print(fetch.fetch(args.name, args.cache))
        return 0

    source = grid_source.load(args.source)
    if args.origin:
        source.origin = args.origin
    if args.row_px:
        source.row_px = args.row_px
    source.levels = sorted(args.level)
    image = args.image
    if image is None and source.image:
        image = fetch.fetch(source.image, args.cache)
    walks = []
    if args.log:
        lines = args.log.read_text(encoding='utf-8', errors='replace').splitlines()
        walks = route_log.parse(lines)
        if args.walk:
            walks = [w for w in walks if w.label in args.walk]
        walks = [w for w in walks if any(p[0] == source.name for p in w.start + [e for l in w.legs for e in l.ends])]
        if args.last:
            walks = walks[-args.last:]
    title = f'{source.name}: {len(source.rows)} rows x {source.width}, {len(walks)} walk(s)'
    args.out.write_text(svg.render(source, image.read_bytes() if image else None, walks, title), encoding='utf-8')
    print(args.out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
