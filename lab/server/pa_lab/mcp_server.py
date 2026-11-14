"""The attach-only read server: MCP over Streamable HTTP on 127.0.0.1 (Slice 2 of fork #131).

One POST endpoint, `/mcp`, answering JSON-RPC with `application/json` (no SSE stream: nothing
here pushes). Every request must carry `Authorization: Bearer <token>`, a loopback `Host`, and no
`Origin` other than this server's own; bodies are capped. Sessions follow the spec's
`Mcp-Session-Id`. The bearer token is never logged or returned.

Resources: `console://B/state` (console_state), `console://B/save` (save_resource).
Tools: `shot`, `ensure_game`. Nothing here presses a button, starts or stops a program, or
holds a lease: those arrive in Slices 3 and 5.
"""

from __future__ import annotations

import base64
from collections import OrderedDict
import hmac
import socketserver
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import logging
import re
import secrets
import threading
from typing import Any, Callable, Dict, Optional, Tuple

from pa_lab import contracts, shot, tools
from pa_lab.lab_reader import ConsoleReader, Unavailable

LOG = logging.getLogger('pa_lab.server')

ENDPOINT = '/mcp'
MAX_BODY_BYTES = 1 << 20
MAX_SESSIONS = 64
PROTOCOL_VERSIONS = ('2025-06-18', '2025-03-26')
SERVER_INFO = {'name': 'pa-lab', 'version': '0.2.0'}
RESOURCE_URI_RE = re.compile(r'^console://([A-Za-z0-9]{1,8})/(state|save)$')

# JSON-RPC error codes
PARSE_ERROR = -32700
INVALID_REQUEST = -32600
METHOD_NOT_FOUND = -32601
INVALID_PARAMS = -32602
RESOURCE_UNAVAILABLE = -32001
RESOURCE_NOT_FOUND = -32002

INSTRUCTIONS = (
    'Read-only view of lab console B. Read console://B/state before acting on the console and '
    'console://B/save for the last known save. shot returns a PNG newer than the call; ensure_game '
    'only verifies, it never navigates. Unknown observations are never success.')

TOOL_LIST = [
    {
        'name': 'shot',
        'title': 'Console screenshot',
        'description': 'The first console frame after this call starts, with its frame_seq. Read-only; needs no lease. '
                       'Refuses on timeout, stale video or a classified bad signal.',
        'inputSchema': {
            'type': 'object',
            'properties': {
                'console': {'type': 'string', 'enum': ['B']},
                'max_age_ms': {'type': 'integer', 'minimum': 100, 'maximum': 60000, 'default': 2000},
                'timeout_ms': {'type': 'integer', 'minimum': 100, 'maximum': 60000, 'default': 5000},
            },
            'required': ['console'],
            'additionalProperties': False,
        },
        'annotations': {'readOnlyHint': True, 'destructiveHint': False, 'idempotentHint': False},
    },
    {
        'name': 'ensure_game',
        'title': 'Verify the game in front',
        'description': "Checks the app's game (and screen) observations against what is expected and "
                       'returns them with their age. Verification only: it never goes HOME or launches '
                       'a title. An unknown observation does not verify.',
        'inputSchema': {
            'type': 'object',
            'properties': {
                'console': {'type': 'string', 'enum': ['B']},
                'expected': {'type': 'string', 'description': "game id, e.g. 'PokemonFRLG'"},
                'allowed_screens': {'type': 'array', 'items': {'type': 'string'}, 'minItems': 1},
                'max_age_ms': {'type': 'integer', 'minimum': 100, 'maximum': 600000, 'default': 5000},
            },
            'required': ['console', 'expected'],
            'additionalProperties': False,
        },
        'annotations': {'readOnlyHint': True, 'destructiveHint': False, 'idempotentHint': True},
    },
]


class RpcError(Exception):
    def __init__(self, code: int, message: str, data: Any = None):
        super().__init__(message)
        self.code = code
        self.message = message
        self.data = data


