"""Rules that keep fixes systematic: learned rules have tests, no special cases by name, planned walks."""
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from devtools.code_health import learned_rules, plan_lint, special_cases
from devtools.code_health.cpp_text import comment_lines, string_literals


REPO = Path(__file__).resolve().parents[3]
ENV = {key: value for key, value in os.environ.items() if key != 'GITHUB_ACTIONS'}
FRLG = 'SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/'
PLZA = 'SerialPrograms/Source/VideoGames/PokemonLZA/Programs/AutoStory/'
SHARED = 'SerialPrograms/Source/Pokemon/AutoStory/'


class CppTextTests(unittest.TestCase):
    def test_comments_skip_string_contents_and_keep_line_numbers(self):
        source = ('int a = 1; // first\r\n'
                  'std::string s = "// not a comment";\r\n'
                  '/* block\r\n * second */\r\n'
                  'char c = \'"\'; // after a quote char\r\n')
        self.assertEqual({1: 'first', 3: 'block', 4: 'second', 5: 'after a quote char'},
                         comment_lines(source))

    def test_string_literals_with_columns(self):
        found = string_literals('if (id == "17.03"){ f("a // b"); }\n')
        self.assertEqual([(1, 10, 17, '17.03'), (1, 22, 30, 'a // b')], found)


class CitationTests(unittest.TestCase):
    def cites(self, comment):
        return [(c.line, c.date, c.checkpoint) for c in
                learned_rules.citations(FRLG + 'X.cpp', comment)]

    def test_citation_shapes_seen_in_the_tree(self):
        cases = [
            ('//  measured (console B, 2026-11-12).\n', [(1, '2026-11-12', '')]),
            ('//  partner is out (03.08, console B 2026-11-14).\n', [(1, '2026-11-14', '03.08')]),
            ('//  Console B, FRLG 07.02 run 2, 2026-11-09: it\n', [(1, '2026-11-09', '07.02')]),
            ('//  a fade (BDSP\'s Oreburgh Gate, mean 46, lab-B\n//  2026-11-08). Check\n',
             [(1, '2026-11-08', '')]),
            ('//  (11.01 ride_bike, console B, 2026-11-12 05:42: "not riding")\n',
             [(1, '2026-11-12', '11.01')]),
            ('//  12.02b, 2026-11-14: no console named\n', []),
            ('//  measured on console B. The camera turns; 2026-11-14\n', []),
            ('const char* s = "console B, 2026-11-14";\n', []),
        ]
        for comment, expected in cases:
            with self.subTest(comment=comment):
                self.assertEqual(expected, self.cites(comment))


def cite(path, text):
    return learned_rules.test_cite(path, text)


class LearnedRuleTests(unittest.TestCase):
    SOURCE = '//  A press during a slide is lost (17.03, console B, 2026-11-15).\nvoid f();\n'

    def findings(self, tests, previous=None, added=(1,)):
        return learned_rules.check(FRLG + 'Walk.h', self.SOURCE, previous, set(added), tests)

    def test_new_citation_needs_a_test_of_its_date_and_checkpoint(self):
        found = self.findings([])
        self.assertEqual(1, len(found))
        self.assertIn('learned rule without a test: ' + FRLG + 'Walk.h:1 (17.03, console B, 2026-11-15)',
                      found[0].message)
        self.assertEqual('learned_rule_test', found[0].check)
        # Another day, or the same day at another checkpoint, is another finding.
        self.assertEqual(1, len(self.findings([cite(FRLG + 'Walk_Tests.cpp', '2026-11-14, 17.03')])))
        self.assertEqual(1, len(self.findings([cite(FRLG + 'Walk_Tests.cpp', '2026-11-15, 17.02')])))

    def test_a_test_or_replay_of_the_same_game_or_shared_code_covers_it(self):
        replay = FRLG + 'ConsoleFailures/17.03-21-12-slide.json'
        for test in (cite(FRLG + 'Walk_Tests.cpp', '//  17.03, console B, 2026-11-15'),
                     cite(replay, json.dumps({'about': 'lab-B 2026-11-15', 'checkpoint': '17.03'})),
                     cite(SHARED + 'Engine_Tests.cpp', '2026-11-15 17.03')):
            with self.subTest(test=test.path):
                self.assertEqual([], self.findings([test]))
        other_game = cite(PLZA + 'Walk_Tests.cpp', '17.03 2026-11-15')
        self.assertEqual(1, len(self.findings([other_game])))

    def test_old_and_untouched_citations_are_not_reported(self):
        reworded = '//  The slide loses presses (17.03, console B, 2026-11-15).\n'
        self.assertEqual([], self.findings([], previous=reworded))
        self.assertEqual([], self.findings([], added=(2,)))
        self.assertEqual([], learned_rules.check(FRLG + 'Walk_Tests.cpp', self.SOURCE, None, {1}, []))

    def test_deleting_the_last_test_of_a_citation_fails(self):
        code = learned_rules.citations(FRLG + 'Walk.h', self.SOURCE)
        test = cite(FRLG + 'Walk_Tests.cpp', '17.03 2026-11-15')
        lost = learned_rules.lost_coverage(code, [test], [])
        self.assertEqual(1, len(lost))
        self.assertIn('learned rule lost its last test', lost[0].message)
        self.assertEqual([], learned_rules.lost_coverage(code, [test], [test]))
        self.assertEqual([], learned_rules.lost_coverage(code, [], []))


