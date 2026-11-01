# Agent notes for dayfine/Arduino-Source

This `agents` branch holds everything that is not code: rules for coding
agents, milestone notes, run records and evidence. It has no code history.
**Never merge, rebase or cherry-pick it into a code branch, and never open a PR
from it.**

Agents working here: Codex (implementation and live validation on a Switch)
and Claude (getting PRs ready for review). The user decides scope and priorities.

## Session startup from a code checkout

At the start of each task, from any code worktree, refresh and read the shared
rules rather than relying on a branch-local copy:

```sh
git fetch origin agents
git show origin/agents:AGENTS.md
git show origin/agents:pokemon_sv/PR_WORKFLOW.md
```

Once per clone, install the pre-push code-health gate (it covers every worktree):

```sh
sh <(git show origin/agents:tools/install_hooks.sh)
```

A push of a branch that fails `devtools/code_health` against its fork PR base is refused, the same
check as CI's "C++ Code Health" job. Fix the failures in that PR's own commits; `--no-verify` only
with the user's OK. A PR is not "CI green" while Code Health is red.

All linked worktrees share these remote refs. Re-read the PR rule before any
GitHub PR mutation. If fetching fails, report that the cached rules may be
stale; the user's explicit restrictions still apply. Keep the dedicated notes
worktree on `agents`. Commit all new notes and run records here;
`bbq/m7-board-read-notes` is retired from active use. Do not copy or merge rules
and notes into code branches. Global Codex and Claude instructions bootstrap
this lookup for code branches that have no rules file. See
[WORKTREE_RULES.md](pokemon_sv/WORKTREE_RULES.md) for the audit and setup.

For substantial autonomous work, check account quota before large chunks and
after milestones using `python3 tools/read_codex_quota.py` from this notes
worktree. Follow [QUOTA_MONITORING.md](pokemon_sv/QUOTA_MONITORING.md): use actual
quota/reset readings, checkpoint and pace work when low, and report unknown
quota honestly if the local account cannot be queried. Do not confuse quota
with context or token counters, or promise background monitoring after a turn.

## Commands: keep them tight

Every shell command can raise a permission prompt for the user. Keep each one small and
obviously safe.

- **One purpose per command.** No chains that build, deploy and clean up in one line. No `cd A
  && … && cd B && …` across repos: use `git -C <path>`.
- **No `rm` on globs or variables.** Delete only what you created, by explicit path, inside your
  own worktree or your temp folder. For git trees use `git clean` or `git worktree remove`.
  Anything else that needs deleting goes in your report for the user.
- **Stay in your lane.** Don't touch other agents' worktrees, `~/BDSPLab/builds` (except a deploy
  your brief asks for), the SerialPrograms profile folders, or the consoles. Console B is run by
  the main session only.
- **No broad scans.** No `find`/`du`/`ls -R` from `/` or `~`, and no `system_profiler` in loops:
  they walk network volumes and trigger macOS privacy prompts. Use explicit paths.
- **Wait with one background command,** not chained `sleep`s.

## Roles

- **Codex owns:**
  - the lab branch `bbq/validation-stack`
  - this branch's notes and run records
  - new feature branches until their first fork PR
- **Claude owns:**
  - PR branches and their descriptions, once a PR exists
  - rebasing PRs as their parents merge
  - answering review comments
  - preparing upstream handoffs for the user
- **Hand over, don't push over.** Changes to an existing PR are handed over as
  lab commits plus a run record. Neither agent pushes to a branch the other
  owns.

## Dispatching subagents

