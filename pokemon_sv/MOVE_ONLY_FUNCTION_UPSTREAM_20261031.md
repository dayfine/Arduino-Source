# MoveOnlyFunction upstream publication

The user explicitly requested “upstream #72 now”, authorizing this upstream PR.

- Fork review: https://github.com/dayfine/Arduino-Source/pull/72
- Upstream draft: https://github.com/PokemonAutomation/Arduino-Source/pull/1810
- Existing clean branch: `upstream/move-only-function`.
- Commit: `95e3e64798885e842683c151afe7e86f93621c37`, one commit, five files.
- Wrapper and tests match fork #72's latest head `87071ad59` exactly.
- No PR branch or other agent worktree was modified.
- Clean branch already passed Mac (41013109446), Windows Clang (41013112034),
  and Ubuntu Default (41013114771) CI. Mac logs confirm all four wrapper tests passed.
- CRLF-aware diff check passed; merge-tree with upstream `147c0dbd8` had no conflicts.
- Fork code-health failure (41211073904): internal `Callable` type-erasure interface
  triggers the single-method-interface rule. Reported transparently in the upstream
  draft description; not suppressed or claimed to pass.
- Publication used the guarded `gh` wrapper's explicit-request override.
- Open upstream count after publication: two (#1754 and draft #1810).
