import unittest

from devtools.code_health import allow
from devtools.code_health.param_comments import check


CPP = 'SerialPrograms/Source/VideoGames/PokemonLZA/Programs/Example.cpp'


def in_function(body):
    return ('namespace PokemonAutomation{\n'
            'void example(ProControllerContext& context){\n' + body + '\n}\n'
            '} // namespace PokemonAutomation\n')


def messages(source, previous=None):
    return [f.message for f in check(CPP, source, previous)]


class LiteralArgumentTests(unittest.TestCase):
    def test_flags_uncommented_literals_in_long_calls(self):
        # The two calls fork #590's review asked to comment, and #572's middle literal.
        for line, literal in (
                ('pbf_controller_state(context, BUTTON_ZL, DPAD_NONE, {0, 0}, {0, 0}, lock_on);', '{0, 0}, {0, 0}'),
                ('ssf_press_button(context, BUTTON_ZL | BUTTON_ZR, aim, hold, 0ms);', '0ms'),
                ('auto db = make_story_select_database(story_list(), 2, story_list().size());', '2'),
                ('pbf_press_button(context, BUTTON_A, 160ms, 400ms);', '160ms, 400ms'),
                ('sign_room_walk(Direction::EAST, 2, {3, 3}, true);', '2, {3, 3}, true'),
                ('reader.read(image, nullptr, -1.5f);', 'nullptr, -1.5f'),
                ('run(context, {}, options);', '{}')):
            with self.subTest(line=line):
                found = messages(in_function(line))
                self.assertEqual(1, len(found), found)
                self.assertIn(f' {literal} need', found[0])

    def test_commented_literals_pass(self):
        source = in_function(
            'pbf_controller_state(\n'
            '    context, BUTTON_ZL, DPAD_NONE,\n'
            '    /*left_joystick=*/{0, 0}, /*right_joystick=*/{0, 0},\n'
            '    /*duration=*/lock_on\n'
            ');\n'
            'ssf_press_button(context, BUTTONS, /*delay=*/aim, /*hold=*/hold, /*cool=*/0ms);')
        self.assertEqual([], messages(source))

    def test_exemptions(self):
        for line in (
                'f(a, b);',                                              # fewer than three arguments
                'f(a, b, c);',                                           # no literal
                'save_button_shows(console, context, 5s);',              # lone trailing duration
                'env.log(std::format("{} {}", 1, 2), COLOR_RED, true);', # log call
                'std::string s = std::format("{} of {}", index, 3);',    # format call
                'report(  "story {} failed", 3, true);',                 # format string first
                'auto x = std::clamp(value, 0, 255);',                   # std:: signature
                'PA_CHECK(value, 3, true);',                             # macro
                'throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "bad row");',
                'ImageFloatBox box(0.1, 0.2, 0.3, 0.4);',                # value tuple
                'return ImageFloatBox(left + 0.1 * i, top, 0.015, 0.2);', # value type
                'box.critical(nullptr, "Error", text);',                 # null Qt parent
                'select(image, "label", "other", name);',                # strings name themselves
                'if (a(1) && b(2) && c(3)){\n    go();\n}'):              # three one-argument calls
            with self.subTest(line=line):
                self.assertEqual([], messages(in_function(line)))

    def test_long_call_lone_duration_is_reported(self):
        self.assertEqual(1, len(messages(in_function('wait(console, context, BUTTON_A, timeout, 0ms);'))))

    def test_leading_nullptr_is_exempt_but_not_the_rest(self):
        found = messages(in_function('execute(nullptr, false, 2000ms);'))
        self.assertEqual(1, len(found), found)
        self.assertIn(' false, 2000ms need', found[0])

    def test_declarations_and_namespace_scope_are_not_calls(self):
        source = ('namespace PokemonAutomation{\n'
                  'void walk(int tiles, bool run = true, Milliseconds wait = 0ms, int retries = 3);\n'
                  'const ImageFloatBox BOX(0.1, 0.2, 0.3, 0.4);\n'
                  '} // namespace PokemonAutomation\n')
        self.assertEqual([], messages(source))

    def test_only_added_lines_count(self):
        old = in_function('pbf_press_button(context, BUTTON_A, 160ms, 400ms);')
        new = in_function('pbf_press_button(context, BUTTON_A, 160ms, 400ms);\nok();')
        self.assertEqual([], messages(new, old))
        self.assertEqual(1, len(messages(new)))

    def test_comment_must_name_the_parameter_declared_in_this_file(self):
        source = ('namespace PokemonAutomation{\n'
                  'void walk(int tiles, bool run, int retries){\n}\n'
                  'void example(){\n'
                  '    walk(/*tiles=*/2, /*running=*/true, /*retries=*/3);\n'
                  '}\n'
                  '} // namespace PokemonAutomation\n')
        found = messages(source)
        self.assertEqual(1, len(found), found)
        self.assertIn("/*running=*/ names parameter 'run'", found[0])

    def test_finding_carries_its_tag_check(self):
        source = in_function('    // code-health: allow param_comment -- the controller state reads as a table of joystick axes\n'
                             '    pbf_controller_state(context, BUTTON_ZL, DPAD_NONE, {0, 0}, {0, 0}, lock_on);')
        findings = check(CPP, source, None)
        allowances, errors = allow.parse(CPP, source)
        kept, waived, unattached = allow.waive(CPP, source, allowances, findings)
        self.assertEqual(([], 1, [], []), (kept, len(waived), unattached, errors))


if __name__ == '__main__':
    unittest.main()
