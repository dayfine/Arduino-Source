"""Server config: the B-only allowlist, loopback bind and the bearer token file (Slice 2, fork #131).

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import json
import os
from pathlib import Path
import tempfile
import unittest

from pa_lab import config


class ConfigTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory(prefix='pa-lab-config-')
        self.addCleanup(tmp.cleanup)
        self.dir = Path(tmp.name)
        self.token = self.dir / 'token'
        config.create_token(self.token)

    def write(self, doc):
        path = self.dir / 'server.json'
        path.write_text(json.dumps(doc), encoding='utf-8')
        return path

    def good(self):
        return {'schema': 1, 'host': '127.0.0.1', 'port': 8765, 'token_file': str(self.token),
                'consoles': {'B': {'lab_dir': str(self.dir / 'Lab'), 'profile': 'lab-B'}}}

    def test_good_config_loads(self):
        loaded = config.load(self.write(self.good()))
        self.assertEqual(list(loaded.consoles), ['B'])
        self.assertEqual(loaded.consoles['B'].profile, 'lab-B')
        self.assertEqual(loaded.port, 8765)

    def test_refused_configs(self):
        cases = [
            dict(name='console A listed', input={'consoles.A': {'lab_dir': '/x/Lab'}}, expectation='consoles'),
            dict(name='no console', input={'consoles': {}}, expectation='consoles'),
            dict(name='bind to every interface', input={'host': '0.0.0.0'}, expectation='host'),
            dict(name='relative lab dir', input={'consoles.B': {'lab_dir': 'Lab'}}, expectation='consoles.B.lab_dir'),
            dict(name='unknown key', input={'mutations': True}, expectation='mutations'),
            dict(name='future schema', input={'schema': 2}, expectation='schema'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                doc = self.good()
                for dotted, value in case['input'].items():
                    node = doc
                    keys = dotted.split('.')
                    for key in keys[:-1]:
                        node = node[key]
                    node[keys[-1]] = value
                with self.assertRaises(config.ConfigError) as raised:
                    config.load(self.write(doc))
                self.assertIn(case['expectation'], str(raised.exception))

    def test_token_file_is_owner_only(self):
        self.assertEqual(os.stat(self.token).st_mode & 0o777, 0o600)
        self.assertGreaterEqual(len(config.read_token(self.token)), 32)
        os.chmod(self.token, 0o644)
        with self.assertRaises(config.ConfigError) as raised:
            config.read_token(self.token)
        self.assertIn('0644', str(raised.exception))

    def test_create_token_never_overwrites(self):
        with self.assertRaises(config.ConfigError):
            config.create_token(self.token)


if __name__ == '__main__':
    unittest.main()
