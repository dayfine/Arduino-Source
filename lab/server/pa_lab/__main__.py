"""Check or describe version-1 lab contract documents.

  python3 -m pa_lab check app_state ~/Library/Application\\ Support/SerialPrograms/Profiles/lab-B/Lab/state.json
  python3 -m pa_lab describe app_state
  python3 -m pa_lab kinds

Run from lab/server (or with lab/server on PYTHONPATH). Exit 0 valid, 1 invalid, 2 usage.
"""

from __future__ import annotations

import argparse
import sys

from pa_lab import contracts


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog='pa_lab', description='Check or describe lab contract documents.')
    sub = parser.add_subparsers(dest='command')
    check = sub.add_parser('check', help='validate one JSON file; prints field-level errors')
    check.add_argument('kind', choices=contracts.KINDS)
    check.add_argument('path')
    describe = sub.add_parser('describe', help='print the fields of a kind')
    describe.add_argument('kind', choices=contracts.KINDS)
    sub.add_parser('kinds', help='list kinds and who writes them')
    args = parser.parse_args(argv)

    if args.command == 'check':
        with open(args.path, 'rb') as f:
            data = f.read()
        try:
            parsed = contracts.parse(args.kind, data)
        except contracts.ContractError as e:
            for error in e.errors:
                print('error {}'.format(error))
            return 1
        print('ok {} schema {}'.format(args.kind, parsed.version))
        for path in parsed.unknown:
            print('unknown field kept: {}'.format(path))
        return 0
    if args.command == 'describe':
        print('\n'.join(contracts.describe(args.kind)))
        return 0
    if args.command == 'kinds':
        for kind in contracts.KINDS:
            print('{:15} {}'.format(kind, contracts.AUTHORS[kind]))
        return 0
    parser.print_help()
    return 2


if __name__ == '__main__':
    sys.exit(main())