class SpecialCaseTests(unittest.TestCase):
    EVENTS = {'right_of_takashi', 'rival_takes_bulbasaur'}

    def findings(self, line, path=FRLG + 'PokemonFRLG_AutoStoryWalk.cpp'):
        return [f.message.split('"')[1] for f in special_cases.check(path, line + '\n', {1}, self.EVENTS)]

    def test_ids_used_as_keys_are_reported(self):
        cases = [
            'if (checkpoint.id == "17.03"){',
            'if ("12.02b" != row.id){',
            'return step.name == "rival_takes_bulbasaur";',
            'auto it = waits.find("right_of_takashi");',
            'const auto& w = waits["right_of_takashi"];',
            'const std::map<std::string, int> WAITS{{"17.03", 2}, {"right_of_takashi", 1}};',
        ]
        expected = [['17.03'], ['12.02b'], ['rival_takes_bulbasaur'], ['right_of_takashi'],
                    ['right_of_takashi'], ['17.03', 'right_of_takashi']]
        for line, names in zip(cases, expected):
            with self.subTest(line=line):
                self.assertEqual(names, self.findings(line))

    def test_lists_calls_other_strings_tests_and_other_code_pass(self):
        for line in ('"01.01", "01.02", "01.03",',
                     'first = checkpoint_index(checkpoints, "01.02");',
                     'if (item == "bicycle"){',
                     'if (action == "taps"){',
                     'log("17.03 done");'):
            with self.subTest(line=line):
                self.assertEqual([], self.findings(line))
        line = 'if (checkpoint.id == "17.03"){'
        for path in (FRLG + 'PokemonFRLG_AutoStoryWalk_Tests.cpp', FRLG + 'StoryData/x.cpp',
                     'SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/PokemonFRLG_Rng.cpp'):
            with self.subTest(path=path):
                self.assertEqual([], self.findings(line, path))

    def test_event_ids_come_from_plans_without_handler_names_or_one_word_ids(self):
        plan = {'checkpoint': {'id': '17.03', 'events': [
            {'id': 'right_of_takashi', 'kind': 'route', 'action': 'run'},
            {'id': 'bicycle', 'kind': 'game_action', 'action': 'use_item'},
            {'id': 'use_item', 'kind': 'step'},
        ]}}
        events, handlers = special_cases.plan_names([json.dumps(plan), 'not json'])
        self.assertEqual({'right_of_takashi'}, events)
        self.assertEqual({'route', 'run', 'game_action', 'use_item', 'step'}, handlers)


def plan(events, game='frlg'):
    return json.dumps({'game': game, 'checkpoint': {'id': '17.03', 'events': events}}, indent=2)


class PlanLintTests(unittest.TestCase):
    PATH = FRLG + 'StoryData/frlg-17.03.plan.json'
    FRLG_LEGS = {'id': 'right_of_takashi', 'kind': 'legs', 'action': 'taps',
                 'args': {'legs': [{'direction': 'east', 'tiles': 1}]}, 'why': 'east then north'}
    PLZA_WALK = {'id': 'to_the_gate', 'kind': 'game_action', 'action': 'walk',
                 'args': {'legs': [[0, 4500], [90, 10000]]}, 'why': 'up the road'}
    ROUTE = {'id': 'below_giovanni', 'kind': 'route', 'action': 'run', 'args': {'target': {}}, 'why': 'planned'}

    def ids(self, text, previous=None, path=PATH):
        return [f.message.split()[2] for f in plan_lint.check(path, text, previous)]

    def test_new_legs_events_need_a_not_planned_reason(self):
        self.assertEqual(['right_of_takashi', 'to_the_gate'],
                         self.ids(plan([self.FRLG_LEGS, self.PLZA_WALK, self.ROUTE])))
        reasoned = dict(self.FRLG_LEGS, why='not planned: a tap beside a wall where a run overshoots')
        self.assertEqual([], self.ids(plan([reasoned, self.ROUTE])))

    def test_unchanged_legs_at_the_base_are_grandfathered_and_edits_are_not(self):
        base = plan([self.FRLG_LEGS, self.ROUTE])
        self.assertEqual([], self.ids(plan([self.FRLG_LEGS, self.ROUTE, dict(self.ROUTE, id='b')]), base))
        edited = dict(self.FRLG_LEGS, args={'legs': [{'direction': 'east', 'tiles': 2}]})
        self.assertEqual(['right_of_takashi'], self.ids(plan([edited, self.ROUTE]), base))

    def test_finding_line_other_files_and_counts(self):
        text = plan([self.ROUTE, self.FRLG_LEGS])
        line = plan_lint.check(self.PATH, text, None)[0].line
        self.assertIn('"id": "right_of_takashi"', text.split('\n')[line - 1])
        self.assertEqual([], self.ids(plan([self.FRLG_LEGS]), path=FRLG + 'StoryData/maps.json'))
        self.assertEqual(['not'], self.ids('{"checkpoint": 3}'))
        counts = plan_lint.counts([(self.PATH, plan([self.FRLG_LEGS, self.ROUTE])),
                                   (PLZA + 'StoryData/plza-01.01.plan.json', plan([self.PLZA_WALK], 'plza'))])
        self.assertEqual({'frlg': (1, 0), 'plza': (1, 0)}, counts)