def refusal_doc(tool: str, refusal: Dict[str, Any]) -> Dict[str, Any]:
    return contracts.validate('refusal', {'schema': 1, 'request_id': None, 'tool': tool,
                                          'refusal': refusal}).data


def _json_text(value: Any) -> str:
    return json.dumps(value, indent=2, ensure_ascii=False)


class LabMcp:
    """JSON-RPC methods, without the transport. `consoles` is the allowlist: console id -> reader."""

    def __init__(self, consoles: Dict[str, ConsoleReader],
                 take_shot: Callable[..., Dict[str, Any]] = shot.take_shot):
        self.consoles = consoles
        self.take_shot = take_shot

    def call(self, method: str, params: Dict[str, Any]) -> Dict[str, Any]:
        if method == 'ping':
            return {}
        if method == 'resources/list':
            return {'resources': [r for c in sorted(self.consoles) for r in self._resources(c)]}
        if method == 'resources/templates/list':
            return {'resourceTemplates': []}
        if method == 'resources/read':
            return self._read_resource(params.get('uri'))
        if method == 'tools/list':
            return {'tools': TOOL_LIST}
        if method == 'tools/call':
            return self._call_tool(params.get('name'), params.get('arguments', {}))
        raise RpcError(METHOD_NOT_FOUND, 'method not found: {}'.format(method))

    @staticmethod
    def initialize_result(params: Dict[str, Any]) -> Dict[str, Any]:
        asked = params.get('protocolVersion')
        version = asked if asked in PROTOCOL_VERSIONS else PROTOCOL_VERSIONS[0]
        return {'protocolVersion': version, 'capabilities': {'resources': {}, 'tools': {}},
                'serverInfo': SERVER_INFO, 'instructions': INSTRUCTIONS}

    @staticmethod
    def _resources(console: str):
        return [
            {'uri': 'console://{}/state'.format(console), 'name': 'console-{}-state'.format(console),
             'title': 'Console {} state'.format(console), 'mimeType': 'application/json',
             'description': "The app's Lab/state.json with availability, the reported lease and what "
                            'blocks a run.'},
            {'uri': 'console://{}/save'.format(console), 'name': 'console-{}-save'.format(console),
             'title': 'Console {} save'.format(console), 'mimeType': 'application/json',
             'description': 'The last program-authored save fact: known, stale or unknown.'},
        ]

    def _console(self, console: Any) -> Optional[ConsoleReader]:
        # An id is looked up in the allowlist, never turned into a path.
        return self.consoles.get(console) if isinstance(console, str) else None

    def _read_resource(self, uri: Any) -> Dict[str, Any]:
        match = RESOURCE_URI_RE.match(uri) if isinstance(uri, str) else None
        if match is None:
            raise RpcError(RESOURCE_NOT_FOUND, 'Resource not found', {'uri': uri})
        reader = self._console(match.group(1))
        if reader is None:
            raise RpcError(RESOURCE_NOT_FOUND, 'Resource not found', {
                'uri': uri, 'refusal': {'code': 'console_not_allowed',
                                        'message': 'Console {} is not served.'.format(match.group(1))}})
        if match.group(2) == 'state':
            doc = reader.console_state()
        else:
            try:
                doc = reader.save_resource()
            except Unavailable as e:
                raise RpcError(RESOURCE_UNAVAILABLE, e.message, {'uri': uri, 'refusal': e.refusal()})
        return {'contents': [{'uri': uri, 'mimeType': 'application/json', 'text': _json_text(doc)}]}

    def _call_tool(self, name: Any, arguments: Any) -> Dict[str, Any]:
        if name not in ('shot', 'ensure_game'):
            raise RpcError(INVALID_PARAMS, 'Unknown tool: {}'.format(name))
        if not isinstance(arguments, dict):
            return self._refused(name, {'code': 'invalid_arguments', 'field': 'arguments',
                                        'message': 'expected an object'})
        reader = self._console(arguments.get('console'))
        if reader is None:
            return self._refused(name, {'code': 'console_not_allowed', 'message':
                                        'Console {!r} is not served; only {}.'.format(
                                            arguments.get('console'), ', '.join(sorted(self.consoles)))})
        try:
            if name == 'ensure_game':
                result = tools.ensure_game(reader, arguments)
                return {'content': [{'type': 'text', 'text': _json_text(result)}],
                        'structuredContent': result, 'isError': not result['verified']}
            result = self.take_shot(reader, arguments)
        except tools.InvalidArguments as e:
            return self._refused(name, e.refusal())
        except Unavailable as e:
            return self._refused(name, e.refusal())
        except shot.Refused as e:
            return self._refused(name, e.refusal)
        png = result.pop('png')
        return {'content': [{'type': 'image', 'data': base64.b64encode(png).decode('ascii'),
                             'mimeType': 'image/png'},
                            {'type': 'text', 'text': _json_text(result)}],
                'structuredContent': result, 'isError': False}

    @staticmethod
    def _refused(tool: str, refusal: Dict[str, Any]) -> Dict[str, Any]:
        doc = refusal_doc(tool, refusal)
        return {'content': [{'type': 'text', 'text': _json_text(doc)}], 'structuredContent': doc,
                'isError': True}


