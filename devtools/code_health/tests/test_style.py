import unittest

from devtools.code_health.style import added_lines, check, strip_code


CPP = 'SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/Example.cpp'
HEADER = 'SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/Example.h'


def wrap(body):
    return 'namespace PokemonAutomation{\nnamespace NintendoSwitch{\n' + body + '\n}\n}\n'


def in_function(body):
    return wrap('void example(){\n' + body + '\n}')


class StyleCase(unittest.TestCase):
    def errors(self, source, path=CPP, previous=None):
        return [f.message for f in check(path, source, previous)[0]]

    def warnings(self, source, path=CPP, previous=None):
        return [f.message for f in check(path, source, previous)[1]]

    def assertFlags(self, fragment, source, path=CPP, count=1):
        messages = [m for m in self.errors(source, path) if fragment in m]
        self.assertEqual(count, len(messages), self.errors(source, path))

    def assertClean(self, source, path=CPP):
        self.assertEqual([], self.errors(source, path))
        self.assertEqual([], self.warnings(source, path))


class LexingTests(unittest.TestCase):
    def test_comments_strings_and_preprocessor_are_blanked(self):
        code = strip_code('int a; // new\n/* std::bind(\n */ s = "delete";\n#define X new int\n'
                          'r = R"x(std::mutex)x"; c = \'{\'; n = 1\'000;')
        self.assertEqual(['int a;', '', '    s = "      ";', '',
                          'r = "               "; c = \' \'; n = 1\'000;'],
                         [line.rstrip() for line in code])

    def test_only_inserted_and_replaced_lines_are_added(self):
        self.assertEqual({2, 4}, added_lines(['a', 'B', 'c', 'd'], ['a', 'b', 'c']))
        self.assertEqual({1, 2}, added_lines(['a', 'b'], None))


class BanTests(StyleCase):
    def test_banned_std_facilities(self):
        for line, fragment in (('std::filesystem::path p;', 'std::filesystem is banned'),
                               ('std::thread worker(run);', 'std::thread is banned'),
                               ('std::jthread worker(run);', 'std::thread is banned'),
                               ('std::lock_guard<std::mutex> lg(m_lock);', 'std::mutex'),
                               ('std::shared_mutex m;', 'std::mutex'),
                               ('std::condition_variable cv;', 'std::mutex'),
                               ('auto f = std::async(run);', 'std::async is banned')):
            with self.subTest(line=line):
                self.assertFlags(fragment, in_function(line))

    def test_allowed_neighbours_and_wrapper_directories(self):
        self.assertClean(in_function('std::this_thread::sleep_for(10ms);\n'
                                     'size_t n = std::thread::hardware_concurrency();\n'
                                     'Mutex m;\n// std::mutex in a comment\nlog("std::async");'))
        self.assertEqual([], self.errors(in_function('std::mutex m;'), 'Common/Cpp/Concurrency/Mutex.h'))
        self.assertEqual([], self.errors(in_function('std::filesystem::path p;'),
                                         'Common/Cpp/Filesystem/Path.cpp'))


class ExpressionTests(StyleCase):
    def test_std_bind(self):
        self.assertFlags('TotW #108', in_function('auto f = std::bind(&run, 1);'))
        self.assertClean(in_function('auto f = std::bind_front(&run, 1);\nauto g = [&]{ run(1); };'))

    def test_new_and_delete(self):
        self.assertFlags('TotW #126', in_function('Foo* p = new Foo();'))
        self.assertFlags('TotW #126', in_function('delete[] p;'))
        self.assertClean(wrap('struct A{\n    A(const A&) = delete;\n    void* operator new(size_t);\n'
                              '    void operator delete(void*);\n};\n'
                              'void f(){\n    auto p = std::make_unique<Foo>();\n}'))

    def test_new_allowed_in_allocators_and_qt_files(self):
        self.assertEqual([], self.errors(in_function('T* p = new T();'), 'Common/Cpp/AlignedAllocator.h'))
        self.assertEqual([], self.errors('#include <QLabel>\n' + in_function('new QLabel(this);')))

    def test_return_std_move_of_a_local(self):
        self.assertFlags('TotW #120', wrap('Foo f(){\n    Foo result;\n    return std::move(result);\n}'))
        self.assertFlags('TotW #120', wrap('Foo f(Foo&& value){\n    return std::move(value);\n}'))

    def test_return_std_move_of_members_and_lvalue_references(self):
        self.assertClean(wrap('Foo A::f(){\n    return std::move(m_value);\n}\n'
                              'Foo g(Foo& value){\n    return std::move(value);\n}\n'
                              'Foo h(Map& map){\n    return std::move(map.begin()->second);\n}'))


