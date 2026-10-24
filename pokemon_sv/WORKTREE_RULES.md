# Shared rules across worktrees

2026-10-23 PDT. Codex audited 21 linked worktrees. The rules are shared through
`origin/agents`; updating code branches is not needed to distribute them.
Do not merge, rebase or cherry-pick the notes branch into a code branch.

## Current publication rule

No agent opens, reopens or marks ready a PR on
`PokemonAutomation/Arduino-Source`. New PRs are drafts on
`dayfine/Arduino-Source`. The user receives anything intended for upstream.
The upstream cap of two open PRs, including drafts, grants no publication
permission to agents. These instructions supersede the old Claude-publishes
workflow. See [PR_WORKFLOW.md](PR_WORKFLOW.md).

## One source, every checkout

The following local global instruction files bootstrap the same lookup:

- `/Users/difan/.codex/AGENTS.md`
- `/Users/difan/.claude/CLAUDE.md`

At the start of each task, from the current checkout:

```sh
git fetch origin agents
git show origin/agents:AGENTS.md
git show origin/agents:pokemon_sv/PR_WORKFLOW.md
```

Re-read the publication rule before any PR mutation. All linked worktrees share
Git's remote refs, so one fetch refreshes their common `origin/agents` ref.
A separate clone or another computer needs its own fetch and the global
bootstrap instructions installed there. If fetching fails, report the cached
rules as potentially stale; the user's explicit restrictions still apply.

Code branches do not need copies of the notes or the rules. The legacy
`docs/agents-md` branch has an older tracked root AGENTS.md; treat it as a
historical document, and use the canonical rules above. Its tracked content
was not edited as part of this task.

Codex discovers its instruction chain once per launched session. Existing
sessions must explicitly read the updated rules or restart; changing files
cannot make another running conversation reread them automatically.
[Official discovery documentation](https://learn.chatgpt.com/docs/agent-configuration/agents-md).

## GitHub CLI guard

The user's `/Users/difan/.local/bin/gh` wrapper blocks upstream PR creation,
reopening and marking ready. The wrapper itself was not changed. It was only
on the interactive startup PATH; the audit found noninteractive commands
resolving Homebrew gh instead. Local-bin precedence is now set at the end of
`.zshenv`, `.zprofile` and `.zshrc`.

Verified all three resolve `gh` to `/Users/difan/.local/bin/gh`:

```sh
zsh -lc 'command -v gh'
zsh -ic 'command -v gh'
zsh -c 'command -v gh'
```

`bash -n ~/.local/bin/gh` passed. No upstream PR command or mutation was issued
to test the guard. Existing shells may need a new shell or a PATH refresh.
Never bypass the wrapper or substitute a direct API call for a blocked action.

## Worktree audit

Snapshot taken before these documentation edits. Dirty-path counts include
tracked and untracked changes reported by Git. Existing edits were preserved;
no code branch was rebased, reset, switched or changed by this work.

| Worktree | Branch | Dirty paths | Root instruction files |
| --- | --- | ---: | --- |
| Arduino-Source | `frlg/evolution-exit-stall` | 0 | None; global bootstrap applies |
| Arduino-Source-1663 | `detached` | 0 | None; global bootstrap applies |
| Arduino-Source-agents | `agents` | 0 | AGENTS.md (tracked) |
| Arduino-Source-agentsmd | `docs/agents-md` | 0 | AGENTS.md (tracked) |
| Arduino-Source-bbq-lab | `bbq/validation-stack` | 0 | None; global bootstrap applies |
| Arduino-Source-c12 | `bbq/catch-any-court` | 0 | None; global bootstrap applies |
| Arduino-Source-c18 | `bbq/startup-fly-check` | 0 | None; global bootstrap applies |
| Arduino-Source-isolated | `prototype/sv-isolated-hunt` | 0 | None; global bootstrap applies |
| Arduino-Source-m3 | `bbq/battle-entry-guard` | 0 | None; global bootstrap applies |
| Arduino-Source-m4 | `bbq/persistent-quest-retries` | 0 | None; global bootstrap applies |
| Arduino-Source-m5 | `bbq/unsupported-quest-rerolls` | 38 | None; global bootstrap applies |
| Arduino-Source-m6 | `bbq/exhausted-quest-actions` | 41 | None; global bootstrap applies |
| Arduino-Source-m7 | `bbq/m7-board-read-notes` | 1 | None; global bootstrap applies |
| Arduino-Source-m7a-release | `bbq/reuse-quest-board` | 0 | None; global bootstrap applies |
| Arduino-Source-m7b | `bbq/m7b-replan` | 0 | None; global bootstrap applies |
| Arduino-Source-m7b-release | `bbq/replan-after-reroll` | 0 | None; global bootstrap applies |
| Arduino-Source-m8a | `bbq/m8a-party-restoration` | 6 | None; global bootstrap applies |
| Arduino-Source-r10 | `lab/r10` | 0 | None; global bootstrap applies |
| Arduino-Source-r6 | `lab/r6` | 0 | None; global bootstrap applies |
| Arduino-Source-r7 | `lab/r7` | 0 | None; global bootstrap applies |
| Arduino-Source-r8 | `lab/r8` | 0 | None; global bootstrap applies |

Nineteen worktrees have no root instructions file; the canonical `agents`
worktree and legacy `docs/agents-md` worktree have tracked AGENTS.md files.
Branch age and dependency rebases are a separate code-maintenance task. PR
branches remain owned by Claude; uncommitted work and historical validation
revisions must not be overwritten to distribute instructions.
