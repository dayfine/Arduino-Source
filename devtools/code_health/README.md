# Incremental C++ code-health and agent verification

This linter lives only on the fork's `agents` branch (dayfine/Arduino-Source). Code branches keep a
thin `.github/workflows/code-health.yml` that checks out `agents` next to the code and runs it from
there; the pre-push hook (`tools/hooks/pre-push`) does the same locally.

Create the environment once (Python 3.8+; `tools/install_hooks.sh` does this):

```sh
python3 -m venv ~/.venvs/code-health
~/.venvs/code-health/bin/pip install -r devtools/code_health/requirements.txt
```

Self-tests, from a checkout of `agents` (e.g. `~/Projects/Arduino-Source-agents`):

```sh
~/.venvs/code-health/bin/python -m unittest discover -s devtools/code_health/tests -v
```

Check a code checkout: run from the `agents` checkout and point `--root` at the code:

```sh
cd ~/Projects/Arduino-Source-agents
~/.venvs/code-health/bin/python -m devtools.code_health --root <code checkout> --base-ref origin/<PR base>
```

`--root` defaults to the current directory, so `PYTHONPATH=<agents checkout> python -m
devtools.code_health` from the code checkout also works once that branch no longer carries its own
`devtools/code_health` (Python would import the code branch's copy first). Set `CODE_HEALTH_REPO` to
a code checkout when running the self-tests, so the layering test can classify its source tree.

Without `--base-ref`, the base is `git merge-base HEAD upstream/main`, or
`origin/main` when there is no `upstream` remote, and the command prints the
base it used. A stale fork `main` therefore cannot report upstream changes
as this branch's. CI passes the PR base explicitly.

Only added/modified/renamed Git-tracked `.h .hpp .c .cpp .tpp` files under
`Common/` and `SerialPrograms/` are analyzed. Directory components starting
with `3rdParty` are excluded. Unchanged and deleted files are not read.
Stage new source files with `git add` before checking; tracked unstaged edits
are included. Missing base revisions and analysis errors fail the command.

| Check | Limit |
| --- | --- |
| File length | 500 physical lines advisory; 1000 hard |
| Function length | 50 physical lines, signature through closing brace; 25 recommended |
| Nesting | 4 lexical brace levels inside the function body |
| Parameters | 7 |
| Public methods per class/struct | 7 (excluding ctors, dtors, operators, deleted and Qt signals); 6-7 advisory |
| Public non-static data in a class with mutating methods | 0 (exempt: plain structs, value types with only const/static methods, const fields, `*Option`/`*Cell` fields, test files) |
| Single-method interface (one pure virtual + optional virtual dtor, no bases) | not allowed: prefer a callable (`MoveOnlyFunction` / `std::function`) |

New files/functions must meet these limits. In modified files, compare each
metric with its value at the base revision: existing violations may remain or
improve but may not grow. Unchanged long functions in a touched file do not
block unrelated improvements. Renamed files compare with their original path.
Renamed function signatures have a new identity and must meet the limits.
There is no repository-wide scan, generated baseline or exceptions registry.
Fix/extract code instead of raising limits; the one way past a limit is a justified tag at the site.

With `--github` (CI passes it; also the default when `GITHUB_ACTIONS=true`) each
failure is also printed as an `::error` annotation and each advisory as a
`::warning` at its file and line, so they show inline in the PR diff: changed files
over 500 lines at line 1, functions over 25 lines at their start, classes with 6-7
public methods at the class. At most 50 of each level are annotated per run;
advisories never fail the job.

The CI job runs on PRs (including drafts) and pushes to main, comparing the
current tree with the PR base / previous main revision. An initial push uses
Git's empty tree as its base. The tests generate X-line sources in memory or
temporary Git repos; no large fixture or snapshot files are checked in. These
Python tests exercise the linter, not SerialPrograms application code.

Lizard 1.17.31 recognizes C++ functions without a compilation database. The
brace extension counts nested blocks, lambdas and initializer braces; it
ignores enclosing namespace/class braces, comments and string literals. This
is lexical analysis, not a C++ compiler or clang-tidy replacement. Macro-generated
functions and advanced syntax may require parser follow-up. CCN is not enforced.

Class shapes come from a lexical member scan over lizard's tokens
(`classes.py`), keyed by qualified class name and compared with the base like
functions. libclang would need a compilation database with Qt include paths
to parse these headers; the scan only has to tell access sections, member
functions and data apart. Classes split across `#if` branches are skipped.

## Bypass tags

```cpp
// Moves by generation, in the game's own order.
// code-health: allow function_lines -- one table of move data; splitting it hides which rows belong together
void load_move_table(){
```

`// code-health: allow <check>[,<check>] -- <justification>` lets the listed metrics of one
function or class exceed their limit. Checks are the metric names in the table above:
`file_lines`, `function_lines`, `nesting`, `parameters`, `public_methods`, `public_data`,
`single_method_interface`. Style and layering findings have no bypass.

- **Place it** on the function's or class's first line, or in the comment block directly above
  it (only `//` comment and `template` lines between). A `file_lines` tag goes in the first 20
  lines of the file. A tag that attaches to nothing fails.
- **Justify it:** the text after `--`, on the tag line itself, must be at least 40 characters and 6 words,
  and say why splitting would make the code worse. Unknown checks, a missing or short
  justification, or a malformed tag fail.
- **Ceiling:** a tag allows at most twice the limit (100-line functions, 8 nesting levels,
  14 parameters, 14 public methods, 2000-line files). Yes/no checks with limit 0 (public data,
  single-method interface) have no ceiling. Legacy values above the ceiling may still stay or shrink.
- **Visibility:** every value over its limit under a tag prints as a `Bypass:` line quoting the
  justification, and under `--github` as a `::warning` titled "Code health (bypass)" at the
  function or class, so each bypass shows in the PR diff. A tag on a value within its limit is reported as
  an advisory to remove it.

A bypass is acceptable when splitting would hurt readability: a data table or data-driven file,
a flat switch over every enum value or game state, a generated-like list of registrations, or an
interface whose shape is fixed by Qt or upstream. "Legacy", "TODO" or "will split later" are not
justifications. Squashing statements onto fewer lines, or packing parameters into an ad-hoc
struct, to get under a limit is not acceptable: it hides the size from the linter and reviewers.
Use a tag and argue for it, or split the code.

## Layering

Files under `Common/`, `SerialPrograms/Source/CommonFramework/` and
`SerialPrograms/Source/CommonTools/` may not add an `#include` of a game
module: `Pokemon/`, `PokemonBDSP/` or `VideoGames/` under
`SerialPrograms/Source`. Consoles, controllers and frameworks (including
`NintendoSwitch/`) are platform modules games build on, not games; `config.py`
lists both groups and a test fails when a new top-level directory is in neither.
Includes already present at the base are not reported.

## Style (added lines only)

`style.py` checks only the lines a change adds or edits (a line diff against the
base; every line of a new file), so legacy code in a touched file is not reported.
Comments, string/character literals and preprocessor lines are blanked first, and
a brace-scope stack tells namespace, class and function scope apart. It is a
lexer, not a parser: rules that need types stay in review.

| Check | Level | Rule |
| --- | --- | --- |
| `std::filesystem` (outside `Common/Cpp/Filesystem/`) | error | upstream AGENTS.md "Filesystem" |
| `std::thread`/`jthread`, `std::*mutex`, `std::condition_variable`, `std::async` (outside `Common/Cpp/Concurrency/`) | error | upstream AGENTS.md "Concurrency"; TotW #197 |
| Control flow: `) {`, `} else`, `else {`, `namespace X {`, brace on the next line, one-line `if`/`for`/`while`/`else` bodies | error | upstream AGENTS.md "Control flow formatting" |
| Non-const namespace-scope variable or `extern` | error | TotW #103 |
| `new` / `delete` expressions (exempt: paths containing `alloc`, files including Qt headers, `operator new/delete`, `= delete`) | error | TotW #126 |
| `std::bind` | error | TotW #108 |
| `return std::move(name);` of a local or by-value/`&&` parameter (not `m_` members or `T&` names) | error | TotW #120 |
| Top-level `const` on a by-value parameter in a header declaration | error | TotW #109 |
| A defaulted `bool` parameter, or 2+ `bool` parameters (not `override`/`final`, not an unchanged signature) | error | TotW #94 |
| `using namespace` at namespace scope in a header | error | TotW #153 |
| Forward `class`/`struct` declaration in a header: new header / existing header | error / advisory | fork AGENTS.md "Forward declarations" |
| A comment whose first sentence describes the neighbouring function, none of its own | advisory | CONTRIBUTING; Google "Function Comments" |

Advisories print as `Advisory:` lines and `::warning` annotations and never fail.

## Build and native C++ tests in one command

The existing `.github/workflows/cpp-ci-serial-programs-base.yml` already runs
native tests after building on Linux, macOS and Windows when `run-tests` is
set. `Tests/CommandLineTests.cpp` runs the registered `UnitTestDatabase`,
returns 1 on failure, and needs no Switch. Linux CI uses `QT_QPA_PLATFORM=offscreen`.
It still initializes QApplication/camera enumeration; headless is not Qt-free.

Prerequisites: install platform dependencies as in that CI workflow, configure
the code checkout's CMake build (e.g. `cmake -S SerialPrograms -B SerialPrograms/bin
-DQT_MAJOR:STRING=6`), and provide Packages/resources. Put the separate
`PokemonAutomation/CommandLineTests` fixture checkout in an ancestor of the
binary, within its five-level resource search, or use `UnitTestResources`.
Unpack downloadable model ZIPs as the existing CI's “Install downloadable
resources for tests” step does. For this runner on macOS, install models under
`~/Library/Application Support/SerialPrograms/Profiles/agent-native-tests/DownloadedResources/`.
On Linux/Windows install under `<build-dir>/DownloadedResources/`. Missing
fixture/model resources are not passes.

For a configured Linux build, from the code checkout:

```sh
~/.venvs/code-health/bin/python ~/Projects/Arduino-Source-agents/devtools/verify.py --build-dir SerialPrograms/bin --binary SerialPrograms/bin/SerialPrograms --base-ref origin/main
```

On macOS use `--binary SerialPrograms/bin/SerialPrograms.app/Contents/MacOS/SerialPrograms`;
on Windows use `--binary SerialPrograms/bin/RelWithDebInfo/SerialPrograms.exe`.
This runs linter self-tests, the incremental check, CMake build, then native tests;
it stops at the first nonzero exit and propagates it. Native tests run from
the build directory. On macOS it uses a separate `agent-native-tests` profile;
Qt is offscreen on all platforms. Use the code checkout's build/binary.

## Agent work loop and reuse

Feature branch -> one concern -> test/verify -> read exit status -> review diff
and CI -> draft PR on dayfine/Arduino-Source. No upstream PR publication by
agents. Refresh canonical rules from origin/agents at task start and before
PR mutations; never merge that notes branch into code branches. Shared
infrastructure needs portable regression coverage; gameplay changes additionally
need appropriate live run evidence.

`metrics.py` and `policy.py` accept source/metrics as values and know nothing
about this repository's Git layout. `git_files.py` and `__main__.py` are the
integration layer; extract a separate package only when another consumer needs it.

## Header and native test contracts

The fork's structural checks in `style.py` run in both the normal CLI and
pre-push/CI gate. They share the existing parsing pass and comment/literal-aware
scope lexer, not raw line
matches or application source-extraction tests. Only changed namespace closes
and declarations are enforced; untouched legacy declarations are grandfathered.

- `namespace-close`: named namespaces end in `} // namespace Name`, anonymous
  ones in `} // namespace`. The name must match, including C++ nested namespace syntax.
