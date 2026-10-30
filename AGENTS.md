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
