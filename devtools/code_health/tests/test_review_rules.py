import unittest

from devtools.code_health.review_rules import check


DIR = 'SerialPrograms/Source/Pokemon/AutoStory/'
HEADER = DIR + 'Pokemon_StoryEngine.h'
CALLER = DIR + 'Pokemon_StoryRun.cpp'
TESTS = DIR + 'Pokemon_StoryEngine_Tests.cpp'


def wrap(body):
    return 'namespace PokemonAutomation{\nnamespace Pokemon{\n' + body + '\n}\n}\n'


def findings(source, files=None, path=HEADER):
    """Checks of `source` as a new file; `files` maps other paths to their text."""
    files = files or {}
    users = lambda names: {n: {f for f, text in files.items() if n in text} for n in names}
    return [(f.check, f.message.split("'")[1]) for f in
            check(path, wrap(source), None, users, lambda f: files.get(f, ''))]


# Fork #648 before its review: the engine holds what it was given and reports a bool next to a run.
ENGINE = '''struct EngineReport{
    CheckpointReport run = {};
    bool passed = false;
};
class Engine{
public:
    Engine(const Game& game, Backend& backend);
    const Registry& registry() const{
        return m_registry;
    }
    EngineReport run_segment(RunState& state);
private:
    const Game& m_game;
    Backend& m_backend;
    Registry m_registry;
};'''


class ReviewRulesTests(unittest.TestCase):
    def test_engine_before_review(self):
        caller = '#include "Pokemon_StoryEngine.h"\nengine.run_segment(state);'
        self.assertEqual([('bool_with_failure', 'EngineReport'), ('free_function', 'Engine'),
                          ('uncalled_method', 'Engine::registry')],
                         findings(ENGINE, {CALLER: caller, TESTS: 'engine.registry();'}))

    def test_state_owning_class_and_called_methods_pass(self):
        source = ENGINE.replace('bool passed', 'size_t passes').replace(
            'Registry m_registry;', 'Registry m_registry;\n    std::vector<int> m_seen;\n    int m_count;')
        caller = '#include "Pokemon_StoryEngine.h"\nengine.run_segment(state);\nengine.registry();'
        self.assertEqual([], findings(source, {CALLER: caller}))

    def test_unrelated_same_named_call_does_not_count(self):
        other = 'checkpoint->run_segment(env);'  # another class's method; never names Engine
        self.assertIn(('uncalled_method', 'Engine::run_segment'), findings(ENGINE, {CALLER: other}))

    def test_alias_with_one_user(self):
        user = DIR + 'Pokemon_StoryEvents.h'
        files = {user: '#include "Pokemon_StoryEngine.h"\nvoid run(EventLogger& log);',
                 DIR + 'Pokemon_StoryEvents.cpp': '#include "Pokemon_StoryEvents.h"\nEventLogger x;'}
        self.assertEqual([('one_user_alias', 'EventLogger')],
                         findings('using EventLogger = std::function<void()>;', files))
        files[DIR + 'Pokemon_Other.h'] = '#include "Pokemon_StoryEngine.h"\nEventLogger y;'
        self.assertEqual([], findings('using EventLogger = std::function<void()>;', files))

    def test_bool_with_outcome(self):
        self.assertEqual([('bool_with_failure', 'Report')],
                         findings('struct Report{\n    bool completed = false;\n    Outcome result;\n};'))
        self.assertEqual([], findings('struct Report{\n    bool saved = false;\n    Outcome result;\n};'))

    def test_enum_to_string_switch(self):
        # Fork #667 before its review: screen_event_name in CommonTools/ScreenEvents.
        names = """std::string screen_event_name(ScreenEvent event){
    switch (event){
    case ScreenEvent::FADE_BLACK:       return "black fade";
    case ScreenEvent::FADE_WHITE:
    case ScreenEvent::DIALOG_BOX:       return "other";
    }
    return "?";
}"""
        self.assertEqual([('enum_string_switch', 'screen_event_name')], findings(names, path=CALLER))
        computed = names.replace('return "other";', 'return describe(event);')
        self.assertEqual([], findings(computed, path=CALLER))


if __name__ == '__main__':
    unittest.main()
