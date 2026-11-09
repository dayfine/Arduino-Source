"""Write native decoder parity inputs from a clean, immutable Simulation checkout.

Example, from Arduino-Source:
  python3 devtools/runtime-map-tests/prepare_parity.py --publisher /path/to/PokemonSimulation --out /tmp/runtime-map-pairs.json
  /path/to/RuntimeMapTests /tmp/runtime-map-pairs.json

This generates test data only; the native executable performs the assertions.
No maps, manifests, consumer pins or game resources are changed.
"""

import argparse
import json
import re
from pathlib import Path
import subprocess
import sys


PUBLISHER_REVISION = '40717c7c809c415c751fd7a348dfc1895f7ec3d7'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--publisher', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--publisher-revision', default=PUBLISHER_REVISION,
                        help='Full immutable publisher SHA; defaults to the original v1 fixture pin')
    args = parser.parse_args()
    revision = args.publisher_revision
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        parser.error('Publisher revision must be a full lowercase commit SHA')
    publisher = args.publisher.resolve()
    def git(*arguments):
        return subprocess.check_output(['git', '-C', str(publisher), *arguments], text=True).strip()
    if git('rev-parse', 'HEAD') != revision or git('status', '--porcelain', '--untracked-files=no'):
        parser.error('Publisher must be a clean checkout of ' + revision)
    sys.path.insert(0, str(publisher / 'src'))
    from pokesim.maps.item_identity import enrich_map
    from pokesim.maps.runtime_map_coordinates import compact_map, legacy_map
    pairs = []
    for game in ('frlg', 'bdsp'):
        for path in sorted((publisher / 'data/games' / game / 'maps/runtime').glob('*.json')):
            source = legacy_map(json.loads(path.read_text(encoding='utf-8')))
            for variant in (source, enrich_map(source, game)):
                compact = compact_map(variant)
                pairs.append({'input': compact, 'expectation': legacy_map(compact)})
    if len(pairs) != 150:
        parser.error('Expected both variants of all 75 pinned maps')
    with args.out.open('x', encoding='utf-8') as stream:
        json.dump(pairs, stream)
    print('Prepared 150 full map records from ' + revision)


if __name__ == '__main__':
    main()
