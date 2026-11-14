"""The Streamable HTTP endpoint end to end against a fake lab (Slice 2, fork #131): auth, Origin,
sessions, resources, tools, and many readers at once while the app keeps rewriting its files.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import base64
import http.client
import json
import threading
import time
import unittest

from pa_lab import lab_reader, mcp_server, timestamps
from tests.fake_lab import PNG, FakeApp, FakeLab, golden

TOKEN = 'test-token-' + 'x' * 32


class WallClock:
    """FakeApp's clock in real time: the app thread ticks it, `sleep` really sleeps."""

    def __init__(self):
        self.on_sleep = []

    def now(self):
        return time.time()


class Client:
    def __init__(self, port, token=TOKEN):
        self.port = port
        self.token = token
        self.session = None
        self.next_id = 0

    def raw(self, method='POST', body=None, headers=None):
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)
        all_headers = {'Authorization': 'Bearer ' + self.token, 'Content-Type': 'application/json',
                       'Accept': 'application/json, text/event-stream'}
        if self.session is not None:
            all_headers['Mcp-Session-Id'] = self.session
        all_headers.update(headers or {})
        all_headers = {k: v for k, v in all_headers.items() if v is not None}
        data = json.dumps(body).encode('utf-8') if body is not None else None
        conn.request(method, '/mcp', body=data, headers=all_headers)
        response = conn.getresponse()
        payload = response.read()
        conn.close()
        return response, (json.loads(payload) if payload else None)

    def rpc(self, method, params=None, **headers):
        self.next_id += 1
        response, body = self.raw(body={'jsonrpc': '2.0', 'id': self.next_id, 'method': method,
                                        'params': params or {}}, headers=headers)
        return response.status, body

    def initialize(self):
        response, body = self.raw(body={'jsonrpc': '2.0', 'id': 0, 'method': 'initialize', 'params': {
            'protocolVersion': '2025-06-18', 'capabilities': {}, 'clientInfo': {'name': 'test'}}})
        self.session = response.getheader('Mcp-Session-Id')
        self.raw(body={'jsonrpc': '2.0', 'method': 'notifications/initialized'})
        return response, body

    def read(self, uri):
        status, body = self.rpc('resources/read', {'uri': uri})
        if 'error' in body:
            return body['error']
        return json.loads(body['result']['contents'][0]['text'])

    def tool(self, name, **arguments):
        status, body = self.rpc('tools/call', {'name': name, 'arguments': arguments})
        return body['result']


class ServerTest(unittest.TestCase):
    def setUp(self):
        self.lab = FakeLab(self)
        state = golden('app_state', 'running_lab')
        state['written_at'] = timestamps.format(time.time())
        state['video']['captured_at'] = state['written_at']
        self.lab.write_state(state)
        self.app = FakeApp(self.lab, WallClock())
        self.stop = threading.Event()
        self.app_thread = threading.Thread(target=self._run_app, daemon=True)
        self.app_thread.start()
        console = lab_reader.ConsoleReader('B', self.lab.dir, profile='lab-B', pid_alive=lambda pid: True)
        self.server = mcp_server.make_server(0, TOKEN, {'B': console})
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.addCleanup(self._shutdown)
        self.client = Client(self.server.server_address[1])

    def _run_app(self):
        for _ in range(2000):  # 20 ms ticks, at most 40 s
            if self.stop.wait(0.02):
                return
            self.app.tick()

    def _shutdown(self):
        self.stop.set()
        self.app_thread.join(5)
        self.server.shutdown()
        self.server.server_close()


class GateTests(ServerTest):
    def test_requests_without_the_right_credentials_are_refused(self):
        cases = [
            dict(name='no token', input={'Authorization': None}, expectation=401),
            dict(name='wrong token', input={'Authorization': 'Bearer nope'}, expectation=401),
            dict(name='foreign origin', input={'Origin': 'https://evil.example'}, expectation=403),
            dict(name='rebinding host', input={'Host': 'evil.example:80'}, expectation=403),
            dict(name='unknown protocol version', input={'MCP-Protocol-Version': '1999-01-01'}, expectation=400),
        ]
        self.client.initialize()
        for case in cases:
            with self.subTest(case['name']):
                status, body = self.client.rpc('ping', **case['input'])
                self.assertEqual(status, case['expectation'])
                self.assertNotIn(TOKEN, json.dumps(body))

    def test_own_origin_is_allowed(self):
        self.client.initialize()
        status, body = self.client.rpc('ping', Origin='http://127.0.0.1:{}'.format(self.client.port))
        self.assertEqual((status, body['result']), (200, {}))

    def test_session_required_after_initialize(self):
        status, _ = self.client.rpc('ping')
        self.assertEqual(status, 400)
        self.client.session = 'f' * 32
        status, _ = self.client.rpc('ping')
        self.assertEqual(status, 404)

    def test_delete_ends_the_session(self):
        self.client.initialize()
        response, _ = self.client.raw('DELETE')
        self.assertEqual(response.status, 200)
        status, _ = self.client.rpc('ping')
        self.assertEqual(status, 404)

    def test_get_has_no_stream(self):
        response, _ = self.client.raw('GET')
        self.assertEqual(response.status, 405)

    def test_oversized_body_is_refused(self):
        self.client.initialize()
        conn = http.client.HTTPConnection('127.0.0.1', self.client.port, timeout=10)
        self.addCleanup(conn.close)
        conn.putrequest('POST', '/mcp')
        for key, value in (('Authorization', 'Bearer ' + TOKEN), ('Content-Type', 'application/json'),
                           ('Mcp-Session-Id', self.client.session),
                           ('Content-Length', str(mcp_server.MAX_BODY_BYTES + 1))):
            conn.putheader(key, value)
        conn.endheaders()
        self.assertEqual(conn.getresponse().status, 413)


