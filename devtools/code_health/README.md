# Incremental C++ code-health and agent verification

From the checkout root, create a dedicated environment (Python 3.8+):

```sh
python3 -m venv .venv-code-health
.venv-code-health/bin/python -m pip install -r devtools/code_health/requirements.txt
.venv-code-health/bin/python -m unittest discover -s devtools/code_health/tests -v
.venv-code-health/bin/python -m devtools.code_health
```

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
Fix/extract code instead of raising limits.

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

## Layering

Files under `Common/`, `SerialPrograms/Source/CommonFramework/` and
`SerialPrograms/Source/CommonTools/` may not add an `#include` of a game
module: `Pokemon/`, `PokemonBDSP/` or `VideoGames/` under
`SerialPrograms/Source`. Consoles, controllers and frameworks (including
`NintendoSwitch/`) are platform modules games build on, not games; `config.py`
lists both groups and a test fails when a new top-level directory is in neither.
Includes already present at the base are not reported.

## Build and native C++ tests in one command

The existing `.github/workflows/cpp-ci-serial-programs-base.yml` already runs
native tests after building on Linux, macOS and Windows when `run-tests` is
set. `Tests/CommandLineTests.cpp` runs the registered `UnitTestDatabase`,
returns 1 on failure, and needs no Switch. Linux CI uses `QT_QPA_PLATFORM=offscreen`.
It still initializes QApplication/camera enumeration; headless is not Qt-free.

Prerequisites: install platform dependencies as in that CI workflow, configure
this checkout's CMake build (e.g. `cmake -S SerialPrograms -B SerialPrograms/bin
-DQT_MAJOR:STRING=6`), and provide Packages/resources. Put the separate
`PokemonAutomation/CommandLineTests` fixture checkout in an ancestor of the
binary, within its five-level resource search, or use `UnitTestResources`.
Unpack downloadable model ZIPs as the existing CI's “Install downloadable
resources for tests” step does. For this runner on macOS, install models under
`~/Library/Application Support/SerialPrograms/Profiles/agent-native-tests/DownloadedResources/`.
On Linux/Windows install under `<build-dir>/DownloadedResources/`. Missing
fixture/model resources are not passes.

For a configured Linux build:

```sh
.venv-code-health/bin/python devtools/verify.py --build-dir SerialPrograms/bin --binary SerialPrograms/bin/SerialPrograms --base-ref origin/main
```

On macOS use `--binary SerialPrograms/bin/SerialPrograms.app/Contents/MacOS/SerialPrograms`;
on Windows use `--binary SerialPrograms/bin/RelWithDebInfo/SerialPrograms.exe`.
This runs linter self-tests, the incremental check, CMake build, then native tests;
it stops at the first nonzero exit and propagates it. Native tests run from
the build directory. On macOS it uses a separate `agent-native-tests` profile;
Qt is offscreen on all platforms. Use this checkout's build/binary.

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
