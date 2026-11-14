"""Check or describe version-1 lab contract documents, or run the read-only lab server.

  python3 -m pa_lab check app_state ~/Library/Application\\ Support/SerialPrograms/Profiles/lab-B/Lab/state.json
  python3 -m pa_lab describe app_state
  python3 -m pa_lab kinds
  python3 -m pa_lab init-token ~/.config/pa-lab/token
  python3 -m pa_lab serve --config ~/.config/pa-lab/server.json

Run from lab/server (or with lab/server on PYTHONPATH). Exit 0 valid, 1 invalid, 2 usage.
"""

from __future__ import annotations

import argparse
import logging
import sys

from pa_lab import config, contracts


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog='pa_lab', description='Check or describe lab contract documents.')
    sub = parser.add_subparsers(dest='command')
    check = sub.add_parser('check', help='validate one JSON file; prints field-level errors')
    check.add_argument('kind', choices=contracts.KINDS)
    check.add_argument('path')
    describe = sub.add_parser('describe', help='print the fields of a kind')
    describe.add_argument('kind', choices=contracts.KINDS)
    sub.add_parser('kinds', help='list kinds and who writes them')
    init_token = sub.add_parser('init-token', help='write a new owner-only bearer token file')
    init_token.add_argument('path')
    serve = sub.add_parser('serve', help='run the read-only MCP server (Streamable HTTP on 127.0.0.1)')
    serve.add_argument('--config', required=True)
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
    if args.command == 'init-token':
        try:
            config.create_token(args.path)
        except config.ConfigError as e:
            print('error {}'.format(e))
            return 1
        print('wrote {}'.format(args.path))
        return 0
    if args.command == 'serve':
        return _serve(args.config)
    parser.print_help()
    return 2


def _serve(path: str) -> int:
    from pa_lab import lab_reader, mcp_server
    try:
        cfg = config.load(path)
        token = config.read_token(cfg.token_file)
    except config.ConfigError as e:
        print('error {}'.format(e))
        return 1
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(name)s %(message)s')
    readers = {name: lab_reader.ConsoleReader(name, entry.lab_dir, profile=entry.profile)
               for name, entry in cfg.consoles.items()}
    server = mcp_server.make_server(cfg.port, token, readers)
    logging.getLogger('pa_lab.server').info('serving %s on http://127.0.0.1:%d%s',
                                            ', '.join(sorted(readers)), server.server_address[1],
                                            mcp_server.ENDPOINT)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == '__main__':
    sys.exit(main())
