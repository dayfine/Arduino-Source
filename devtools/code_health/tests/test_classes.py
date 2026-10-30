import unittest

from devtools.code_health.classes import find_classes, measure_classes
from devtools.code_health.metrics import Metric
from devtools.code_health.policy import advisories, evaluate


def values(source, symbol=None):
    result = {}
    for metric in measure_classes('Common/x.h', source):
        if symbol is None or metric.symbol == symbol:
            result[metric.check] = metric.value
    return result


def methods(count, extra=''):
    return 'class A{\npublic:\n' + extra + ''.join(f'    void m{i}();\n' for i in range(count)) + '};\n'


class ClassShapeTests(unittest.TestCase):
    def test_single_method_interface(self):
        for source in ('struct L{ virtual void on(int x) = 0; };',
                       'class L{ public: virtual ~L() = default; virtual void on() const = 0; };',
                       'class L{\npublic:\n    virtual ~L(){}\n    virtual void on() = 0;\n};'):
            with self.subTest(source=source):
                self.assertEqual(1, values(source)['single_method_interface'])
        for source in ('struct L{ virtual void on() = 0; virtual void off() = 0; };',
                       'struct L : Base{ virtual void on() = 0; };',
                       'struct L{ virtual void on(); };',
                       'struct L{ virtual void on() = 0; int state; };'):
            with self.subTest(source=source):
                self.assertEqual(0, values(source)['single_method_interface'])

    def test_public_method_count_excludes_special_members(self):
        extra = ('    A();\n    A(const A&) = delete;\n    ~A();\n    A& operator=(A&&);\n'
                 '    operator bool() const;\n    void gone() = delete;\nprivate:\n    void hidden();\npublic:\n')
        self.assertEqual(7, values(methods(7, extra))['public_methods'])
        qt = 'class W : public QWidget{\n    Q_OBJECT\npublic:\n    void a();\nsignals:\n    void b();\npublic slots:\n    void c();\n};'
        self.assertEqual(2, values(qt)['public_methods'])

    def test_public_method_limit_and_advisory(self):
        for count, failing, advisory in ((5, False, False), (6, False, True), (7, False, True), (8, True, False)):
            with self.subTest(count=count):
                metrics = measure_classes('Common/x.h', methods(count))
                self.assertEqual(failing, bool(evaluate(metrics, [])))
                self.assertEqual(advisory, bool(advisories(metrics, [])))

    def test_existing_large_class_may_change_without_growing(self):
        before = measure_classes('Common/x.h', methods(9))
        self.assertEqual([], evaluate(measure_classes('Common/x.h', methods(9, '    A();\n')), before))
        self.assertEqual([], advisories(measure_classes('Common/x.h', methods(9)), before))
        self.assertEqual([], advisories(measure_classes('Common/x.h', methods(8)), before))
        self.assertTrue(evaluate(measure_classes('Common/x.h', methods(10)), before))

    def test_public_data_only_fails_with_methods(self):
        self.assertEqual(0, values('struct P{ int a; std::string b = "{"; };')['public_data'])
        self.assertEqual(1, values('struct P{ int a; void set(int x); };')['public_data'])
        self.assertEqual(0, values('class P{ int a; public: void set(int x); };')['public_data'])
        exempt = ('struct P{ void f(); static int count; static constexpr int K = 3;\n'
                  '    BooleanCheckBoxOption ENABLE; EnumDropdownCell<Mode> mode; Q q; };')
        self.assertEqual(1, values(exempt)['public_data'])
        self.assertEqual(1, values('struct P{ void f(); std::function<void(int)> callback; };')['public_data'])
        read_only = 'class D{ public: const Type type; const char* name; virtual void run(); };'
        self.assertEqual(1, values(read_only)['public_data'])

    def test_value_types_and_test_fakes_may_expose_data(self):
        value = ('struct TilePos{ int x = 0; int y = 0; TilePos next(Direction d) const;\n'
                 '    static TilePos origin(); bool operator==(const TilePos&) const = default; };')
        self.assertEqual(0, values(value)['public_data'])
        fake = 'struct Fake{ std::vector<int> calls; void hold(int x){ calls.push_back(x); } };'
        self.assertEqual(1, values(fake)['public_data'])
        tests = measure_classes('SerialPrograms/Source/Pokemon/X_Tests.cpp', fake)
        self.assertEqual([0], [m.value for m in tests if m.check == 'public_data'])

    def test_constructor_initializers_and_inline_bodies(self):
        source = ('struct A{\n    A(int a) : m_a(a), m_b{2} { if (a){ m_a = 1; } }\n'
                  '    void f(int x = {}) { for (;;){} }\n    void (*fp)(int);\n'
                  'private:\n    int m_a; int m_b;\n};')
        self.assertEqual({'public_methods': 1, 'public_data': 1, 'single_method_interface': 0},
                         values(source))

    def test_qualified_nested_names_and_skipped_declarations(self):
        source = ('namespace PA{ namespace Game{\ntemplate <typename T, class U> void g();\n'
                  'class X; enum class E{ A, B }; struct Y{ struct Z{ int a; }; friend class X; };\n}}')
        self.assertEqual(['PA::Game::Y', 'PA::Game::Y::Z'], [c.name for c in find_classes(source)])

    def test_crlf_and_unbalanced_preprocessor_branches_do_not_raise(self):
        source = '#if X\nclass A : public B{\n#else\nclass A{\n#endif\npublic:\n    void f();\n};\n'
        measure_classes('Common/x.h', source)
        lf = methods(8)
        self.assertEqual(measure_classes('Common/x.h', lf),
                         measure_classes('Common/x.h', lf.replace('\n', '\r\n')))

    def test_failure_suggests_a_callable(self):
        failures = evaluate(measure_classes('Common/x.h', 'struct L{ virtual void on() = 0; };'), [])
        self.assertEqual(1, len(failures))
        self.assertIn('MoveOnlyFunction', str(failures[0]))

    def test_metrics_are_plain_values(self):
        self.assertIsInstance(measure_classes('Common/x.h', methods(1))[0], Metric)


if __name__ == '__main__':
    unittest.main()
