# C++ code-health and agent verification

From the checkout root, create a dedicated environment (Python 3.8+):

```sh
python3 -m venv .venv-code-health
.venv-code-health/bin/python -m pip install -r devtools/code_health/requirements.txt
.venv-code-health/bin/python -m unittest discover -s devtools/code_health/tests -v
.venv-code-health/bin/python -m devtools.code_health --base-ref origin/main
```

The check reads Git-tracked `.h .hpp .c .cpp .tpp` files under `Common/` and
`SerialPrograms/`, excluding any directory component starting with `3rdParty`.
Untracked files are excluded, so `git add` new source before checking it.
Analysis errors and malformed baselines fail the command.

| Check | Limit |
| --- | --- |
| File length | 500 physical lines advisory; 1000 hard |
| Function length | 50 physical lines, signature through closing brace; 25 recommended |
| Nesting | 4 lexical brace levels inside the function body |
| Parameters | 7 |

Lizard 1.17.31 recognizes C++ functions without a compilation database. The
brace extension counts nested blocks, lambdas and initializer braces; it
ignores enclosing namespace/class braces, comments and string literals. This
is lexical code-health analysis, not a C++ compiler or clang-tidy replacement.
Macro-generated functions and advanced syntax may require parser follow-up.
Cyclomatic complexity is deliberately not enforced in this first rollout.

## Existing debt and the ratchet

`baseline.json` records each existing violation by path, qualified function
signature and check, with its measured value, reason and `review_at` date.
The initial review date is 90 days after generation. An exception expires on
that date, failing only if the violation still exists. Fix the code; don't
raise limits, regenerate the baseline to hide growth or add blanket skips.
A renamed function has a new identity and needs to meet the limits.

With `--base-ref`, current metrics must meet both the dated baseline ceiling
and the actual base revision's value. A previously improved function cannot
regrow to its older baseline allowance. New violations fail. CI reads the
baseline from the trusted base, so editing the PR's baseline cannot mask a
regression. The initial rollout is the only bootstrap exception: if the base
has no baseline, the head baseline is read, but base measurements still ratchet.
For the initial snapshot only:

```sh
.venv-code-health/bin/python -m devtools.code_health --write-baseline --review-at YYYY-MM-DD
```

The CI job runs on every PR, including drafts, and pushes to main. It runs
self-tests first, then scans the current tree against the PR base / previous
main revision. No hardware, Qt or CMake build is needed for this job.
Pass/fail C++ fixtures cover each threshold; integration tests use temporary
Git repos to prove nonzero exits, bootstrap, exclusions and baseline tampering.
These Python tests exercise the linter, not SerialPrograms application code.

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
Unpack downloadable model ZIPs exactly as the existing CI's “Install downloadable
resources for tests” step does. Missing fixture/model resources are not passes.

For a configured Linux build:

```sh
.venv-code-health/bin/python devtools/verify.py --build-dir SerialPrograms/bin --binary SerialPrograms/bin/SerialPrograms --base-ref origin/main
```

On macOS use `--binary SerialPrograms/bin/SerialPrograms.app/Contents/MacOS/SerialPrograms`;
on Windows use `--binary SerialPrograms/bin/RelWithDebInfo/SerialPrograms.exe`.
The command runs linter self-tests, the ratchet, CMake build, then native tests;
it stops at the first nonzero exit and propagates it. Native tests run from
the binary directory, matching Linux/Windows resource layout. On macOS it uses
a separate `agent-native-tests` profile (named profiles are macOS-only); Qt is
offscreen on all platforms. Use this checkout's build/binary,
not a release installation or another worktree's executable.

## Agent work loop

Feature branch -> one concern -> test/verify -> read exit status -> review diff
and CI -> draft PR on dayfine/Arduino-Source. No upstream PR publication by
agents. Refresh canonical rules from origin/agents at task start and before
PR mutations. Canonical AGENTS.md lives on that notes branch and is never
merged into code branches. Shared infrastructure needs portable, regression-tested behavior;
program changes additionally need the appropriate live run evidence.

Reuse: `metrics.py` and `policy.py` accept source/metrics as values and know
nothing about this repository's Git layout. `git_files.py` and `__main__.py`
are the integration layer. Keep them here until another consumer justifies
extracting a separate package/repository.
