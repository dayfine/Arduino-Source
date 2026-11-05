import unittest

from devtools.code_health.contracts import check


HEADER = 'SerialPrograms/Source/Pokemon/Navigation/Example.h'
CPP = HEADER.replace('.h', '_Tests.cpp')


class ContractTests(unittest.TestCase):
    def messages(self, source, path=HEADER, previous=None):
        return [finding.message for finding in check(path, source, previous)]

    def test_namespace_close_requires_matching_comment(self):
        self.assertTrue(any('namespace-close' in m for m in self.messages('namespace Pokemon{\n}\n')))
        self.assertEqual([], self.messages('namespace Pokemon{\n} // namespace Pokemon\n'))

    def test_template_header_declaration_precedes_definition(self):
        body = '// Identity.\ntemplate <typename T>\nT identity(T value){ return value; }\n'
        self.assertTrue(any('template-api' in m for m in self.messages(body)))
        declaration = '// Identity.\ntemplate <typename T>\nT identity(T value);\n'
        self.assertFalse(any('template-api' in m for m in self.messages(declaration + body)))

    def test_top_level_header_symbols_have_comments(self):
        source = 'struct Limits{};\nusing Count = int;\nvoid run();\n'
        self.assertEqual(3, sum('header-comment' in m for m in self.messages(source)))
        documented = '// Search budget.\nstruct Limits{};\n// Number of tiles.\nusing Count = int;\n// Execute.\nvoid run();\n'
        self.assertFalse(any('header-comment' in m for m in self.messages(documented)))

    def test_native_cases_separate_input_and_expectation_in_a_table(self):
        good = '''struct Input{ int value; };
struct Expectation{ int value; };
struct Case{ Input input; Expectation expectation; };
class Test_Value : public UnitTest{
public:
    explicit Test_Value(Case scenario) : UnitTest(scenario.input.name){}
};
const Case CASES[] = {{.input = {.value = 1}, .expectation = {.value = 2}}};
void add_tests(){
    for (const Case& scenario : CASES){
        database.add<Test_Value>(scenario);
    }
}
'''
        bad = good.replace('Input input; Expectation expectation;', 'int value; int expected;')
        self.assertTrue(any('test-case' in m for m in self.messages(bad, CPP)))
        self.assertFalse(any('test-case' in m for m in self.messages(good, CPP)))

    def test_public_template_interface_precedes_internal_namespace(self):
        source = '''namespace DetailInternal{
// Identity helper.
template <typename T> T identity(T value);
template <typename T> T identity(T value){ return value; }
} // namespace DetailInternal
// Public operation.
template <typename T> T run(T value);
template <typename T> T run(T value){ return value; }
'''
        self.assertTrue(any('template-api-order' in m for m in self.messages(source)))

    def test_case_table_rejects_positional_rows(self):
        source = '''struct Input{ int value; };
struct Expectation{ int value; };
struct Case{ Input input; Expectation expectation; };
class Test_Value : public UnitTest{
public:
    explicit Test_Value(Case scenario) : UnitTest(scenario.input.name){}
};
const Case CASES[] = {{{1}, {2}}};
void add_tests(){
    for (const Case& scenario : CASES){ database.add<Test_Value>(scenario); }
}
'''
        self.assertTrue(any('test-case' in m for m in self.messages(source, CPP)))

    def test_namespace_names_anonymous_nested_and_literal_braces(self):
        good = '''namespace Outer{
namespace Inner{
namespace{
void f(){ const char* text = "} namespace Wrong"; }
} // namespace
} // namespace Inner
} // namespace Outer
'''
        self.assertEqual([], self.messages(good, CPP))
        for replacement in ('} // namespace Other', '} // namespace', '}'):
            with self.subTest(replacement=replacement):
                bad = good.replace('} // namespace Inner', replacement)
                self.assertEqual(1, sum('namespace-close' in m for m in self.messages(bad, CPP)))

    def test_unchanged_debt_is_grandfathered(self):
        old = 'namespace Old{\nstruct Limits{};\n}\n'
        self.assertEqual([], self.messages(old + '\n// unrelated edit\n', previous=old))
        new = old.replace('struct Limits{};', 'struct NewLimits{};')
        self.assertEqual(1, sum('header-comment' in m for m in self.messages(new, previous=old)))

    def test_all_header_symbol_kinds_and_methods(self):
        source = '''// This block documents only First.
void first();
void second();
enum class Mode{ FIRST, SECOND };
constexpr int COUNT = 3;
template <typename T> struct Box{ void method(){}; T value; };
'''
        self.assertEqual(4, sum('header-comment' in m for m in self.messages(source)))
        self.assertFalse(any('template-api' in m for m in self.messages(source)))

    def test_block_comments_and_documented_redeclaration(self):
        source = '''/** Returns its input. */
template <typename T>
T identity(T value);
template <typename T>
T identity(T value){ return value; }
'''
        self.assertEqual([], self.messages(source))

    def test_raw_strings_and_preprocessor_do_not_declare_symbols(self):
        source = '''#define DECLARE struct Invisible{}
// Not code: namespace Foo{ } and template<typename T> T f(T x){}
// Diagnostic text.
const char* SAMPLE = R"x(namespace Bad{ struct Missing{}; })x";
'''
        self.assertEqual([], self.messages(source))

    def test_template_overloads_need_their_own_declaration(self):
        source = '''// Identity.
template <typename T> T f(T value);
// Another overload.
template <typename T> T f(T value, int count){ return value; }
'''
        self.assertEqual(1, sum('template-api:' in m for m in self.messages(source)))

    def test_declarations_first_allow_internal_helpers(self):
        source = '''// Public operation.
template <typename T> T run(T value);
namespace DetailInternal{
// Identity helper.
template <typename T> T identity(T value);
template <typename T> T identity(T value){ return value; }
} // namespace DetailInternal
template <typename T> T run(T value){ return value; }
'''
        self.assertEqual([], self.messages(source))

    def test_registration_without_a_case_struct_fails(self):
        source = '''class Test_Value : public UnitTest{
public:
    Test_Value(int input, int expected){}
};
'''
        self.assertTrue(any('test-case' in m for m in self.messages(source, CPP)))
        self.assertFalse(any('test-case' in m for m in self.messages(source, CPP.replace('_Tests', ''))))


    def test_constexpr_std_array_table_and_trailing_commas(self):
        source = """struct Input{ int value; };
struct Expectation{ int value; };
struct Case{ Input input; Expectation expectation; };
class Test_Value : public UnitTest{
public:
    explicit Test_Value(Case scenario) : UnitTest(scenario.input.name){}
};
constexpr std::array<Case, 1> CASES = {
    {.input = {.value = 1}, .expectation = {.value = 2},},
};
void add_tests(){ for (const auto& scenario : CASES){ database.add<Test_Value>(scenario); } }
"""
        self.assertFalse(any('test-case' in m for m in self.messages(source, CPP)))
        for broken in (
            source.replace('Input input;', 'int input;'),
            source.replace('Expectation expectation;', 'int expectation;'),
            source.replace('const auto& scenario : CASES', 'int i = 0; i < 1; i++'),
            source.replace('constexpr std::array<Case, 1> CASES', 'Case one_case'),
            source.replace('.expectation =', '.expected ='),
        ):
            with self.subTest(source=broken):
                self.assertTrue(any('test-case' in m for m in self.messages(broken, CPP)))


    def test_header_namespace_comments_are_not_required_for_class_closes(self):
        source = "// Configuration.\nstruct Limits{ int budget; };\n"
        self.assertEqual([], self.messages(source))
        self.assertEqual([], self.messages('namespace A::B{\n} // namespace A::B\n'))

    def test_empty_comments_do_not_document_a_header_symbol(self):
        self.assertTrue(any('header-comment' in m for m in self.messages('//\nvoid run();\n')))

    def test_changed_native_test_class_is_checked(self):
        old = 'class Test_Value : public UnitTest{\npublic:\n    Test_Value(int input, int expected){}\n};\n'
        new = old.replace('int expected', 'long expected')
        self.assertTrue(any('test-case' in m for m in self.messages(new, CPP, old)))

    def test_template_declarations_allow_unnamed_and_renamed_parameters(self):
        source = '''// Identity.
template <typename T> T f(T);
template <typename T> T f(T value){ return value; }
'''
        self.assertEqual([], self.messages(source))

    def test_extern_c_linkage_block_is_not_a_namespace(self):
        source = 'extern "C"{\n// Exported entry point.\nvoid run();\n}\n'
        self.assertEqual([], self.messages(source))