class SystematicCliTests(unittest.TestCase):
    """The three rules through the command line, in a temporary Git repository."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for args in (('init', '-q'), ('config', 'user.email', 'test@example.invalid'),
                     ('config', 'user.name', 'Test')):
            self.git(*args)

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args])

    def write(self, path, text):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text)
        self.git('add', '-A')

    def commit(self):
        self.git('commit', '-qm', 'base revision')

    def cli(self):
        return subprocess.run([sys.executable, '-m', 'devtools.code_health', '--root', str(self.root),
                               '--base-ref', 'HEAD'], cwd=REPO, capture_output=True, text=True, env=ENV)

    def test_learned_rule_without_a_test_fails_until_a_test_cites_it(self):
        self.write(FRLG + 'Walk.h', '#pragma once\n')
        self.commit()
        self.write(FRLG + 'Walk.h', '#pragma once\n//  Presses in a slide are lost (console B, 2026-11-15).\n')
        result = self.cli()
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn('learned rule without a test: ' + FRLG + 'Walk.h:2 (console B, 2026-11-15)', result.stderr)
        self.write(FRLG + 'Walk_Tests.cpp', '//  The slide (console B, 2026-11-15).\n')
        result = self.cli()
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_deleting_the_last_test_fails_and_a_tag_waives_it(self):
        self.write(FRLG + 'Walk.h', '//  Presses in a slide are lost (console B, 2026-11-15).\n')
        self.write(FRLG + 'Walk_Tests.cpp', '//  The slide (console B, 2026-11-15).\n')
        self.commit()
        self.git('rm', '-q', FRLG + 'Walk_Tests.cpp')
        result = self.cli()
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn('learned rule lost its last test: ' + FRLG + 'Walk.h:1', result.stderr)
        self.write(FRLG + 'Walk.h', '//  code-health: allow learned_rule_test -- the image test for this lives '
                   'in CommandLineTests, outside this repository\n'
                   '//  Presses in a slide are lost (console B, 2026-11-15).\n')
        result = self.cli()
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn('learned_rule_test bypassed by the tag at line 1', result.stdout)

    def test_special_case_by_event_name_fails(self):
        self.write(FRLG + 'StoryData/frlg-17.03.plan.json',
                   plan([{'id': 'right_of_takashi', 'kind': 'route', 'action': 'run', 'why': 'planned'}]))
        self.write(FRLG + 'Walk.cpp', 'bool f(){\n    return true;\n}\n')
        self.commit()
        self.write(FRLG + 'Walk.cpp', 'bool f(const Step& step){\n    return step.name == "right_of_takashi";\n}\n')
        result = self.cli()
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn('special case by name: "right_of_takashi" at ' + FRLG + 'Walk.cpp:2', result.stderr)

    def test_plan_lint_blocks_new_legs_and_reports_counts(self):
        legs = PlanLintTests.FRLG_LEGS
        self.write(FRLG + 'StoryData/frlg-17.03.plan.json', plan([legs]))
        self.commit()
        self.write(FRLG + 'StoryData/frlg-17.03.plan.json', plan([legs, dict(legs, id='second')]))
        result = self.cli()
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn('hand-written legs: second in', result.stderr)
        self.assertNotIn('hand-written legs: right_of_takashi', result.stderr)
        self.assertIn('Plan lint: frlg: 2 legs events in the changed plans, 0 with', result.stdout)


if __name__ == '__main__':
    unittest.main()
