"""Check or describe version-1 lab contract documents, or run the read-only lab server.

  python3 -m pa_lab check app_state ~/Library/Application\\ Support/SerialPrograms/Profiles/lab-B/Lab/state.json
  python3 -m pa_lab describe app_state
  python3 -m pa_lab kinds
  python3 -m pa_lab init-token ~/.config/pa-lab/token
  python3 -m pa_lab serve --config ~/.config/pa-lab/server.json [--read-only | --actions]
  python3 -m pa_lab serve-stdio --profile-dir ~/Library/Application\\ Support/SerialPrograms/Profiles/lab-B
  python3 -m pa_lab lease --config ~/.config/pa-lab/server.json show|claim-user|release-user

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
    serve.add_argument('--read-only', action='store_true',
                       help='serve only console_status and the resources: no inbox tools, no control plane')
    serve.add_argument('--actions', action='store_true',
                       help='also serve run_program and stop_program (Slice 5a): they start and stop the program')
    stdio = sub.add_parser('serve-stdio', help='run the read-only MCP server on stdio (for .mcp.json)')
    stdio.add_argument('--profile-dir', required=True, help="the console's SerialPrograms profile folder")
    stdio.add_argument('--keepawake-log', default='~/BDSPLab/keepawake.log')
    lease = sub.add_parser('lease', help="show console B's lease, or (operator) claim or release it for the user")
    lease.add_argument('--config', required=True)
    lease.add_argument('action', choices=('show', 'claim-user', 'release-user'))
    lease.add_argument('--console', default='B')
    args = parser.parse_args(argv)

    if args.command == 'check':
        try:
            with open(args.path, 'rb') as f:
                data = f.read()
        except OSError as e:
            print('error {}: {}'.format(args.path, e.strerror or e))
            return 2
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
        if args.read_only and args.actions:
            print('error --read-only and --actions exclude each other')
            return 2
        return _serve(args.config, args.read_only, args.actions)
    if args.command == 'serve-stdio':
        from pa_lab import stdio_server
        return stdio_server.main(args.profile_dir, args.keepawake_log)
    if args.command == 'lease':
        return _lease(args.config, args.action, args.console)
    parser.print_help()
    return 2


def _serve(path: str, read_only: bool = False, actions: bool = False) -> int:
    from pa_lab import lab_reader, mcp_server
    try:
        cfg = config.load(path)
        token = config.read_token(cfg.token_file)
    except config.ConfigError as e:
        print('error {}'.format(e))
        return 1
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(name)s %(message)s')
    if read_only:
        from pathlib import Path
        from pa_lab import status
        statuses = {name: status.ProfileStatus(name, Path(entry.lab_dir).parent,
                                               keepawake_log=Path.home() / 'BDSPLab' / 'keepawake.log')
                    for name, entry in cfg.consoles.items()}
        readers = {name: s.reader for name, s in statuses.items()}
        server = mcp_server.make_server(cfg.port, token, readers, statuses=statuses, read_only=True)
        return _serve_forever(server, readers, mcp_server.ENDPOINT)
    from pa_lab import control, db
    plane = control.ControlPlane(db.Database(cfg.db_file), sorted(cfg.consoles))
    readers = {name: lab_reader.ConsoleReader(name, entry.lab_dir, profile=entry.profile, control=plane)
               for name, entry in cfg.consoles.items()}
    for reader in readers.values():  # restart reconciliation: apply what the app shows now
        reader.console_state()
    plane.compact()
    program_actions = None
    if actions:
        from pa_lab import actions as actions_module
        program_actions = actions_module.ProgramActions(plane, readers)
    server = mcp_server.make_server(cfg.port, token, readers, control_plane=plane, actions=program_actions)
    return _serve_forever(server, readers, mcp_server.ENDPOINT)


def _serve_forever(server, readers, endpoint: str) -> int:
    logging.getLogger('pa_lab.server').info('serving %s on http://127.0.0.1:%d%s',
                                            ', '.join(sorted(readers)), server.server_address[1],
                                            endpoint)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


def _lease(path: str, action: str, console: str) -> int:
    import json
    from pa_lab import control, db
    try:
        cfg = config.load(path)
    except config.ConfigError as e:
        print('error {}'.format(e))
        return 1
    plane = control.ControlPlane(db.Database(cfg.db_file), sorted(cfg.consoles))
    try:
        if action == 'claim-user':
            lease = plane.claim_user(console)
        elif action == 'release-user':
            lease = plane.release_user(console)
        else:
            lease = plane.lease(console)
    except control.Refused as e:
        print('error {}'.format(e.refusal['message']))
        return 1
    print(json.dumps(lease, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