class FormattingTests(StyleCase):
    def test_brace_spacing(self):
        for line, fragment in (('if (x) {', 'no space between ) and {'),
                               ('for (int i = 0; i < n; i++) {', 'no space between ) and {'),
                               ('}else {', 'no spaces around else'),
                               ('} else{', 'no spaces around else')):
            with self.subTest(line=line):
                self.assertFlags(fragment, in_function(line + '\n    run();\n}'))
        self.assertFlags('namespace X{', 'namespace PokemonAutomation {\n}\n')
        self.assertFlags('namespace X{', 'namespace{\n}\nnamespace A::B {\n}\n')

    def test_single_line_bodies_and_next_line_braces(self):
        self.assertFlags('if body goes on its own lines', in_function('if (x) return;'))
        self.assertFlags('if body goes on its own lines', in_function('if (x){ return; }'))
        self.assertFlags('else body goes on its own lines', in_function('if (x){\n    a();\n}else b();'))
        self.assertFlags('put the opening brace on the while line',
                         in_function('while (x)\n{\n    run();\n}'))

    def test_repo_formatting_passes(self):
        self.assertClean(in_function(
            'if (x){\n    a();\n}else if (y){\n    b();\n}else{\n    c();\n}\n'
            'do{\n    a();\n}while (x);\n'
            'if (std::any_of(v.begin(), v.end(), [](int i){ return i > 0; })){\n    a();\n}\n'
            'if (x &&\n    y){\n    a();\n}\n'
            'switch (x){\ncase 1:\n    break;\n}'))


class DeclarationTests(StyleCase):
    def test_mutable_namespace_scope_variables(self):
        self.assertFlags("non-const namespace-scope variable 'counter'", wrap('int counter = 0;'))
        self.assertFlags("'TABLE'", wrap('static std::map<int, int> TABLE = {\n    {1, 2},\n};'))
        self.assertFlags("'BOX'", wrap('ImageFloatBox BOX{0.1, 0.2, 0.3, 0.4};'))
        self.assertFlags("non-const extern 'g_stop'", wrap('extern bool g_stop;'), HEADER)

    def test_constants_functions_and_locals_pass(self):
        self.assertClean(wrap(
            'const int LIMIT = 3;\nconstexpr double SCALE = 1.5;\nextern const ImageFloatBox AREA;\n'
            'static const std::map<int, int> TABLE = {\n    {1, 2},\n};\n'
            'bool operator==(const A& lhs, const A& rhs);\nusing Alias = int;\n'
            'struct Point{\n    int x = 0;\n};\n'
            'void f(){\n    static int calls = 0;\n    int local = 1;\n}'))
        self.assertEqual([], self.errors('typedef struct{\n    int x;\n} PABB_PACK pabb_Message;\n'))

    def test_using_directive_in_header(self):
        self.assertFlags('TotW #153', wrap('using namespace Pokemon;'), HEADER)
        self.assertEqual([], self.errors(wrap('using namespace Pokemon;')))
        self.assertEqual([], self.errors(wrap('inline void f(){\n    using namespace std::chrono_literals;\n}'),
                                         HEADER))

    def test_forward_declarations(self):
        source = wrap('class ImageViewRGB32;\nstruct Stats;\nclass Reader{\n    friend class Tester;\n'
                      '    struct Stats;\n};')
        self.assertEqual(2, sum('forward declaration' in m for m in self.errors(source, HEADER)))
        self.assertEqual([], self.errors(source))  # .cpp files may declare local helpers
        previous = wrap('')
        changed = self.warnings(wrap('class ImageViewRGB32;'), HEADER, previous)
        self.assertEqual(1, sum('forward declaration' in m for m in changed))
        self.assertEqual([], self.errors(wrap('class ImageViewRGB32;'), HEADER, previous))

    def test_primary_template_before_its_specialization_is_not_a_forward_declaration(self):
        self.assertClean(wrap('template <typename Signature>\nclass MoveOnlyFunction;\n'
                              'template <typename R, typename... Args>\nclass MoveOnlyFunction<R(Args...)>{\n};'))

    def test_top_level_const_by_value_parameters_in_header_declarations(self):
        self.assertFlags("'const int count'", wrap('void f(const int count, const std::string& name);'), HEADER)
        self.assertFlags("'const size_t index = 0'", wrap('struct A{\n    A(const size_t index = 0);\n};'), HEADER)
        self.assertClean(wrap('void f(const std::string& a, const char* b, const std::vector<int&> c_ref,\n'
                              '       int d, std::function<void(const int)>&& e);'))
        self.assertEqual([], self.errors(wrap('inline void f(const int x){\n    g(x);\n}'), HEADER))
        self.assertEqual([], self.errors(wrap('void f(const int count);')))  # .cpp declaration

    def test_bool_parameters(self):
        self.assertFlags('a defaulted bool parameter',
                         wrap('void checkpoint_reattempt_loop(Env& env, bool save = true);'), HEADER)
        self.assertFlags('2 bool parameters',
                         wrap('void exit_wild_battle(Context& c, bool stop_on_move_learn, bool prevent_evolution){\n}'))
        self.assertClean(wrap('void f(Context& c, bool enabled);\nvoid g(Mode mode);\n'
                              'struct A : B{\n    virtual void h(bool a, bool b) override;\n};\n'
                              'void call(){\n    run(true, false);\n    auto l = [](bool a, bool b){ return a; };\n}'))

    def test_existing_bool_signature_is_not_reported_when_its_line_changes(self):
        previous = wrap('void f(bool a, bool b);')
        self.assertEqual([], self.errors(wrap('void f(bool a, bool b);  '), HEADER, previous))
        self.assertEqual(1, len(self.errors(wrap('void f(bool a, bool b, bool c);'), HEADER, previous)))