Dispatched agents follow [agent-harness](https://github.com/dayfine/agent-harness)'s liveness and
long-chain rules: [.agents/rules/agent-liveness.md](.agents/rules/agent-liveness.md) and
[.agents/rules/long-chain-ownership.md](.agents/rules/long-chain-ownership.md). In short, a subagent
never waits on anything outside its own turn: it pushes, opens or updates the draft fork PR, reports
the PR number and tip SHA, and stops. The main session waits on CI with
`sh dev/lib/wait-pr-checks.sh <PR> -R dayfine/Arduino-Source` as a tracked background task (0
green, 10 a check failed, 1 still pending at the cap), and `dev/lib/chain-runner.sh` runs longer
chains.

Each role has a definition under `.agents/agents/`, carrying its standing rules (build, test,
checkout and finish commands): `autostory-code`, `pr-maintenance`, `run-records` and `reviewer`.
A dispatch brief points at the definition (`git show origin/agents:.agents/agents/<name>.md`, or
`subagent_type: <name>` once linked into `~/.claude/agents/` from a checkout of this branch, as for
skills) and carries only the task: PR or branch, tip SHA, scope, prior review, rework round. Don't
restate the standing rules in a brief. `sh bin/agent-standing-rules-check.sh` and
`sh bin/agent-harness-check.sh` must pass; the Harness Check workflow runs them on PRs into `agents`.

## Where PRs live

See [pokemon_sv/PR_WORKFLOW.md](pokemon_sv/PR_WORKFLOW.md). In short:

- **No agent opens, reopens or marks ready a PR on upstream**
  (`PokemonAutomation/Arduino-Source`). All agent-created PRs are **drafts on
  `dayfine/Arduino-Source`**, including changes ready for review. Hand anything
  intended for upstream to the user. This replaces the former Claude-publishes
  workflow, per the user's global instructions on 2026-10-23.
- Upstream allows **at most two open PRs from us, drafts included**
  (CONTRIBUTIONS.md). This limit does not authorize agents to publish upstream.
- Use `gh` through `~/.local/bin/gh`, first on PATH. Do not bypass the guard
  through the Homebrew executable, an override environment variable, another
  client, or direct API calls. The user's explicit instructions govern any
  exception; readiness and parent merges are not authorization.
- **Stacked draft PRs** go on the fork, based on their parent's branch.
- **Upstream references in fork PRs** are written in full:
  `PokemonAutomation/Arduino-Source#1631`.
- **Descriptions** tell the reviewer what to read: a "How to review" section for
  any PR that moves or restructures code (see "Writing the PR description" in
  PR_WORKFLOW.md).

## Code conventions for PR branches

- **Match the repo's style:**
  - comments explain why, at the density of the surrounding code
  - `virtual ... override`
  - nested namespace braces
  - CRLF line endings in source files
- **Dependencies:**
  - no forward declarations; include the real header
  - never declare a function in one module's header and define it in another
  - module dependencies point one way
- **Tests:** native C++ unit tests registered in the game's `*_Tests.cpp`
  (e.g. `PokemonSV_Tests.cpp`).
  - Data-driven, with designated initializers so every value has a field name,
    and no if/else-if chains.
  - Failure messages say what was expected and what was read.
  - No Python or source-extraction tests.
- **Seams:** to test controller logic, inject the game steps through a small
  interface, with a console implementation and a scripted fake. Examples:
  `BBQGameActions`, `BBQRerollActions`, FRLG `WildBattleExitActions`.
- **Errors:**
  - `UserSetupError` for a user's setup or configuration problem.
  - Error reports (`SEND_ERROR_REPORT`) only for program failures.
  - Don't throw from deep in a routine to end the program; return a result and
    let the program decide.
- **Behavior changes:** defaults keep today's behavior. A behavior change goes
  in its own PR and its title says so. Prep refactors go in their own PR, kept
  minimal.
- **Shared code:** reuse code from a prerequisite PR; never copy it into a
  second PR.
- **Notes:** nothing from this branch goes into a PR branch: no milestone notes,
  run records, fixtures or manifests.

## C++ style

**Order of authority:** upstream `AGENTS.md` and `CONTRIBUTING.md` (formatting, naming,
Qt/thread/mutex/filesystem bans, errors), then `CodingAgentContext/IncludeOrder.md`, then
the surrounding code, then this section. It adopts the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) and the
[Abseil Tips of the Week](https://abseil.io/tips/) (TotW) where they don't conflict, with
Abseil translated to `std`. The build is **C++23** with Apple Clang, MSVC/clang-cl
(`/W4 /WX`) and GCC/clang (`-Werror`). `std::format` and `std::ranges` are already used;
`std::span` and `std::string_view` are available but rare. A first use in a file is a style
change a reviewer will notice, so keep it where it clearly pays off.

### Where the repo wins over Google

- **Naming:** types `PascalCase` (programs `Name_Descriptor` / `Name`), functions and
  locals `snake_case`, members `m_name`, constants and option members `UPPER_SNAKE`.
  Enumerators follow their file (`SEND_ERROR_REPORT` and `BattleExited` both exist).
  Files are `PokemonFRLG_Navigation.h/.cpp`. Not Google's `kName`, `name_`, `PascalCase()`.
- **Formatting:** 4 spaces, CRLF, `if (x){`, `}else{`, `namespace X{`, `class A : public B{`,
  always braced multi-line bodies. No 80-column limit. Wrap long parameter lists the way
  nearby code does. Close namespaces with a bare `}`.
- **`virtual ... override`** on overrides (Google says `override` alone).
- **Guards and includes:** `PokemonAutomation_<Name>_H`. Include order per
  IncludeOrder.md with the file's own header **last** (Google puts it first).
- **Exceptions are used** (Google bans them). Game failure:
  `OperationFailedExceptionWithScreenshot::fire()` / `throw_and_log<OperationFailedException>`.
  Config problem: `UserSetupError`. Programmer bug (Google's `CHECK`): `InternalProgramError`.
  Let `ProgramCancelledException` and `ProgramFinishedException` propagate.
- **Forward declarations:** stricter than Google. None in our code; include the header.
- **Unsigned:** counts and indexes are `size_t` / `uint8_t` (Google prefers `int`).
  Match that, and see TotW #227 below.
- **Callbacks:** APIs such as `run_until()` take `std::function<...>&&`. Match the API
  rather than TotW's by-value advice.
- **`emplace_back`** is the repo default (TotW #112 prefers `push_back`). Keep it, but never
  emplace a raw `new` into a container of `unique_ptr`.
- **`using namespace Pokemon;`** inside the game namespace in a `.cpp` is normal. Never write
  a using-directive at namespace scope in a header (TotW #153).

### Adopted

**Strings**
- **`std::string_view` by value** for new read-only text helpers. Never store it past the
  call (TotW #1, #180). Existing APIs keep `const std::string&`.
- **`std::format`** for messages with several parts, instead of `+` / `std::to_string`
  chains or `ostringstream` (TotW #3, #124). Raw string literals for embedded JSON (TotW #64).

**Construction and constants**
- **Initialize every scalar**, using default member initializers (TotW #61, #146, #182).
  Example: `DpadPosition last_dir = DPAD_NONE;`.
- **Designated initializers** for aggregates and option structs, not just tests (TotW #172).
- **`= default` / `= delete`** for special members, not empty bodies or private
  declarations (TotW #131, #143).
- **`explicit`** on single-argument and converting constructors (TotW #142).
- **A constructor leaves the object usable.** No `init()` step; use a factory returning
  `std::unique_ptr` or `std::optional` (TotW #42).
- **Constants:** `constexpr` in the `.cpp`, `inline constexpr` in a header. Tables with
  non-trivial destructors go in a function-local `static const`, the repo's `database()`
  idiom (TotW #140, #168).

**Ownership and lifetimes**
- **`std::make_unique`, never `new`**, and move a `unique_ptr` explicitly (TotW #126, #187).
- **Parameters:** pass `T&` / `const T&` unless ownership transfers. Take a smart pointer
  only to take or share ownership (TotW #188, #234).
- **"Value or nothing" is `std::optional<T>`**, not a `unique_ptr` (TotW #123).
- **Don't keep references or views to arguments or temporaries** (TotW #5, #101, #107,
  #116, #180). CI's clang-query already rejects one case: an `ImageViewRGB32` built from
  `VideoSnapshot&&`.
- **No reference or `const` data members in types that get copied or assigned**
  (TotW #177). Use a pointer or pass the reference separately.

**Functions**
- **Return values, not output parameters** (TotW #176, #11). Return a struct or result
  enum. State carried across calls belongs in an object with methods (e.g. replace
  `repeat_dpad(context, DpadState& state, ...)` with `DpadWalker::press(...)`).
- **Don't `std::move` a returned local, and don't return `const T`** (TotW #120, #166).
- **Sink parameters by value, then `std::move`** (TotW #117, #77).
  Example: `OperationFailedException(ErrorReportMode, std::string message)`.
- **No bare `bool` parameters in new APIs.** Use an `enum class` or an options struct, and
  no defaulted `bool` (TotW #94, #173). Bad examples:
  `exit_wild_battle(..., bool stop_on_move_learn, bool prevent_evolution)`,
  `checkpoint_reattempt_loop(..., bool save = true)`. For existing ones, write
  `/*save=*/false` at the call site.
- **No sentinel values in new APIs** (`-1`, `""`, `0` meaning "none"). Return
  `std::optional` or a result enum like `WildBattleExitResult` (TotW #171). For an optional
  input, overload or take `const T*`, not `const std::optional<T>&` or `= ""` (TotW #163).
- **No top-level `const` on by-value parameters in declarations** (TotW #109).
- **File-local helpers go in an unnamed `namespace{`**, not `static` and not the header
  (TotW #186).
- **Overloads only when every overload means the same thing**
  (`move_cursor_to_option(..., BattleMenuOption / BattleMoveOption)`) (TotW #148).
- **Lambdas, not `std::bind`** (TotW #108). Free functions over extra public methods
  (TotW #99).

**Containers and algorithms**
- **Read-only sequence parameters:** `std::span<const T>` where callers hold arrays or
  `std::array` (TotW #93). Prefer `std::array` over C arrays in new structs, e.g.
  `TorterraMove attacks[4]`.
- **Lookups:** `contains()`, `if (auto it = m.find(k); it != m.end()){`, structured
  bindings over `.first/.second`, `std::less<>` for string-keyed maps (TotW #158, #165, #144).
- **Guard empty containers before `size() - 1`** (TotW #227). Don't use `.at()` as a bounds
  check; check explicitly and throw `InternalProgramError` (TotW #224).
- **Prefer `std::clamp`, `std::minmax`, `std::ranges::find_if`** over hand-written loops
  (TotW #231).
- **`auto` only when the type is obvious or pure noise** (iterators, `make_unique`)
  (TotW #232).

**Types, state and time**
- **`enum class`**, and `switch` over it with no `default:`, so `-Wswitch` flags a new
  value. After the switch, throw `InternalProgramError` (TotW #86, #147).
- **`explicit operator bool`.** Don't rely on implicit int or pointer to `bool` conversion
  in new APIs (TotW #141).
- **No mutable globals or config knobs.** Pass options down in a struct
  (TotW #45, #52, #103). Example to fix: `extern bool g_stop_at_first_failure;` set from
  the program's option (bdsp/event-sandgem-center).
- **Durations are `std::chrono` / `Milliseconds` with literals** (`160ms`), never bare ints.
  Deadlines are `WallClock`.
- **No reader/writer locks.** Use the `Mutex` wrapper; contention is not an issue here
  (TotW #197).

**Comments and tests**
- **Function comments sit directly above their function** and state the start screen, end
  screen, return value and failure behavior (CONTRIBUTING; Google "Function Comments").
  Example to fix: `save_game`'s comment sits above `add_wrapped_log` in
  `PokemonBDSP_AutoStoryTools.h`. No commented-out code, no `// ----` banners.
- **Test the contract through the seam, not internals** (TotW #135). Each case's inputs
  are visible in its row, with no shared mutable fixture (TotW #122).

### Checkable

- **Checked now by `devtools/code_health`:**
  - file length (500 advisory / 1000 hard)
  - function length (50; 25 recommended)
  - nesting (4) and parameters (7)
  - public methods (7), public data in a mutating class, single-method interface
  - layering: shared code must not include game modules
- **Checked now by CI:**
  - the tabs job
  - clang-query (`std::filesystem::path` built from `std::string`; `ImageViewRGB32` built
    from `VideoSnapshot&&`)
  - `-Werror` / `/WX`: unused variables, `-Wswitch` on clang/GCC (TotW #147), 64-to-32-bit
    narrowing on Apple Clang
- **Could add to `code_health` (lexical, new lines only):**
  - non-const namespace-scope variables and non-const `extern` (TotW #103)
  - `new` / `delete` outside allocators (TotW #126)
  - `std::bind` (TotW #108), `return std::move(local)` (TotW #120)
  - a top-level `const` by-value parameter in a header declaration (TotW #109)
  - a defaulted `bool` parameter, or two or more `bool` parameters (TotW #94)
  - `using namespace` at namespace scope in a header (TotW #153)
  - forward class declarations in new headers
  - `std::thread` / `std::mutex` / `std::async` / `std::filesystem` (upstream bans)
  - control-flow formatting: `) {`, `} else`, `namespace X {`, single-line `if`
  - a function comment directly above a different function
- **Review only:** sentinel values (TotW #171), ownership and lifetimes (TotW #180,
  #188), output parameters (TotW #176), comment content.

## Layout

- `pokemon_sv/`: BBQ milestone notes (M7–M13), the validation workflow, the PR
  workflow, run audits, BP ledgers and fixtures. These files were moved from
  `tests/pokemon_sv/` on `bbq/m7-board-read-notes`. Paths inside them that say
  `tests/pokemon_sv/` now mean `pokemon_sv/` on this branch.

### Plans and state: where to look first

- [pokemon_sv/MILESTONE_STATE.md](pokemon_sv/MILESTONE_STATE.md): current
  state of each milestone and the active lab run (owned by Codex).
- [pokemon_sv/ROADMAP.md](pokemon_sv/ROADMAP.md): milestones ordered by payoff
  and dependency, with what to reuse from existing code. **Approved by the
  user on 2026-10-21**, with the cave experiments moved ahead of M8a (see its
  "Decisions" section). The queue in MILESTONE_STATE.md should be updated to
  match. It adds N1 (fly from anywhere) and M15 (area batching).
- [pokemon_sv/M9a_PLAN.md](pokemon_sv/M9a_PLAN.md): the PR-by-PR plan for
  faster, bounded Auto Battle quests.
- [pokemon_sv/PR_PLAN.md](pokemon_sv/PR_PLAN.md): how all BBQ lab work
  becomes upstream PRs, by layer and chain, with a size estimate per PR.
  Live status is in [pokemon_sv/PR_PROGRESS.md](pokemon_sv/PR_PROGRESS.md);
  open problems in [pokemon_sv/KNOWN_ISSUES.md](pokemon_sv/KNOWN_ISSUES.md).
  Switch runs that check each stage, and `tools/run_summary.py` to write up
  their results, are in [pokemon_sv/STAGE_RUNS.md](pokemon_sv/STAGE_RUNS.md).
- [pokemon_sv/PR_WORKFLOW.md](pokemon_sv/PR_WORKFLOW.md): where PRs live and
  how to write their descriptions.
- [pokemon_sv/lab/MACOS_LOCAL_SIGNING.md](pokemon_sv/lab/MACOS_LOCAL_SIGNING.md): persistent lab signing, permission troubleshooting and reuse by other agents.
- [pokemon_sv/lab/CONTROL.md](pokemon_sv/lab/CONTROL.md): how to build, launch
  and control a lab instance and the Switch.
- [lab/LAB_BUILDS.md](lab/LAB_BUILDS.md): lab builds base on `lab/main`, which
  holds lab-only tooling; upstream-bound branches never merge it.

### Skills

Shared Claude Code skills live under `skills/<name>/SKILL.md`. Install one into
a session with `ln -s "$PWD/skills/<name>" ~/.claude/skills/<name>` from a
checkout of this branch; edit it here and PR the change, not the local copy.

- [skills/console-iteration/SKILL.md](skills/console-iteration/SKILL.md): the
  dev loop for a console automation segment (define, observe, hypothesize,
  verify by hand, fix once, rerun), one train on the console, hard stops,
  learning, escalation and follow-ups.

## C++ harness and verification (fork issue #40)

Fork PR #41 is merged into `origin/main` at `67ed6141d`. It provides reusable
measurements/policy under `devtools/code_health/`, incremental comparison
against the base revision, and a PR/main CI job. Run from a code checkout
with its dedicated Python environment:

```sh
.venv-code-health/bin/python devtools/verify.py --build-dir SerialPrograms/bin --binary SerialPrograms/bin/SerialPrograms --base-ref origin/main
```

This command requires a configured build, Packages/downloaded resources and the
CommandLineTests fixture checkout; see `devtools/code_health/README.md`. On
macOS use the app's `Contents/MacOS/SerialPrograms` binary; on Windows use
`RelWithDebInfo/SerialPrograms.exe`. It runs linter self-tests, the incremental
code-health check, CMake build and native registered C++ tests, stops at the first failure,
and returns that exit status. Agents must read that status before opening a
code PR; report any unavailable prerequisite instead of claiming a pass.
Linter-only changes run its self-tests and incremental scan; they do not require
a SerialPrograms rebuild. Python self-tests are for this Python tool only;
application tests remain native C++.

Limits: file 500 lines advisory / 1000 hard, function 50 physical lines (25
recommended), 4 brace nesting levels inside a function, 7 parameters. Only added/modified/renamed
C++ files are analyzed against the base revision.
New violations and worsening existing metrics fail; unchanged legacy violations
pass. There is no baseline/exception registry or repository-wide scan. Tests
generate X-line input in memory or temporary repos; do not check in bulk fixture
files. **Fix/extract code; do not raise limits to hide growth.** Keep one concern per
branch/PR and require applicable checks/CI before review. Shared infrastructure needs portable regression coverage;
gameplay changes additionally need appropriate live validation.

### Let CI build and test; build locally only when you must

Upstream's C++ CI (six jobs: Mac, Mac Intel, Ubuntu clang/gcc, Windows
clang/MSVC) builds the app and runs the native unit tests against
`PokemonAutomation/CommandLineTests`. Every job is skipped on draft PRs, and all
fork PRs are drafts, so a push alone runs nothing but the tab check. Dispatch
the jobs on the fork branch instead:

```sh
for w in mac windows-clang ubuntu-default; do
  gh workflow run cpp-ci-serial-programs-$w.yml -R dayfine/Arduino-Source --ref <branch>
done
```

- **Use CI** for review fixes, refactors and anything whose check is "compiles
  everywhere and the unit tests pass". A run takes 8–30 minutes. Windows clang
  catches warnings that Apple Clang misses.
- **Build locally only** for a lab deployment or Switch run, for tests that
  need images not yet in upstream CommandLineTests, or for a quick
  edit-compile loop in a worktree that already has a build.
- **Compile just the files you touched** for a quick local check, in a build
  directory that already exists: `ninja -C <build> CMakeFiles/SerialProgramsLib.dir/Source/<path>.cpp.o`
  takes seconds. Leave the full build and the tests to CI.
- **Keep one persistent build directory and switch branches in it**, rather
  than creating a worktree with a cold build per task; ninja rebuilds only what
  the switch changed, and the shared ccache covers the rest.
- **One local build at a time, `-j4`.** A full build takes 0.5–1.5 GB per
  compiler process; three cold builds at full parallelism exhausted a 16 GB Mac.
  Reuse an existing build directory rather than a fresh worktree per task: a
  new worktree on another base revision is mostly ccache misses.
- **A fresh macOS configure** needs
  `PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig` and
  `-DWrapOpenGL_AGL=<CommandLineTools SDK>/System/Library/Frameworks/AGL.framework/AGL.tbd`,
  or the final link fails with "framework 'AGL' not found".
- Report which CI runs passed (with links) instead of claiming a local pass.

Canonical rules remain here on `agents`; never merge this branch into code.
