"""The read-only lab server over MCP's stdio transport, for a Claude Code `.mcp.json` entry.

Claude Code starts it as a child process and talks newline-delimited JSON-RPC on stdin/stdout, so
it needs no port, token or daemon. It is always read-only: `LabMcp(read_only=True)` serves
`console_status` and the `console://B/state` and `/save` resources, nothing that writes into
`Lab/inbox`, and no control plane. Logs go to stderr; stdout carries only protocol messages.

  python -m pa_lab serve-stdio --profile-dir "$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B"
"""

from __future__ import annotations

import json
import logging
from pathlib import Path
import sys
from typing import Any, Dict, IO, Optional

from pa_lab.mcp_server import (INVALID_PARAMS, INVALID_REQUEST, PARSE_ERROR, PROTOCOL_VERSIONS, SERVER_INFO,
                               LabMcp, RpcError)
from pa_lab import status

LOG = logging.getLogger('pa_lab.stdio')

INSTRUCTIONS = (
    'Read-only view of lab console B. console_status returns what ~/BDSPLab/checkin.sh reports '
    '(app pid, Lab/state, last run lines, last press, newest failure frame and screenshot, log tail). '
    'Nothing here presses a button, starts or stops a program, or writes to the app.')


def make_mcp(profile_dir: Path, console: str = 'B', keepawake_log: Optional[Path] = None) -> LabMcp:
    profile_status = status.ProfileStatus(console, profile_dir, keepawake_log=keepawake_log)
    return LabMcp({console: profile_status.reader}, statuses={console: profile_status}, read_only=True)


def handle(mcp: LabMcp, message: Any) -> Optional[Dict[str, Any]]:
    """One JSON-RPC message in, the reply out (None for notifications and client responses)."""
    if not isinstance(message, dict) or message.get('jsonrpc') != '2.0':
        return {'jsonrpc': '2.0', 'id': None, 'error': {'code': INVALID_REQUEST, 'message':
                                                        'Expected one JSON-RPC 2.0 message'}}
    if 'method' not in message or 'id' not in message:
        return None
    method = message['method']
    params = message.get('params') or {}
    if not isinstance(params, dict):
        return {'jsonrpc': '2.0', 'id': message['id'],
                'error': {'code': INVALID_PARAMS, 'message': 'params must be an object'}}
    try:
        if method == 'initialize':
            asked = params.get('protocolVersion')
            result = {'protocolVersion': asked if asked in PROTOCOL_VERSIONS else PROTOCOL_VERSIONS[0],
                      'capabilities': {'resources': {}, 'tools': {}},
                      'serverInfo': dict(SERVER_INFO, name='pa-lab-readonly'), 'instructions': INSTRUCTIONS}
        else:
            result = mcp.call(method, params, client='stdio')
    except RpcError as e:
        error = {'code': e.code, 'message': e.message}
        if e.data is not None:
            error['data'] = e.data
        return {'jsonrpc': '2.0', 'id': message['id'], 'error': error}
    except Exception:
        LOG.exception('method %s failed', method)
        return {'jsonrpc': '2.0', 'id': message['id'], 'error': {'code': -32603, 'message': 'Internal error'}}
    return {'jsonrpc': '2.0', 'id': message['id'], 'result': result}


def serve(mcp: LabMcp, stdin: IO[str], stdout: IO[str]) -> None:
    for line in stdin:
        if not line.strip():
            continue
        try:
            message = json.loads(line)
        except ValueError:
            reply = {'jsonrpc': '2.0', 'id': None, 'error': {'code': PARSE_ERROR, 'message': 'Parse error'}}
        else:
            reply = handle(mcp, message)
        if reply is not None:
            stdout.write(json.dumps(reply, ensure_ascii=False) + '\n')
            stdout.flush()


def main(profile_dir: str, keepawake_log: Optional[str]) -> int:
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(name)s %(message)s', stream=sys.stderr)
    path = Path(profile_dir).expanduser()
    if not path.is_dir():
        print('error --profile-dir {} is not a directory'.format(path), file=sys.stderr)
        return 1
    mcp = make_mcp(path, keepawake_log=Path(keepawake_log).expanduser() if keepawake_log else None)
    LOG.info('serving console B read-only from %s on stdio', path)
    serve(mcp, sys.stdin, sys.stdout)
    return 0
