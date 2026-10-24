# Issue #40 — harness delivery state

2026-10-24. Draft fork PR: https://github.com/dayfine/Arduino-Source/pull/41
Code branch: harness/code-health; worktree /Users/difan/Projects/Arduino-Source-harness.
Head b34fb5bc052885bfdbf519559bd7fb444a9a2371, based on fork main 8e973c48f.
The initial snapshot was refreshed after main advanced during implementation;
no C++ gameplay/application changes were made by this PR.

## Delivered

- Reusable Python C++ metrics and dated baseline/ratchet policy, separate from
  Git integration. Pinned Lizard 1.17.31; no compile database needed.
- File length 500 advisory/1000 hard, function physical lines 50 (25 recommended),
  nesting 4 lexical brace levels inside function, parameters 7. CCN deferred.
- First-party Git-tracked h/hpp/c/cpp/tpp under Common/SerialPrograms, excluding
  3rdParty* components. Strings/comments and enclosing scopes do not add depth.
- 3,117 files, 1,504 baseline entries: 1,191 function length, 105 nesting,
  192 parameters, 16 file length. review_at 2027-01-22, reason per entry.
- Trusted base snapshot plus actual base metrics prevents new debt/regrowth and
  head-baseline tampering. Exceptions fail on expiry while still needed.
- PR (including draft) and main-push CI. Pass/fail fixtures, real temporary-Git
  integration tests, CRLF/preprocessor regression and fail-fast runner tests.
- devtools/verify.py: self-tests -> ratchet -> configured CMake build -> native
  registered C++ tests; propagates failures. Native cwd is the build directory;
  macOS uses a separate profile with its own DownloadedResources prerequisites.
- Canonical AGENTS.md on agents documents policy/local command. Never merge
  agents into a code branch. README explains reusable parts and setup.

## Validation

19 self-tests passed locally. Whole tree scan against origin/main: 0 failures.
Git diff whitespace check passed. Latest GitHub jobs at head b34fb5bc0:
code-health passed (29s), no-tabs passed (14s). Existing full C++ CI build jobs
skip drafts; native suite was inspected/documented, not rerun for this tooling
change. Fixtures/models/configured build remain required to use the full
verification command. No upstream PR was created and the issue remains open.

This initial implementation/CI repair is complete. PR branch now handed to
Claude per canonical ownership rules; Codex should hand over further commits
rather than pushing over another agent's PR work.

## Follow-up task

Resume isolated encounter queue in
pokemon_sv/research/isolated-encounters/2026-10-23/IMPLEMENTATION_QUEUE.md.
Release AZP was restored; leave it running during code work.