class Sessions:
    def __init__(self):
        self._lock = threading.Lock()
        self._clients: 'OrderedDict[str, str]' = OrderedDict()

    def open(self, client: str) -> str:
        session = secrets.token_hex(16)
        with self._lock:
            self._clients[session] = client
            while len(self._clients) > MAX_SESSIONS:
                self._clients.popitem(last=False)
        return session

    def client(self, session: str) -> Optional[str]:
        with self._lock:
            return self._clients.get(session)

    def close(self, session: str) -> bool:
        with self._lock:
            return self._clients.pop(session, None) is not None


class LabHttpServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address: Tuple[str, int], token: str, mcp: LabMcp):
        super().__init__(address, _Handler)
        self.token = token
        self.mcp = mcp
        self.sessions = Sessions()

    def server_bind(self):
        # HTTPServer.server_bind looks up the host's FQDN, a reverse DNS call that can hang for
        # half a minute on macOS; the name is never used here.
        socketserver.TCPServer.server_bind(self)
        self.server_name, self.server_port = self.server_address[:2]

    def allowed_authorities(self):
        port = self.server_address[1]
        return ('127.0.0.1:{}'.format(port), 'localhost:{}'.format(port))


class _Handler(BaseHTTPRequestHandler):
    server: LabHttpServer
    protocol_version = 'HTTP/1.1'

    def log_message(self, format, *args):  # the default writes every request to stderr
        LOG.debug('%s %s', self.address_string(), format % args)

    # ---- responses

    def _send(self, status: int, body: Optional[Dict[str, Any]] = None,
              headers: Optional[Dict[str, str]] = None) -> None:
        data = json.dumps(body).encode('utf-8') if body is not None else b''
        self.send_response(status)
        for key, value in (headers or {}).items():
            self.send_header(key, value)
        if body is not None:
            self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _http_error(self, status: int, message: str, headers: Optional[Dict[str, str]] = None) -> None:
        self._send(status, {'jsonrpc': '2.0', 'id': None, 'error': {'code': INVALID_REQUEST, 'message': message}},
                   headers)

    # ---- checks every method shares

    def _gate(self) -> bool:
        if self.path != ENDPOINT:
            self._http_error(404, 'Not found; the endpoint is {}'.format(ENDPOINT))
            return False
        authorities = self.server.allowed_authorities()
        if self.headers.get('Host') not in authorities:
            self._http_error(403, 'Host must be a loopback address of this server')
            return False
        origin = self.headers.get('Origin')
        if origin is not None and origin not in ['http://' + a for a in authorities]:
            self._http_error(403, 'Origin not allowed')
            return False
        auth = self.headers.get('Authorization', '')
        given = auth[len('Bearer '):] if auth.startswith('Bearer ') else ''
        if not hmac.compare_digest(given.encode('utf-8'), self.server.token.encode('utf-8')):
            self._http_error(401, 'Missing or wrong bearer token', {'WWW-Authenticate': 'Bearer'})
            return False
        version = self.headers.get('MCP-Protocol-Version')
        if version is not None and version not in PROTOCOL_VERSIONS:
            self._http_error(400, 'Unsupported MCP-Protocol-Version {}'.format(version))
            return False
        return True

    def _session(self) -> Optional[Tuple[str, str]]:
        session = self.headers.get('Mcp-Session-Id')
        if session is None:
            self._http_error(400, 'Missing Mcp-Session-Id; send initialize first')
            return None
        client = self.server.sessions.client(session)
        if client is None:
            self._http_error(404, 'Unknown or ended session')
            return None
        return session, client

    # ---- methods

    def do_GET(self):
        if self._gate():
            self._send(405, None, {'Allow': 'POST, DELETE'})

    def do_DELETE(self):
        if not self._gate():
            return
        found = self._session()
        if found is not None:
            self.server.sessions.close(found[0])
            self._send(200)

    def do_POST(self):
        if not self._gate():
            return
        length = self.headers.get('Content-Length')
        if length is None or not length.isdigit():
            self._http_error(411, 'Content-Length required')
            return
        if int(length) > MAX_BODY_BYTES:
            self.close_connection = True
            self._http_error(413, 'Body larger than {} bytes'.format(MAX_BODY_BYTES))
            return
        raw = self.rfile.read(int(length))
        if not self.headers.get('Content-Type', '').startswith('application/json'):
            self._http_error(415, 'Content-Type must be application/json')
            return
        try:
            message = json.loads(raw.decode('utf-8'))
        except (UnicodeDecodeError, ValueError):
            self._send(400, {'jsonrpc': '2.0', 'id': None, 'error': {'code': PARSE_ERROR, 'message': 'Parse error'}})
            return
        if not isinstance(message, dict) or message.get('jsonrpc') != '2.0':
            self._http_error(400, 'Expected one JSON-RPC 2.0 message (batches are not supported)')
            return
        if 'method' not in message:  # a response from the client: nothing asked for one
            self._send(202)
            return
        method = message['method']
        params = message.get('params') or {}
        if method == 'initialize':
            self._initialize(message, params)
            return
        found = self._session()
        if found is None:
            return
        if 'id' not in message:  # notifications (notifications/initialized, cancelled)
            self._send(202)
            return
        LOG.info('client=%s method=%s tool=%s', found[1], method,
                 params.get('name') if method == 'tools/call' else '-')
        try:
            if not isinstance(params, dict):
                raise RpcError(INVALID_PARAMS, 'params must be an object')
            reply = {'jsonrpc': '2.0', 'id': message['id'], 'result': self.server.mcp.call(method, params)}
        except RpcError as e:
            error = {'code': e.code, 'message': e.message}
            if e.data is not None:
                error['data'] = e.data
            reply = {'jsonrpc': '2.0', 'id': message['id'], 'error': error}
        except Exception:
            LOG.exception('method %s failed', method)
            reply = {'jsonrpc': '2.0', 'id': message['id'], 'error': {'code': -32603, 'message': 'Internal error'}}
        self._send(200, reply)

    def _initialize(self, message: Dict[str, Any], params: Any) -> None:
        if 'id' not in message or not isinstance(params, dict):
            self._http_error(400, 'initialize must be a request with params')
            return
        info = params.get('clientInfo') if isinstance(params.get('clientInfo'), dict) else {}
        client = str(info.get('name') or 'unnamed')[:64]
        session = self.server.sessions.open(client)
        LOG.info('client=%s initialize', client)
        self._send(200, {'jsonrpc': '2.0', 'id': message['id'], 'result': LabMcp.initialize_result(params)},
                   {'Mcp-Session-Id': session})


def make_server(port: int, token: str, consoles: Dict[str, ConsoleReader], **mcp_options) -> LabHttpServer:
    """Binds 127.0.0.1 only; port 0 picks a free one (tests)."""
    return LabHttpServer(('127.0.0.1', port), token, LabMcp(consoles, **mcp_options))
