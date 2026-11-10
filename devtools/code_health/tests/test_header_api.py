import unittest

from devtools.code_health import allow
from devtools.code_health.header_api import measure_header, prefix_overloads, public_names
from devtools.code_health.policy import advisories, evaluate
from devtools.code_health.style import check


CPP = 'SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/Example.cpp'
HEADER = 'SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/Example.h'
CHAR_MESSAGE = 'no char* / char[] in new code'


def wrap(body):
    return 'namespace PokemonAutomation{\n' + body + '\n} // namespace PokemonAutomation\n'


def char_errors(source, path=CPP, previous=None):
    return [f for f in check(path, source, previous)[0] if CHAR_MESSAGE in f.message]


class CharPointerTests(unittest.TestCase):
    def test_flags_char_pointer_and_array_declarations(self):
        for line in ('const char* name();',
                     'void f(const char* text);',
                     'void f(char const* text);',
                     'void f(char** list);',
                     'char* buffer = nullptr;',
                     'inline constexpr const char* SLUG = "x";',
                     'struct A{ const char* m_name; };',
                     'char name[16];',
                     'const char label[] = "x";',
                     'std::function<void(const char*)> on_text;'):
            with self.subTest(line=line):
                found = char_errors(wrap(line))
                self.assertEqual(1, len(found), line)
                self.assertEqual('char_pointer', found[0].check)

    def test_ignores_other_char_types_literals_comments_and_main(self):
        for line in ('unsigned char* bytes;', 'signed char* bytes;', 'const unsigned char table[4] = {};',
                     'char8_t* a; char16_t* b; char32_t* c; wchar_t* d;', 'uint8_t* bytes;',
                     'char c = \'x\';', 'void f(char c);', 'std::string s = "const char* x";',
                     '// returns const char* today', 'int main(int argc, char** argv);',
                     'auto p = reinterpret_cast<const char*>(data);', 'std::vector<char> bytes;'):
            with self.subTest(line=line):
                self.assertEqual([], char_errors(wrap(line)))

    def test_only_added_lines(self):
        old = wrap('const char* legacy();')
        self.assertEqual([], char_errors(wrap('const char* legacy();\nint x();'), previous=old))
        self.assertEqual(1, len(char_errors(wrap('const char* legacy();\nconst char* fresh();'),
                                            previous=old)))


class PrefixOverloadTests(unittest.TestCase):
    def messages(self, source, previous=None, path=HEADER):
        return [f.message for f in prefix_overloads(path, source, previous)]

    def test_flags_strict_prefix_overloads(self):
        source = wrap('std::string checkpoint_start_line(std::string_view checkpoint);\n'
                      'std::string checkpoint_start_line(std::string_view checkpoint, size_t events);')
        found = prefix_overloads(HEADER, source)
        self.assertEqual(1, len(found))
        self.assertEqual(3, found[0].line)
        self.assertEqual('overload_prefix', found[0].check)
        self.assertIn('checkpoint_start_line: overload (std::string_view) is a prefix of '
                      '(std::string_view, size_t)', found[0].message)
        for body in ('void f();\nvoid f(int x);',
                     'struct A{\n    void run(int a) const;\n    void run(int a, bool b) const;\n};',
                     'inline void f(const Foo& a){\n}\nvoid f(const Foo& b, int c = 0);'):
            with self.subTest(body=body):
                self.assertEqual(1, len(self.messages(wrap(body))), body)

    def test_ignores_different_types_ctors_operators_and_other_scopes(self):
        for body in ('std::string to_string(A a);\nstd::string to_string(B b);',
                     'void f(int a);\nvoid f(std::string a, int b);',
                     'struct A{\n    A();\n    A(int x);\n};',
                     'struct A{\n    A& operator=(const A&);\n    A& operator=(const A&, int);\n};',
                     'void f(int a);\nstruct A{\n    void f(int a, int b);\n};',
                     'namespace x{\nvoid f(int a);\n} // namespace x\nvoid f(int a, int b);',
                     'void f(int a);\nvoid f(int a);',
                     'void f(int a);\nvoid f(int a, int b) = delete;',
                     'template <typename T> void log(T v);\ntemplate <typename T, typename... R> '
                     'void log(T v, R... rest);'):
            with self.subTest(body=body):
                self.assertEqual([], self.messages(wrap(body)), body)
        self.assertEqual([], self.messages(wrap('void f();\nvoid f(int x);'), path=CPP))

    def test_needs_one_of_the_pair_on_an_added_line(self):
        old = wrap('void f();\nvoid f(int x);')
        self.assertEqual([], self.messages(wrap('void f();\nvoid f(int x);\nvoid g();'), old))
        self.assertEqual(1, len(self.messages(wrap('void f();\nvoid f(int x, int y);'), old)))


