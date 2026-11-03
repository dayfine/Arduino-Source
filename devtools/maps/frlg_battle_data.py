"""Writes the FRLG battle data AutoStory chooses its attacks from, as JSON, from the pret/pokefirered decompilation.

Usage:
    python3 devtools/maps/frlg_battle_data.py --out SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/BattleData
    python3 devtools/maps/frlg_battle_data.py --out <folder> --check

Reads, at frlg_maps.COMMIT (cached in --cache like frlg_maps.py):
- src/battle_main.c gTypeEffectiveness: type_chart.json, each pairing that
  isn't 1x, in tenths;
- src/data/pokemon/species_info.h: species.json, every species' two types
  and base Attack and Sp. Atk;
- src/data/battle_moves.h: moves.json, every move's power, type and PP;
- src/data/trainers.h and src/data/trainer_parties.h: trainers.json, every
  trainer's party: species, level, and the moves where the party gives them
  (not the Ruby/Sapphire placeholders, DUMMY_TRAINER_MON);
- data/maps/*/scripts.inc, map.json and data/scripts/trainers.inc:
  map_trainers.json, the trainers each map battles (trainerbattle_*
  TRAINER_X in its scripts or its objects').
Names are the program's slugs: SPECIES_MR_MIME is "mr-mime", MOVE_WATER_GUN
"water-gun", TYPE_WATER "water". One entry per line, so a change in pret
shows as a line in the diff. --check writes nothing: it regenerates the files
and fails if any differs.
"""
import argparse
import json
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frlg_maps  # noqa: E402

#   Slugs the plain rule (lower case, '_' to '-') gets wrong: the program's
#   names, from Resources/Pokemon/Pokedex/Pokedex-National.json and PokeAPI.
SPECIES_SLUGS = {'FARFETCHD': 'farfetch-d'}
MOVE_SLUGS = {
    'FAINT_ATTACK': 'feint-attack',
    'HI_JUMP_KICK': 'high-jump-kick',
    'SMELLING_SALT': 'smelling-salts',
    'VICE_GRIP': 'vise-grip',
}
#   Not species a battle can show.
SKIPPED_SPECIES = re.compile(r'NONE$|EGG$|OLD_UNOWN_')
#   gTypeEffectiveness's multipliers, in tenths.
MULTIPLIERS = {'TYPE_MUL_NO_EFFECT': 0, 'TYPE_MUL_NOT_EFFECTIVE': 5, 'TYPE_MUL_SUPER_EFFECTIVE': 20}


def slug(name, fixes=None):
    return (fixes or {}).get(name) or name.lower().replace('_', '-')


def type_slug(name):
    return name[len('TYPE_'):].lower()


def entries(text, prefix):
    """(NAME, body) for each designated initializer `[PREFIX_NAME] = { body }`."""
    pattern = re.compile(r'\[' + prefix + r'_(\w+)\]\s*=\s*\{((?:(?!\n    \[).)*?)\n    \},', re.S)
    return pattern.findall(text)


def field(body, name):
    match = re.search(r'\.' + name + r'\s*=\s*([^,\n]+(?:\{[^}]*\})?)', body)
    return match.group(1).strip() if match else None


def type_chart(text):
    table = re.search(r'gTypeEffectiveness\[\d+\]\s*=\s*\{(.*?)\};', text, re.S).group(1)
    chart = []
    for attack, defend, mul in re.findall(r'(TYPE_\w+),\s*(TYPE_\w+),\s*(TYPE_MUL_\w+)', table):
        if attack in ('TYPE_FORESIGHT', 'TYPE_ENDTABLE'):
            continue
        chart.append([type_slug(attack), type_slug(defend), MULTIPLIERS[mul]])
    return chart


def species(text):
    data = {}
    for name, body in entries(text, 'SPECIES'):
        if SKIPPED_SPECIES.search(name):
            continue
        types = re.search(r'\.types\s*=\s*\{(TYPE_\w+),\s*(TYPE_\w+)\}', body)
        stats = [int(field(body, f)) for f in ('baseAttack', 'baseSpAttack')]
        data[slug(name, SPECIES_SLUGS)] = [type_slug(types.group(1)), type_slug(types.group(2))] + stats
    return data


def moves(text):
    data = {}
    for name, body in entries(text, 'MOVE'):
        if name == 'NONE':
            continue
        data[slug(name, MOVE_SLUGS)] = [int(field(body, 'power')), type_slug(field(body, 'type')), int(field(body, 'pp'))]
    return data


