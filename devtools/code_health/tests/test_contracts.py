import unittest

from devtools.code_health.style import check


HEADER = 'SerialPrograms/Source/Pokemon/Navigation/Example.h'
CPP = HEADER.replace('.h', '_Tests.cpp')


class ContractTests(unittest.TestCase):
    def messages(self, source, path=HEADER, previous=None):
        # Isolate structural rules; CLI tests exercise the complete style pipeline.
        return [finding.message for finding in check(path, source, previous)[0]
                if not finding.message.startswith('style:')]

    def test_namespace_name_after_a_metatype_macro_without_semicolon(self):
        source = ('Q_DECLARE_METATYPE(std::string)\nnamespace Project{\nnamespace{\n'
                  '} // namespace\n} // namespace Project\n')
        self.assertEqual([], self.messages(source, path=CPP))
        broken = source.replace('// namespace Project', '// namespace Wrong')
        self.assertTrue(any('namespace-close' in m for m in self.messages(broken, path=CPP)))

    def test_namespace_close_requires_matching_comment(self):
        self.assertTrue(any('namespace-close' in m for m in self.messages('namespace Pokemon{\n}\n')))
        self.assertEqual([], self.messages('namespace Pokemon{\n} // namespace Pokemon\n'))

    def test_template_header_declaration_precedes_definition(self):
        body = '// Identity.\ntemplate <typename T>\nT identity(T value){ return value; }\n'
        self.assertTrue(any('template-api' in m for m in self.messages(body)))
        declaration = '// Identity.\ntemplate <typename T>\nT identity(T value);\n'
        self.assertFalse(any('template-api' in m for m in self.messages(declaration + body)))

    def test_header_symbols_whose_name_says_it_all_need_no_comment(self):
        source = 'struct CheckpointRunResult{};\nusing Count = int;\nvoid run();\n'
        self.assertEqual([], self.messages(source))

    def test_native_cases_separate_input_and_expectation_in_a_table(self):
        good = '''struct Case{
    std::string name;
    struct Input{ int value; };
    Input input;
    struct Expectation{ int value; };
    Expectation expectation;
};
class Test_Value : public UnitTest{
public:
    explicit Test_Value(Case scenario) : UnitTest(scenario.name){}
};
const Case CASES[] = {{.name = "one", .input = {.value = 1}, .expectation = {.value = 2}}};
void add_tests(){
    for (const Case& scenario : CASES){
        database.add<Test_Value>(scenario);
    }
}
'''
        bad = good.replace('Input input;', 'int value;').replace('Expectation expectation;', 'int expected;')
        self.assertTrue(any('test-case' in m for m in self.messages(bad, CPP)))
        self.assertFalse(any('test-case' in m for m in self.messages(good, CPP)))

    def test_case_fields_may_be_value_initialized(self):
        # Windows clang (-Wmissing-designated-field-initializers) wants `= {}` on every field.
        source = '''struct Case{
    struct Input{ int value = {}; };
    struct Expectation{ int value{}; };
    std::string name = {};
    Input input = {};
    Expectation expectation{};
};
class Test_Value : public UnitTest{
public:
    explicit Test_Value(Case scenario) : UnitTest(scenario.name){}
};
const Case CASES[] = {{.name = "one", .input = {.value = 1}, .expectation = {.value = 2}}};
void add_tests(){
    for (const Case& scenario : CASES){ database.add<Test_Value>(scenario); }
}
'''
        self.assertFalse(any('test-case' in m for m in self.messages(source, CPP)))
        name_in_input = source.replace('struct Input{ int value = {}; };',
                                       'struct Input{ std::string name = {}; int value = {}; };')
        self.assertTrue(any('test-case' in m for m in self.messages(name_in_input, CPP)))

    def test_case_name_is_its_own_field_not_in_the_input(self):
        name_in_input = '''struct Case{
    struct Input{ std::string name; int value; };
    struct Expectation{ int value; };
    Input input;
    Expectation expectation;
};
class Test_Value : public UnitTest{
public:
    explicit Test_Value(Case scenario) : UnitTest(scenario.input.name){}
};
const Case CASES[] = {{.input = {.name = "one", .value = 1}, .expectation = {.value = 2}}};
void add_tests(){
    for (const Case& scenario : CASES){ database.add<Test_Value>(scenario); }
}
'''
        self.assertTrue(any('test-case' in m for m in self.messages(name_in_input, CPP)))
        both = name_in_input.replace('    Input input;', '    std::string name;\n    Input input;').replace(
            '{{.input = {.name = "one", ', '{{.name = "one", .input = {')
        self.assertTrue(any('test-case' in m for m in self.messages(both, CPP)))
        fixed = both.replace('struct Input{ std::string name; int value; };', 'struct Input{ int value; };')
        self.assertFalse(any('test-case' in m for m in self.messages(fixed, CPP)))

    def test_public_template_interface_precedes_internal_namespace(self):
        source = '''namespace DetailInternal{
// Identity helper.
template <typename T> T identity(T value){ return value; }
} // namespace DetailInternal
// Public operation.
template <typename T> T run(T value);
template <typename T> T run(T value){ return value; }
'''
        self.assertTrue(any('template-api-order' in m for m in self.messages(source)))

    def test_case_table_rejects_positional_rows(self):
        source = '''struct Case{
    std::string name;
    struct Input{ int value; };
    struct Expectation{ int value; };
    Input input;
    Expectation expectation;
};
class Test_Value : public UnitTest{
public:
    explicit Test_Value(Case scenario) : UnitTest(scenario.name){}
};
const Case CASES[] = {{"one", {1}, {2}}};
void add_tests(){
    for (const Case& scenario : CASES){ database.add<Test_Value>(scenario); }
}
'''
        self.assertTrue(any('test-case' in m for m in self.messages(source, CPP)))

    NESTED = '''struct ValueTestCase{
    std::string name;
    struct Input{ int value; };
    struct Expectation{ int result; };
    Input input;
    Expectation expectation;
};
class Test_Value : public UnitTest{
public:
    explicit Test_Value(ValueTestCase scenario) : UnitTest(scenario.name){}
};
const ValueTestCase CASES[] = {
    {
        .name = "increment",
        .input = {.value = 1},
        .expectation = {.result = 2},
    },
};
void add_tests(){
    for (const ValueTestCase& scenario : CASES){
        database.add<Test_Value>(scenario);
    }
}
'''

    def test_nested_case_form_passes(self):
        self.assertFalse(any('test-case' in m for m in self.messages(self.NESTED, CPP)))

    def test_top_level_input_and_expectation_structs_fail_for_a_changed_test(self):
        old = ('struct ValueInput{ int value; };\n'
               'struct ValueExpectation{ int result; };\n'
               'struct ValueTestCase{ std::string name; ValueInput input; ValueExpectation expectation; };\n'
               + self.NESTED[self.NESTED.index('class Test_Value'):])
        new = old.replace('scenario.name', 'scenario.name /* changed */')
        found = [m for m in self.messages(new, CPP, old) if 'test-case' in m]
        self.assertEqual(1, len(found))
        self.assertIn('nested', found[0])
        self.assertFalse(any('test-case' in m for m in self.messages(old, CPP, old)))

    def test_two_case_structs_with_their_own_nested_input_pass(self):
        source = '''struct AlphaCase{
    std::string name;
    struct Input{ int value; };
    struct Expectation{ int value; };
    Input input;
    Expectation expectation;
};
struct BetaCase{
    std::string name;
    struct Input{ int count; };
    struct Expectation{ int count; };
    Input input;
    Expectation expectation;
};
class Test_Alpha : public UnitTest{
public:
    explicit Test_Alpha(AlphaCase scenario) : UnitTest(scenario.name){}
};
class Test_Beta : public UnitTest{
public:
    explicit Test_Beta(BetaCase scenario) : UnitTest(scenario.name){}
};
const AlphaCase ALPHA[] = {{.name = "a", .input = {.value = 1}, .expectation = {.value = 2}}};
const BetaCase BETA[] = {{.name = "b", .input = {.count = 1}, .expectation = {.count = 2}}};
void add_tests(){
    for (const AlphaCase& scenario : ALPHA){ database.add<Test_Alpha>(scenario); }
    for (const BetaCase& scenario : BETA){ database.add<Test_Beta>(scenario); }
}
'''
        self.assertFalse(any('test-case' in m for m in self.messages(source, CPP)))
        # Qualified names prevent BetaCase::Input from masking a forbidden name field in AlphaCase::Input.
        bad_alpha = source.replace('struct Input{ int value; };', 'struct Input{ std::string name; int value; };')
        self.assertEqual(1, sum('test-case' in m for m in self.messages(bad_alpha, CPP)))

    def test_case_field_that_is_not_a_nested_struct_fails(self):
        broken = self.NESTED.replace('Input input;', 'int input;')
        self.assertTrue(any('test-case' in m for m in self.messages(broken, CPP)))

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
        new = old.replace('}\n', '} // namespace Wrong\n')
        self.assertEqual(1, sum('namespace-close' in m for m in self.messages(new, previous=old)))

    def test_all_header_symbol_kinds_and_methods(self):
        source = '''// This block documents only First.
void first();
void second();
enum class Mode{ FIRST, SECOND };
constexpr int COUNT = 3;
template <typename T> struct Box{ void method(){}; T value; };
'''
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
        source = """struct Case{
    std::string_view name;
    struct Input{ int value; };
    struct Expectation{ int value; };
    Input input;
    Expectation expectation;
};
class Test_Value : public UnitTest{
public:
    explicit Test_Value(Case scenario) : UnitTest(scenario.name){}
};
constexpr std::array<Case, 1> CASES = {
    {.name = "one", .input = {.value = 1}, .expectation = {.value = 2},},
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
            source.replace('.name = "one", ', ''),
        ):
            with self.subTest(source=broken):
                self.assertTrue(any('test-case' in m for m in self.messages(broken, CPP)))


    def test_header_namespace_comments_are_not_required_for_class_closes(self):
        source = "// Configuration.\nstruct Limits{ int budget; };\n"
        self.assertEqual([], self.messages(source))
        self.assertEqual([], self.messages('namespace A::B{\n} // namespace A::B\n'))

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

    def test_internal_templates_are_defined_in_place(self):
        source = '''// Public operation.
template <typename T> T run(T value);
namespace DetailInternal{
// Identity helper.
template <typename T> T identity(T value){ return value; }
} // namespace DetailInternal
template <typename T> T run(T value){ return value; }
'''
        self.assertEqual([], self.messages(source))
        separate = source.replace('// Identity helper.', '// Identity helper.\ntemplate <typename T> T identity(T value);\n// Definition.')
        self.assertTrue(any('template-api-internal' in m for m in self.messages(separate)))
