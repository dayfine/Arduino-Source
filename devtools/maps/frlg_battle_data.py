"""Writes the FRLG battle data AutoStory chooses its attacks from, as JSON, from the pret/pokefirered decompilation.

Usage:
    python3 devtools/maps/frlg_battle_data.py --out SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/BattleData
    python3 devtools/maps/frlg_battle_data.py --out <folder> --check

Reads, at frlg_maps.COMMIT (cached in --cache like frlg_maps.py):
- src/battle_main.c gTypeEffectiveness: type_chart.json, each pairing that
  isn't 1x, in tenths;
- src/data/pokemon/species_info.h: species.json, every species' two types
  and base stats: HP, Attack, Defense, Sp. Atk, Sp. Def, Speed;
- src/data/battle_moves.h: moves.json, every move's power, type, PP,
  accuracy, priority, effect (EFFECT_FLINCH_HIT is "flinch-hit") and its
  secondary effect's chance;
- src/data/trainers.h and src/data/trainer_parties.h: trainers.json, every
  trainer's AI flags (AI_SCRIPT_TRY_TO_FAINT is "try-to-faint"), items and
  party (not the Ruby/Sapphire placeholders, DUMMY_TRAINER_MON): each
  member's species, level, iv (0 to 255, as pret has it), nature and moves.
  A party that gives no moves gets the game's (GiveBoxMonInitialMoveset:
  the last four of its level-up moves up to its level, from
  src/data/pokemon/level_up_learnsets.h). The nature is the one
  CreateNPCTrainerParty's personality gives: a hash of the trainer's and
  the species' names (charmap.txt) on top of 0x78 for a female trainer,
  0x80 for a double battle, else 0x88, mod 25;
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
#   The AI flags, as include/constants/battle_ai.h names them.
AI_FLAG = re.compile(r'AI_SCRIPT_(\w+)')
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
        stats = [int(field(body, f)) for f in ('baseHP', 'baseAttack', 'baseDefense', 'baseSpAttack', 'baseSpDefense', 'baseSpeed')]
        data[slug(name, SPECIES_SLUGS)] = [type_slug(types.group(1)), type_slug(types.group(2))] + stats
    return data


def moves(text):
    data = {}
    for name, body in entries(text, 'MOVE'):
        if name == 'NONE':
            continue
        data[slug(name, MOVE_SLUGS)] = [
            int(field(body, 'power')), type_slug(field(body, 'type')), int(field(body, 'pp')),
            int(field(body, 'accuracy')), int(field(body, 'priority')),
            slug(field(body, 'effect')[len('EFFECT_'):]), int(field(body, 'secondaryEffectChance')),
        ]
    return data


def learnsets(text, pointers):
    """{species slug: [(level, move slug), ...]} from level_up_learnsets.h."""
    by_name = {}
    for name, body in re.findall(r'static const u16 (s\w+LevelUpLearnset)\[\]\s*=\s*\{(.*?)\};', text, re.S):
        by_name[name] = [(int(lvl), slug(move, MOVE_SLUGS)) for lvl, move in re.findall(r'LEVEL_UP_MOVE\(\s*(\d+),\s*MOVE_(\w+)\)', body)]
    return {slug(s, SPECIES_SLUGS): by_name[n] for s, n in re.findall(r'\[SPECIES_(\w+)\]\s*=\s*(s\w+LevelUpLearnset)', pointers)}


def initial_moves(learnset, level):
    """GiveBoxMonInitialMoveset: each level-up move up to `level` in turn;
    one already known is skipped, a fifth pushes out the first."""
    known = []
    for move_level, move in learnset:
        if move_level > level:
            break
        if move in known:
            continue
        if len(known) == 4:
            known.pop(0)
        known.append(move)
    return known


def charmap(text):
    """{character: byte} for the single characters charmap.txt maps."""
    chars = {}
    for char, code in re.findall(r"^'(\\?.)'\s*=\s*([0-9A-F]{2})\s*$", text, re.M):
        chars[char[-1]] = int(code, 16)
    return chars


def name_sum(name, chars):
    return sum(chars[c] for c in name)


def species_names(text):
    return {slug(name, SPECIES_SLUGS): value for name, value in re.findall(r'\[SPECIES_(\w+)\]\s*=\s*_\("(.*?)"\)', text)}


def parties(text, moves_at):
    """sParty_X: [(species, level, iv, [moves]), ...]; `moves_at(species,
    level)` gives a member's moves where the party doesn't."""
    data = {}
    #   A placeholder's {DUMMY_TRAINER_MON} on one line doesn't match.
    for name, body in re.findall(r'struct TrainerMon\w+ (sParty_\w+)\[\]\s*=\s*\{\n((?:(?!\nstatic ).)*?)\n\};', text, re.S):
        party = []
        for mon in re.findall(r'\{(.*?)\n    \}', body, re.S):
            species, level = slug(field(mon, 'species')[len('SPECIES_'):], SPECIES_SLUGS), int(field(mon, 'lvl'))
            given = re.search(r'\.moves\s*=\s*\{(.*?)\}', mon)
            if given:
                moves = [slug(m.strip()[len('MOVE_'):], MOVE_SLUGS) for m in given.group(1).split(',') if m.strip() != 'MOVE_NONE']
            else:
                moves = moves_at(species, level)
            party.append((species, level, int(field(mon, 'iv')), moves))
        data[name] = party
    return data


def personality_base(body):
    """CreateNPCTrainerParty's personality before the name hash."""
    if field(body, 'doubleBattle') == 'TRUE':
        return 0x80
    if 'F_TRAINER_FEMALE' in field(body, 'encounterMusic_gender'):
        return 0x78
    return 0x88


def trainers(text, party_text, learned, names, chars):
    by_party = parties(party_text, lambda species, level: initial_moves(learned.get(species, []), level))
    data = {}
    for name, body in entries(text, 'TRAINER'):
        if name == 'NONE':
            continue
        party = re.search(r'\.party\s*=\s*\w+\((sParty_\w+)\)', body)
        #   Left out: the Ruby/Sapphire placeholders (DUMMY_TRAINER_MON).
        if not party or party.group(1) not in by_party:
            continue
        trainer_sum = name_sum(re.search(r'\.trainerName\s*=\s*_\("(.*?)"\)', body).group(1), chars)
        name_hash = 0
        members = []
        for species, level, iv, moves in by_party[party.group(1)]:
            name_hash += trainer_sum + name_sum(names[species], chars)
            nature = ((personality_base(body) + (name_hash << 8)) & 0xFFFFFFFF) % 25
            members.append([species, level, iv, nature, moves])
        items = re.search(r'\.items\s*=\s*\{(.*?)\}', body).group(1)
        data['TRAINER_' + name] = {
            'ai': [slug(flag) for flag in AI_FLAG.findall(field(body, 'aiFlags') or '')],
            'items': [slug(item.strip()[len('ITEM_'):]) for item in items.split(',') if item.strip() not in ('', 'ITEM_NONE')],
            'party': members,
        }
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
        'trainers': json_lines(trainers(
            read('src/data/trainers.h'), read('src/data/trainer_parties.h'),
            learnsets(read('src/data/pokemon/level_up_learnsets.h'), read('src/data/pokemon/level_up_learnset_pointers.h')),
            species_names(read('src/data/text/species_names.h')), charmap(read('charmap.txt')),
        )),
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
