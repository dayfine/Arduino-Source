import unittest
from types import SimpleNamespace

from devtools.verify import commands, verify


class VerifyTests(unittest.TestCase):
    def test_failure_stops_before_later_steps(self):
        called = []
        def run(command, **kwargs):
            called.append(command)
            return SimpleNamespace(returncode=7)
        self.assertEqual(7, verify([['first'], ['must-not-run']], run=run))
        self.assertEqual([['first']], called)

    def test_non_mac_does_not_claim_named_profile_support(self):
        steps = commands('/build', '/binary', 'origin/main', 4, platform='linux')
        self.assertNotIn('--profile', steps[-1])

    def test_all_steps_run_and_native_tests_use_separate_profile(self):
        steps = commands('/build', '/binary', 'origin/main', 4, platform='darwin')
        self.assertIn('--command-line-test-mode', steps[-1])
        self.assertIn('agent-native-tests', steps[-1])
        called = []
        def run(command, **kwargs):
            called.append(command)
            return SimpleNamespace(returncode=0)
        self.assertEqual(0, verify(steps, run=run))
        self.assertEqual(steps, called)