def parties(text):
    """sParty_X: [[species, level, [moves]?], ...]."""
    data = {}
    #   A placeholder's {DUMMY_TRAINER_MON} on one line doesn't match.
    for name, body in re.findall(r'struct TrainerMon\w+ (sParty_\w+)\[\]\s*=\s*\{\n((?:(?!\nstatic ).)*?)\n\};', text, re.S):
        party = []
        for mon in re.findall(r'\{(.*?)\n    \}', body, re.S):
            member = [slug(field(mon, 'species')[len('SPECIES_'):], SPECIES_SLUGS), int(field(mon, 'lvl'))]
            given = re.search(r'\.moves\s*=\s*\{(.*?)\}', mon)
            if given:
                member.append([slug(m.strip()[len('MOVE_'):], MOVE_SLUGS) for m in given.group(1).split(',') if m.strip() != 'MOVE_NONE'])
            party.append(member)
        data[name] = party
    return data


def trainers(text, party_text):
    by_party = parties(party_text)
    data = {}
    for name, body in entries(text, 'TRAINER'):
        if name == 'NONE':
            continue
        party = re.search(r'\.party\s*=\s*\w+\((sParty_\w+)\)', body)
        #   Left out: the Ruby/Sapphire placeholders (DUMMY_TRAINER_MON).
        if party and party.group(1) in by_party:
            data['TRAINER_' + name] = by_party[party.group(1)]
    return data


def script_trainers(text):
    """{label: TRAINER_X} for each script whose first line battles a trainer."""
    return dict(re.findall(r'^(\w+)::\n\s*trainerbattle\w* (TRAINER_\w+)', text, re.M))


def map_trainers(decomp):
    """The trainers each map battles: in its own scripts, and its objects'
    scripts in data/scripts/trainers.inc (most trainers on routes)."""
    groups = json.loads(decomp.read('data/maps/map_groups.json'))
    names = sorted(m for group in groups['group_order'] for m in groups[group])
    shared = script_trainers(decomp.read('data/scripts/trainers.inc').decode())
    read = lambda m: (decomp.read(f'data/maps/{m}/scripts.inc').decode(), json.loads(decomp.read(f'data/maps/{m}/map.json')))
    with ThreadPoolExecutor(16) as pool:
        sources = list(pool.map(read, names))
    data = {}
    for name, (scripts, source) in zip(names, sources):
        battled = set(re.findall(r'trainerbattle\w* (TRAINER_\w+)', scripts))
        battled |= {shared[o['script']] for o in source.get('object_events', []) if o.get('script') in shared}
        if battled:
            data[name] = sorted(battled)
    return data


def json_lines(data):
    """A JSON object (or a list), one entry per line."""
    if isinstance(data, list):
        lines = [json.dumps(item) for item in data]
        return '[\n' + ',\n'.join(lines) + '\n]\n'
    lines = [f'{json.dumps(key)}: {json.dumps(value)}' for key, value in data.items()]
    return '{\n' + ',\n'.join(lines) + '\n}\n'


def files(decomp):
    read = lambda rel: decomp.read(rel).decode()
    return {
        'type_chart': json_lines(type_chart(read('src/battle_main.c'))),
        'species': json_lines(species(read('src/data/pokemon/species_info.h'))),
        'moves': json_lines(moves(read('src/data/battle_moves.h'))),
        'trainers': json_lines(trainers(read('src/data/trainers.h'), read('src/data/trainer_parties.h'))),
        'map_trainers': json_lines(map_trainers(decomp)),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--commit', default=frlg_maps.COMMIT)
    parser.add_argument('--cache', default=os.path.expanduser('~/.cache/pokefirered'))
    parser.add_argument('--out', required=True, help='folder of the .json files')
    parser.add_argument('--check', action='store_true', help='check the files in --out instead of writing them')
    args = parser.parse_args()
    generated = files(frlg_maps.Decomp(args.commit, args.cache))
    if args.check:
        differ = []
        for name, text in generated.items():
            path = os.path.join(args.out, name + '.json')
            if not os.path.exists(path) or open(path, newline='').read() != text:
                differ.append(name)
        if differ:
            sys.exit(f'Differ from the generator: {" ".join(differ)}. Regenerate them without --check.')
        print(f'{len(generated)} files in {args.out} match the generator.')
        return
    os.makedirs(args.out, exist_ok=True)
    for name, text in generated.items():
        with open(os.path.join(args.out, name + '.json'), 'w', newline='') as out:
            out.write(text)


if __name__ == '__main__':
    main()