class HeaderSymbolTests(unittest.TestCase):
    STORY = wrap('''class StoryCheckpoint{
public:
    void run();
    void stop();
};
struct UnsupportedStep{ std::string reason; };
std::string unsupported_step(std::string_view reason);
std::string story_event(std::string_view name);
StoryCheckpoint build_story_checkpoint(int index);
StoryCheckpoint build_story_checkpoint(int index, int events);
using PreparedSteps = std::vector<std::string>;
void lint_story_checkpoint(const StoryCheckpoint& checkpoint);
void record_story_stats(const StoryCheckpoint& checkpoint);
enum class Facing{ UP, DOWN };
Facing facing_of(int direction);''')

    def test_counts_distinct_public_names(self):
        self.assertEqual(['StoryCheckpoint', 'UnsupportedStep', 'unsupported_step', 'story_event',
                          'build_story_checkpoint', 'PreparedSteps', 'lint_story_checkpoint',
                          'record_story_stats', 'Facing', 'facing_of'],
                         list(public_names(self.STORY)))

    def test_counts_constants_and_typedefs_not_internals(self):
        source = wrap('''inline constexpr std::string_view SLUG = "x";
extern const int LIMIT;
const Point ORIGIN{0, 0};
typedef int Handle;
template <typename T> inline constexpr bool is_step = false;
template <> struct std::hash<Point>{ size_t operator()(const Point&) const; };
class Forward;
#define MACRO_NAME 1
std::ostream& operator<<(std::ostream& out, const Point& p);
void Point::method(){
}
namespace detail{
int helper();
} // namespace detail
namespace{
int file_helper();
} // namespace
namespace StoryInternal{
int other_helper();
} // namespace StoryInternal
struct Holder{
    int member;
    static int count();
};''')
        self.assertEqual(['SLUG', 'LIMIT', 'ORIGIN', 'Handle', 'is_step', 'Holder'],
                         list(public_names(source)))

    def test_nested_public_namespace_counts(self):
        self.assertEqual(['f', 'g'], list(public_names(wrap('void f();\nnamespace Story{\nvoid g();\n'
                                                            '} // namespace Story'))))

    def test_limit_and_base_comparison(self):
        nine = measure_header(HEADER, self.STORY)
        self.assertEqual(10, nine[0].value)
        self.assertEqual(1, len(evaluate(nine, [])))
        self.assertIn('header_symbols=10, limit=8', evaluate(nine, [])[0].message)
        self.assertEqual([], evaluate(nine, nine))  # an unchanged legacy count may stay
        smaller = measure_header(HEADER, wrap('void a();\nvoid b();\nvoid c();\nvoid d();\nvoid e();'
                                              '\nvoid f();\nvoid g();'))
        self.assertEqual(7, smaller[0].value)
        self.assertEqual([], evaluate(smaller, []))
        self.assertEqual(1, len(advisories(smaller, [])))
        self.assertEqual([], measure_header(CPP, self.STORY))

    def test_file_level_tag_attaches(self):
        source = ('// code-health: allow header_symbols -- one table of story names that callers '
                  'always use together\n' + self.STORY)
        metrics = measure_header(HEADER, source)
        allowances, errors = allow.parse(HEADER, source)
        self.assertEqual([], errors)
        allowed, misplaced = allow.attach(HEADER, source, allowances, metrics)
        self.assertEqual([], misplaced)
        self.assertEqual([], evaluate(metrics, [], allowed))


class StyleWaiverTests(unittest.TestCase):
    def test_tag_waives_one_style_finding(self):
        source = wrap('// code-health: allow char_pointer -- the C library callback hands us this '
                      'pointer and owns it\nvoid on_text(const char* text);\nconst char* other();')
        allowances, errors = allow.parse(CPP, source)
        self.assertEqual([], errors)
        kept, notices, unattached = allow.waive(CPP, source, allowances, char_errors(source))
        self.assertEqual([4], [f.line for f in kept])
        self.assertEqual(1, len(notices))
        self.assertEqual([], unattached)

    def test_unattached_style_tag_fails(self):
        source = wrap('// code-health: allow overload_prefix -- the overload set is fixed by Qt '
                      'signal signatures here\nvoid f();')
        allowances, _ = allow.parse(HEADER, source)
        kept, notices, unattached = allow.waive(HEADER, source, allowances, [])
        self.assertEqual(([], []), (kept, notices))
        self.assertEqual(1, len(unattached))
        self.assertEqual([], allow.attach(HEADER, source, allowances, [])[1])


if __name__ == '__main__':
    unittest.main()
