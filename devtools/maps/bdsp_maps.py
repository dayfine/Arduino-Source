"""Writes the BDSP AutoStory maps as JSON, from their C++ tile tables.

Usage:
    python3 devtools/maps/bdsp_maps.py --out Packages/Resources/PokemonBDSP/Maps

Reads each map's rows from SerialPrograms/Source/PokemonBDSP/Programs/
AutoStory/Data/PokemonBDSP_AutoStory_<file>.cpp (the R"(...)" rows, in file
order) and writes <out>/<name>.json in the shared map schema
(SerialPrograms/Source/Pokemon/Navigation/Pokemon_MapData.h). The C++ tables
were derived from pret/pokediamond (each file's header says how); `source`
names the table and the commit it was read at, until this script reads the
decompilation itself.

Glyphs: the tables' '"' (tall grass) is the schema's '~'; a table's 'N' (a
person standing still) is an open tile with an "npc" object on it, which
the program's loader turns back into 'N'. The other glyphs ('#', '.', 'v',
'S', 'D') are the same in both.
"""
import argparse
import datetime
import json
import os
import re
import subprocess

DATA = 'SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Data'

#   (file name, C++ source file stem): the map files the program loads, in
#   the order of story_map_files() in PokemonBDSP_AutoStory_MapFiles.cpp.
MAPS = [
    ('TwinleafTown', 'TwinleafTown'),
    ('VerityLakefront', 'VerityLakefront'),
    ('LakeVerity', 'LakeVerity'),
    ('Route201', 'Route201'),
    ('SandgemTown', 'SandgemTown'),
    ('Route202', 'Route202'),
    ('JubilifeCity', 'JubilifeCity'),
    ('Route203', 'Route203'),
    ('OreburghGate', 'OreburghGate'),
    ('OreburghCity', 'OreburghCity'),
    ('OreburghMine', 'OreburghMine'),
    ('OreburghGym', 'OreburghGym'),
    ('Route204South', 'Route204South'),
    ('RavagedPath', 'RavagedPath'),
    ('Route204North', 'Route204'),
    ('FloaromaTown', 'FloaromaTown'),
]

ROW = re.compile(r'R"\((.*?)\)"')
TABLE_GLYPHS = set('#."vNSD')


def read_rows(path):
    with open(path, encoding='utf-8') as f:
        rows = ROW.findall(f.read())
    if not rows:
        raise SystemExit(f'{path}: no rows')
    width = len(rows[0])
    for y, row in enumerate(rows):
        if len(row) != width:
            raise SystemExit(f'{path}: row {y} is {len(row)} wide, not {width}')
        unknown = set(row) - TABLE_GLYPHS
        if unknown:
            raise SystemExit(f'{path}: row {y} has glyphs {sorted(unknown)}')
    return rows


def to_map(name, rows, source):
    tiles = []
    objects = []
    for y, row in enumerate(rows):
        line = ''
        for x, glyph in enumerate(row):
            if glyph == 'N':
                objects.append({'name': f'npc_{x}_{y}', 'kind': 'npc', 'x': x, 'y': y})
                glyph = '.'
            elif glyph == '"':
                glyph = '~'
            line += glyph
        tiles.append(line)
    data = {'name': name, 'source': source, 'tiles': tiles}
    if objects:
        data['objects'] = objects
    return data


def write_map(path, data):
    #   One row per line, so a diff of two versions shows the changed rows.
    lines = ['{']
    lines.append(f'    "name": {json.dumps(data["name"])},')
    lines.append(f'    "source": {json.dumps(data["source"])},')
    lines.append('    "tiles": [')
    lines += [f'        {json.dumps(row)},' for row in data['tiles']]
    lines[-1] = lines[-1].rstrip(',')
    objects = data.get('objects', [])
    lines.append('    ]' + (',' if objects else ''))
    if objects:
        lines.append('    "objects": [')
        lines += [f'        {json.dumps(item)},' for item in objects]
        lines[-1] = lines[-1].rstrip(',')
        lines.append('    ]')
    lines.append('}')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--out', required=True, help='folder for the .json files')
    parser.add_argument('--repo', default='.', help='Arduino-Source checkout')
    args = parser.parse_args()
    revision = subprocess.run(['git', '-C', args.repo, 'rev-parse', '--short=12', 'HEAD'],
                              capture_output=True, text=True, check=True).stdout.strip()
    date = datetime.date.today().isoformat()
    os.makedirs(args.out, exist_ok=True)
    for name, stem in MAPS:
        cpp = f'{DATA}/PokemonBDSP_AutoStory_{stem}.cpp'
        rows = read_rows(os.path.join(args.repo, cpp))
        source = {'decomp_file': cpp, 'revision': revision, 'date': date}
        write_map(os.path.join(args.out, f'{name}.json'), to_map(name, rows, source))
        print(f'{name}: {len(rows[0])} x {len(rows)}')


if __name__ == '__main__':
    main()