class CommentTests(StyleCase):
    def test_comment_above_the_wrong_function_is_advisory(self):
        source = wrap('//  Save the game from the overworld with retries.\n'
                      '// Adds `text` to the overlay log, split at spaces.\n'
                      'void add_wrapped_log(VideoOverlay& overlay, const std::string& text);\n'
                      'bool save_game(Env& env);')
        self.assertEqual([], self.errors(source, HEADER))
        warnings = self.warnings(source, HEADER)
        self.assertEqual(1, len(warnings))
        self.assertIn('sits above add_wrapped_log but its first sentence describes save_game', warnings[0])

    def test_comment_that_slid_onto_an_uncommented_neighbour_is_advisory(self):
        source = wrap('uint16_t calorie_number() const;\n'
                      '// How many calorie number filters this detector uses.\n'
                      'static void warm_ocr();')
        self.assertEqual(1, len(self.warnings(source, HEADER)))

    def test_comment_on_a_commented_neighbour_mentioning_its_words_passes(self):
        self.assertClean(wrap('//  The last few screens, oldest first.\n'
                              'std::string recent_screens() const;\n'
                              '//  Throws with the recent screens appended.\n'
                              'void fail(const std::string& message) const;'), HEADER)

    def test_comments_that_name_their_function_or_refer_to_others_pass(self):
        self.assertClean(wrap(
            '//  Opens the save screen and reads the location (see `save_game()`).\n'
            'std::string read_location_from_save_screen(Context& c);\n'
            'bool save_game(Env& env);\n'
            '// Remove all messages added by `add_log()`.\n'
            'void clear_log();\n'
            'void add_log(std::string message);'))


class AddedLinesTests(StyleCase):
    def test_unchanged_legacy_lines_pass(self):
        previous = in_function('if (x) return;\nstd::mutex m;')
        current = in_function('if (x) return;\nstd::mutex m;\nrun();')
        self.assertEqual([], self.errors(current, CPP, previous))
        self.assertEqual(1, len(self.errors(in_function('if (y) return;\nstd::mutex m;'), CPP, previous)))

    def test_crlf_and_findings_carry_lines(self):
        findings = check(CPP, in_function('std::thread t(run);').replace('\n', '\r\n'), None)[0]
        self.assertEqual([4], [f.line for f in findings])


if __name__ == '__main__':
    unittest.main()