- `template-api`: public namespace-scope function templates in headers declare their
  interface before their definition. Inline class members and class templates
  do not need redundant declarations. Keep default arguments on the declaration.
- `template-api-order`: public template declarations precede helper namespaces
  named `*Internal`, `internal`, or `detail`.
- `template-api-internal`: internal helper templates are defined in place, with
  no separate forward declaration. A header containing only internal helpers
  needs no invented public API.
- `header-comment`: document each added/changed namespace-scope header symbol,
  including helper declarations, types, aliases, and constants. An immediately
  adjacent `//` or block/Doxygen comment counts. A definition can reuse its own
  documented declaration. Class members and namespaces themselves are excluded.
- `test-case`: new/changed native `*_Tests.cpp` UnitTest classes take one aggregate case.
  That case has exactly `input` and `expectation` fields, each a separate local
  struct. A named C-array, `std::array`, or `std::vector` table contains designated
  `.input` / `.expectation` rows and is consumed by a range-for for registration.
  Include a descriptive name in the input and compare expected and observed values.

```cpp
struct ValueInput{ std::string name; int value; };
struct ValueExpectation{ int result; };
struct ValueTestCase{ ValueInput input; ValueExpectation expectation; };

const ValueTestCase CASES[] = {
    {
        .input = {.name = "increment", .value = 1},
        .expectation = {.result = 2},
    },
};
// Test_Value receives ValueTestCase and compares actual results with expectation.
// Register with: for (const auto& scenario : CASES){ database.add<Test_Value>(scenario); }
```

The native shape follows PokemonAutomation/Arduino-Source#1846. The lexical
checker validates local structure; it cannot resolve included aliases, macro
generated declarations or C++ overloads like a compiler. Shared fixture types
should remain beside the tests or need a deliberate parser extension, not an
unexplained bypass. Header parameter spans, game movement policies and comment
quality still require design review. These rules do not force a default policy.

Namespace moves retain previous metric ceilings only when the declaration text
is unchanged, stays in the same file, matches one old and one current function,
and the old qualified symbol is gone. Copies and ambiguous matches receive no
allowance, and increases beyond the previous metrics still fail. Namespace
comments are also recognized after declarations such as
`Q_DECLARE_METATYPE(...)` that do not end with a semicolon.
