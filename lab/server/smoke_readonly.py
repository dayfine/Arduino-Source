"""Smoke test: start the read-only stdio server the way .mcp.json does and call console_status once.

  ~/.venvs/lab-claude/bin/python lab/server/smoke_readonly.py [PROFILE_DIR]

It only reads files: the server it starts serves no tool that writes into Lab/inbox. Prints the
tool list and the console_status result; exit 0 when both came back, 1 otherwise.
"""

import json
import os
from pathlib import Path
import select
import subprocess
import sys

SERVER_DIR = Path(__file__).resolve().parent
DEFAULT_PROFILE = Path.home() / 'Library/Application Support/SerialPrograms/Profiles/lab-B'
REPLY_TIMEOUT_S = 15


def main(argv) -> int:
    profile = Path(argv[1]) if len(argv) > 1 else DEFAULT_PROFILE
    env = dict(os.environ, PYTHONPATH=str(SERVER_DIR))
    proc = subprocess.Popen([sys.executable, '-m', 'pa_lab', 'serve-stdio', '--profile-dir', str(profile)],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                            text=True, env=env)

    def ask(message):
        proc.stdin.write(json.dumps(message) + '\n')
        proc.stdin.flush()
        if 'id' not in message:
            return None
        ready, _, _ = select.select([proc.stdout], [], [], REPLY_TIMEOUT_S)
        if not ready:
            raise TimeoutError('no reply to {} within {} s'.format(message['method'], REPLY_TIMEOUT_S))
        return json.loads(proc.stdout.readline())

    try:
        init = ask({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
                    'params': {'protocolVersion': '2025-06-18', 'capabilities': {},
                               'clientInfo': {'name': 'smoke'}}})
        print('server:', init['result']['serverInfo'])
        ask({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
        tools = ask({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'})
        print('tools:', [t['name'] for t in tools['result']['tools']])
        reply = ask({'jsonrpc': '2.0', 'id': 3, 'method': 'tools/call',
                     'params': {'name': 'console_status', 'arguments': {'console': 'B', 'log_lines': 5}}})
        print(json.dumps(reply['result']['structuredContent'], indent=2, ensure_ascii=False))
        return 1 if reply['result']['isError'] else 0
    except (TimeoutError, KeyError, ValueError) as e:
        print('smoke failed: {}'.format(e))
        return 1
    finally:
        proc.stdin.close()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == '__main__':
    sys.exit(main(sys.argv))
