"""Golden parse/round-trip tests for the version-1 lab contracts (Slice 0, fork #131).

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import copy
import json
from pathlib import Path
import unittest

from pa_lab import contracts, legacy

FIXTURES = Path(__file__).resolve().parent.parent / 'contracts' / 'v1'


def goldens():
    for kind in contracts.KINDS:
        for path in sorted((FIXTURES / kind).glob('*.json')):
            yield kind, path


class GoldenTests(unittest.TestCase):
    def test_every_kind_has_a_golden(self):
        kinds = {kind for kind, _ in goldens()}
        self.assertEqual(kinds, set(contracts.KINDS))

    def test_goldens_parse_and_round_trip(self):
        for kind, path in goldens():
            with self.subTest(golden='{}/{}'.format(kind, path.name)):
                text = path.read_text(encoding='utf-8')
                parsed = contracts.parse(kind, text)
                self.assertEqual(parsed.version, 1)
                self.assertEqual(json.loads(contracts.dump(parsed)), json.loads(text))
                again = contracts.parse(kind, contracts.dump(parsed))
                self.assertEqual(again.data, parsed.data)

    def test_goldens_are_canonical_json(self):
        for kind, path in goldens():
            with self.subTest(golden='{}/{}'.format(kind, path.name)):
                text = path.read_text(encoding='utf-8')
                self.assertEqual(contracts.dump(contracts.parse(kind, text)), text)

    def test_unknown_fields_survive_and_are_listed(self):
        parsed = contracts.parse('app_state', (FIXTURES / 'app_state' / 'future_fields.json').read_text())
        self.assertEqual(sorted(parsed.unknown), [
            'build', 'observation.game.candidates', 'program.last_transition.note', 'video.fps'])
        self.assertEqual(parsed.data['build'], {'commit': 'abc1234', 'dirty': False})
        self.assertEqual(parsed.data['observation']['game']['candidates'], ['PokemonFRLG', 'PokemonBDSP'])

    def test_unknown_fields_survive_inside_an_embedding(self):
        doc = json.loads((FIXTURES / 'console_state' / 'available_user_lease.json').read_text())
        doc['app']['video']['fps'] = 30
        doc['daemon_build'] = 'x'
        parsed = contracts.validate('console_state', doc)
        self.assertEqual(sorted(parsed.unknown), ['app.video.fps', 'daemon_build'])
        self.assertEqual(parsed.data['app']['video']['fps'], 30)

    def test_parse_does_not_alias_the_input(self):
        doc = json.loads((FIXTURES / 'app_state' / 'future_fields.json').read_text())
        parsed = contracts.validate('app_state', doc)
        parsed.data['build']['commit'] = 'changed'
        self.assertEqual(doc['build']['commit'], 'abc1234')

    def test_bytes_input(self):
        raw = (FIXTURES / 'app_state' / 'stopped_ui.json').read_bytes()
        self.assertEqual(contracts.parse('app_state', raw).data['schema'], 1)
        with self.assertRaises(contracts.ContractError) as caught:
            contracts.parse('app_state', b'\xff\xfe{}')
        self.assertEqual(caught.exception.paths(), ['$'])


class InvalidTests(unittest.TestCase):
    def test_invalid_cases_fail_with_a_field_level_error(self):
        cases = json.loads((FIXTURES / 'invalid_cases.json').read_text(encoding='utf-8'))
        self.assertGreater(len(cases), 0)
        for case in cases:
            with self.subTest(case=case['name']):
                inp, want = case['input'], case['expectation']
                with self.assertRaises(contracts.ContractError) as caught:
                    if 'text' in inp:
                        contracts.parse(case['kind'], inp['text'])
                    else:
                        contracts.validate(case['kind'], inp['json'])
                errors = caught.exception.errors
                match = [e for e in errors if e.path == want['path'] and want['contains'] in e.message]
                self.assertTrue(match, 'want {} ~ {!r}, got {}'.format(want['path'], want['contains'],
                                                                        [str(e) for e in errors]))

    def test_version_error_stops_before_field_errors(self):
        doc = json.loads((FIXTURES / 'app_state' / 'stopped_ui.json').read_text())
        doc['schema'] = 2
        doc['program']['state'] = 'whatever'
        with self.assertRaises(contracts.ContractError) as caught:
            contracts.validate('app_state', doc)
        self.assertEqual(caught.exception.paths(), ['schema'])

    def test_all_field_errors_are_reported_together(self):
        doc = json.loads((FIXTURES / 'app_state' / 'stopped_ui.json').read_text())
        doc['video']['signal'] = 'bars'
        doc['instance']['pid'] = 0
        with self.assertRaises(contracts.ContractError) as caught:
            contracts.validate('app_state', doc)
        self.assertEqual(sorted(caught.exception.paths()), ['instance.pid', 'video.signal'])
        self.assertIn('app_state: ', str(caught.exception))

    def test_every_required_app_field_is_checked(self):
        golden = json.loads((FIXTURES / 'app_state' / 'stopped_ui.json').read_text())
        for top in golden:
            if top in ('schema', 'channel'):
                continue
            doc = copy.deepcopy(golden)
            del doc[top]
            with self.subTest(missing=top):
                with self.assertRaises(contracts.ContractError) as caught:
                    contracts.validate('app_state', doc)
                self.assertEqual(caught.exception.paths(), [top])

    def test_unknown_kind(self):
        with self.assertRaises(ValueError):
            contracts.validate('lab_state', {})


class LegacyTests(unittest.TestCase):
    """state.json and the legacy Lab/state written from the same app state must agree."""

    def test_legacy_twins_agree(self):
        twins = sorted((FIXTURES / 'legacy').glob('*.state'))
        self.assertEqual([p.stem for p in twins], ['no_program', 'running_lab', 'stopped_ui'])
        for path in twins:
            with self.subTest(twin=path.name):
                app = contracts.parse('app_state', (FIXTURES / 'app_state' / (path.stem + '.json')).read_text())
                fields = legacy.parse_legacy_state(path.read_text())
                self.assertEqual(legacy.legacy_view(app.data), legacy.comparable_legacy(fields))

    def test_every_legacy_key_maps(self):
        app = contracts.parse('app_state', (FIXTURES / 'app_state' / 'running_lab.json').read_text())
        self.assertEqual(sorted(legacy.legacy_view(app.data)), sorted(legacy.LEGACY_KEYS))

    def test_parse_keeps_equals_in_values_and_unknown_keys(self):
        fields = legacy.parse_legacy_state('error=a=b\nnew_key=1\n\n')
        self.assertEqual(fields, {'error': 'a=b', 'new_key': '1'})

    def test_parse_refuses_a_line_without_equals(self):
        with self.assertRaises(ValueError):
            legacy.parse_legacy_state('state STOPPED\n')

    def test_state_json_keeps_no_owner_token(self):
        app = contracts.parse('app_state', (FIXTURES / 'app_state' / 'running_lab.json').read_text())
        self.assertNotIn('pa-lab-daemon', json.dumps(app.data))
        self.assertEqual(legacy.legacy_view(app.data)['owner'], 'claimed')


class DescribeTests(unittest.TestCase):
    def test_describe_lists_fields_and_author(self):
        lines = contracts.describe('app_state')
        self.assertIn('app (Lab/state.json)', lines[0])
        self.assertTrue(any(line.strip().startswith('program: object (required)') for line in lines))
        self.assertTrue(any('source: ui|lab|program|error (required)' in line for line in lines))

    def test_every_kind_has_an_author(self):
        self.assertEqual(set(contracts.AUTHORS), set(contracts.KINDS))


if __name__ == '__main__':
    unittest.main()
