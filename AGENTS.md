# Agent notes for dayfine/Arduino-Source

This `agents` branch holds everything that is not code: rules for coding
agents, milestone notes, run records and evidence. It has no code history.
**Never merge, rebase or cherry-pick it into a code branch, and never open a PR
from it.**

Agents working here: Codex (implementation and live validation on a Switch)
and Claude (getting PRs ready for review). The user decides scope and priorities.

## Session startup from a code checkout

Read `/Users/difan/Projects/Arduino-Source-agents/AGENTS.md` at the start of each
session, even when the working directory is a code worktree without AGENTS.md.
Keep this dedicated worktree on `agents`. Commit all new notes and run records
here; `bbq/m7-board-read-notes` is retired from active use. Do not copy these
rules or notes into code branches.

## Roles

- **Codex owns:**
  - the lab branch `bbq/validation-stack`
  - this branch's notes and run records
  - new feature branches until their first fork PR
- **Claude owns:**
  - PR branches and their descriptions, once a PR exists
  - rebasing PRs as their parents merge
  - answering review comments
  - moving PRs from the fork to upstream
- **Hand over, don't push over.** Changes to an existing PR are handed over as
  lab commits plus a run record. Neither agent pushes to a branch the other
  owns.

## Where PRs live

See [pokemon_sv/PR_WORKFLOW.md](pokemon_sv/PR_WORKFLOW.md). In short:

- **Codex opens PRs only on the fork** (`dayfine/Arduino-Source`). Codex must
  not open or reopen upstream PRs. A change being ready for review does not
  authorize Codex to publish it upstream; hand it to Claude instead. Reaffirmed
  by the user on 2026-10-23.
- **Upstream** (PokemonAutomation/Arduino-Source) only holds PRs that are ready
  for review: no unmerged parent, built and tested.
- **Drafts and stacked PRs** go on the fork, based on their parent's branch.
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
- [pokemon_sv/PR_WORKFLOW.md](pokemon_sv/PR_WORKFLOW.md): where PRs live and
  how to write their descriptions.
- [pokemon_sv/lab/CONTROL.md](pokemon_sv/lab/CONTROL.md): how to build, launch
  and control a lab instance and the Switch.
