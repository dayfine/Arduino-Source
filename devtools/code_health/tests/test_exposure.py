import subprocess
import tempfile
import unittest
from pathlib import Path

from devtools.code_health.exposure import check, git_users, test_file


DIR = 'SerialPrograms/Source/VideoGames/PokemonLZA/Programs/'
HEADER = DIR + 'PokemonLZA_BallThrow.h'
OWN_CPP = DIR + 'PokemonLZA_BallThrow.cpp'
TESTS = DIR + 'PokemonLZA_BallThrow_Tests.cpp'
CALLER = DIR + 'Farming/PokemonLZA_InPlaceCatcher.cpp'


def header(body):
    return ('namespace PokemonAutomation{\nnamespace PokemonLZA{\n' + body
            + '\n} // namespace PokemonLZA\n} // namespace PokemonAutomation\n')


# Fork #590 before its review: two helpers exposed next to the public throw_ball().
BALL_THROW = header(
    'struct BallThrowOptions{\n    int aim = 500;\n};\n'
    'struct BallScrolls{\n    int count;\n};\n'
    'BallScrolls ball_scrolls(int right_scrolls);\n'
    'int throw_buttons_hold(const BallThrowOptions& options);\n'
    'void throw_ball(Context& context, const BallThrowOptions& options);\n'
    'void add_tests_BallThrow(UnitTestDatabase& database);')


def fake_users(table):
    return lambda names: {name: set(table.get(name, ())) for name in names}


def names(findings):
    return [f.message.split("'")[1] for f in findings]


class TestOnlyExposureTests(unittest.TestCase):
    def test_flags_helpers_only_tests_or_their_own_cpp_use(self):
        users = fake_users({
            'BallThrowOptions': [HEADER, OWN_CPP, CALLER],
            'BallScrolls': [HEADER, OWN_CPP, TESTS],
            'ball_scrolls': [HEADER, OWN_CPP, TESTS],
            'throw_buttons_hold': [HEADER, OWN_CPP],
            'throw_ball': [HEADER, OWN_CPP, CALLER],
            'add_tests_BallThrow': [HEADER, OWN_CPP, DIR + '../PokemonLZA_Tests.cpp'],
        })
        found = check(HEADER, BALL_THROW, None, users)
        self.assertEqual(['BallScrolls', 'ball_scrolls', 'throw_buttons_hold'], names(found))
        self.assertIn('public only for PokemonLZA_BallThrow_Tests.cpp', found[1].message)
        self.assertIn('used only in its own .cpp', found[2].message)
        self.assertEqual({'test_only_api'}, {f.check for f in found})

    def test_names_without_users_or_used_by_other_public_declarations_pass(self):
        # A type in a kept function's signature is part of its API; a name nothing uses yet
        # is new API for a later caller.
        users = fake_users({
            'BallThrowOptions': [HEADER, OWN_CPP, TESTS],
            'BallScrolls': [HEADER, OWN_CPP, TESTS],
            'ball_scrolls': [HEADER, OWN_CPP, CALLER],
            'throw_ball': [HEADER, OWN_CPP, CALLER],
        })
        self.assertEqual([], check(HEADER, BALL_THROW, None, users))

    def test_only_names_new_since_the_base_count(self):
        users = fake_users({'ball_scrolls': [HEADER, OWN_CPP, TESTS], 'BallScrolls': [HEADER, OWN_CPP]})
        self.assertEqual([], check(HEADER, BALL_THROW, BALL_THROW, users))

    def test_sources_and_test_headers_are_not_checked(self):
        users = fake_users({'ball_scrolls': [OWN_CPP, TESTS]})
        self.assertEqual([], check(OWN_CPP, BALL_THROW, None, users))
        self.assertEqual([], check(DIR + 'PokemonLZA_BallThrow_Tests.h', BALL_THROW, None, users))

    def test_test_files(self):
        for path, expected in ((TESTS, True), (DIR + 'Foo_Test.cpp', True),
                               ('SerialPrograms/Source/Tests/CommandLineTests.cpp', True),
                               (OWN_CPP, False), (DIR + 'SpeedTest.cpp', False)):
            with self.subTest(path=path):
                self.assertEqual(expected, test_file(path))


class GitUsersTests(unittest.TestCase):
    def test_one_grep_maps_each_name_to_the_files_that_mention_it_as_a_word(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for path, text in (('Common/a.h', 'int helper();\nint helper_two();\n'),
                               ('Common/a_Tests.cpp', 'int x = helper();\n'),
                               ('Common/b.cpp', 'int y = helper_two();\n'),
                               ('Common/notes.txt', 'helper\n')):
                (root / path).parent.mkdir(parents=True, exist_ok=True)
                (root / path).write_text(text)
            subprocess.check_call(['git', '-C', temp, 'init', '-q'])
            subprocess.check_call(['git', '-C', temp, 'add', '.'])
            found = git_users(root)(['helper', 'helper_two', 'absent'])
        self.assertEqual({'helper': {'Common/a.h', 'Common/a_Tests.cpp'},
                          'helper_two': {'Common/a.h', 'Common/b.cpp'},
                          'absent': set()}, found)


if __name__ == '__main__':
    unittest.main()