class ProtocolTests(ServerTest):
    def setUp(self):
        super().setUp()
        self.response, self.init = self.client.initialize()

    def test_initialize_offers_resources_and_tools(self):
        result = self.init['result']
        self.assertEqual(result['protocolVersion'], '2025-06-18')
        self.assertEqual(set(result['capabilities']), {'resources', 'tools'})
        self.assertIsNotNone(self.client.session)

    def test_lists_only_console_b(self):
        _, body = self.client.rpc('resources/list')
        self.assertEqual([r['uri'] for r in body['result']['resources']], ['console://B/state', 'console://B/save'])
        _, body = self.client.rpc('tools/list')
        self.assertEqual([t['name'] for t in body['result']['tools']], ['shot', 'ensure_game'])

    def test_state_resource(self):
        state = self.client.read('console://B/state')
        self.assertEqual(state['availability']['state'], 'available')
        self.assertEqual(state['app']['instance']['id'], 'd8ea6a9d-1eab-4a40-b355-d88072b289a1')

    def test_save_resource(self):
        self.assertEqual(self.client.read('console://B/save')['status'], 'unknown')
        self.lab.write_raw(self.lab.save_path, '{"schema": 1, "ga')
        error = self.client.read('console://B/save')
        self.assertEqual(error['code'], mcp_server.RESOURCE_UNAVAILABLE)
        self.assertEqual(error['data']['refusal']['code'], 'app_unavailable')

    def test_console_a_and_paths_are_not_resources(self):
        for uri in ('console://A/state', 'console://B/../../etc/passwd', 'file:///etc/passwd'):
            with self.subTest(uri):
                self.assertEqual(self.client.read(uri)['code'], mcp_server.RESOURCE_NOT_FOUND)

    def test_tools_refuse_console_a(self):
        result = self.client.tool('shot', console='A')
        self.assertTrue(result['isError'])
        self.assertEqual(result['structuredContent']['refusal']['code'], 'console_not_allowed')

    def test_shot_returns_the_png(self):
        result = self.client.tool('shot', console='B')
        self.assertFalse(result['isError'], result)
        image = result['content'][0]
        self.assertEqual((image['type'], image['mimeType']), ('image', 'image/png'))
        self.assertEqual(base64.b64decode(image['data']), PNG)
        self.assertGreater(result['structuredContent']['frame_seq_at_least'], 921930)

    def test_shot_timeout_is_a_refusal_document(self):
        self.app.frames = False
        result = self.client.tool('shot', console='B', timeout_ms=300)
        self.assertTrue(result['isError'])
        self.assertEqual(result['structuredContent']['refusal']['code'], 'timeout')
        self.assertEqual(json.loads(result['content'][0]['text']), result['structuredContent'])

    def test_ensure_game(self):
        result = self.client.tool('ensure_game', console='B', expected='PokemonBDSP')
        self.assertEqual(result['structuredContent']['reason'], 'observation_stale')  # the golden's is old
        self.assertTrue(result['isError'])
        result = self.client.tool('ensure_game', console='B', expected='Pokemon BDSP')
        self.assertEqual(result['structuredContent']['refusal']['field'], 'expected')

    def test_unknown_method_and_tool(self):
        _, body = self.client.rpc('tools/press')
        self.assertEqual(body['error']['code'], mcp_server.METHOD_NOT_FOUND)
        _, body = self.client.rpc('tools/call', {'name': 'run_program', 'arguments': {'console': 'B'}})
        self.assertEqual(body['error']['code'], mcp_server.INVALID_PARAMS)


class ConcurrencyTests(ServerTest):
    def test_many_readers_and_shots_while_the_app_writes(self):
        self.client.initialize()
        errors = []
        paths = []
        lock = threading.Lock()

        def reader():
            client = Client(self.client.port)
            client.initialize()
            for _ in range(15):
                state = client.read('console://B/state')
                if state['availability']['state'] != 'available':
                    with lock:
                        errors.append(state['blocked'])

        def shooter():
            client = Client(self.client.port)
            client.initialize()
            result = client.tool('shot', console='B')
            with lock:
                if result['isError']:
                    errors.append(result['structuredContent'])
                else:
                    paths.append(result['structuredContent']['path'])

        threads = [threading.Thread(target=reader) for _ in range(6)] + \
                  [threading.Thread(target=shooter) for _ in range(4)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join(30)
        self.assertEqual(errors, [])
        self.assertEqual(len(set(paths)), 4)


if __name__ == '__main__':
    unittest.main()
