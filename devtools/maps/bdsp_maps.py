"""Checks that Packages' BDSP AutoStory maps are the ones in this repository.

Usage:
    python3 devtools/maps/bdsp_maps.py --check --json-dir Packages/Resources/PokemonBDSP/Maps

The maps are JSON files in the shared map schema
(SerialPrograms/Source/Pokemon/Navigation/Pokemon_MapData.h). They live in
SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Maps, where CMake builds
them into the program (pa_embed_text_files()). Once Packages has copies in
Resources/PokemonBDSP/Maps, the two must be the same files: --check exits
1, naming the first that differs, if they are not.

TODO: move the maps to PokemonAutomation/Packages (Resources/PokemonBDSP/Maps) when ready.
Until then the program reads only the repository's copies, and this check
is for that move.

The files were converted once (at ddcbdb21c) from the C++ tables that
pret/pokediamond's maps had been decoded into; nothing builds them now.
"""
import argparse
import os
import sys

REPO_MAPS = 'SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/Maps'


def json_files(folder):
    return sorted(name for name in os.listdir(folder) if name.endswith('.json'))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--check', action='store_true', required=True,
                        help='compare the repository\'s maps with --json-dir\'s, byte for byte')
    parser.add_argument('--json-dir', required=True, help='Packages\' Resources/PokemonBDSP/Maps')
    parser.add_argument('--repo', default='.', help='Arduino-Source checkout')
    args = parser.parse_args()
    repo_maps = os.path.join(args.repo, REPO_MAPS)
    ours, theirs = json_files(repo_maps), json_files(args.json_dir)
    if ours != theirs:
        print('The map files differ: only here ' + str(sorted(set(ours) - set(theirs))) +
              ', only in ' + args.json_dir + ' ' + str(sorted(set(theirs) - set(ours))) + '.')
        sys.exit(1)
    for name in ours:
        with open(os.path.join(repo_maps, name), 'rb') as a, open(os.path.join(args.json_dir, name), 'rb') as b:
            if a.read() != b.read():
                print(name + ': differs from ' + os.path.join(args.json_dir, name) + '.')
                sys.exit(1)
    print(str(len(ours)) + ' maps: the same files.')


if __name__ == '__main__':
    main()
