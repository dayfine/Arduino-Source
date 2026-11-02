---
name: run-records
description: Writes run records, notes and lab tooling on the dayfine/Arduino-Source agents notes branch, checks them, pushes, opens or updates a draft PR into agents, and stops. Never edits code branches or touches a console.
harness: project
---

You work on the `agents` notes branch: run records, milestone notes, skills, `tools/` scripts and
this harness. Nothing you write goes into a code branch, and nothing from a code branch comes here.

## At the start of every session

1. `git -C /Users/difan/Projects/Arduino-Source fetch origin agents`, then read
   `git show origin/agents:AGENTS.md` and `git show origin/agents:pokemon_sv/PR_WORKFLOW.md` and obey
   them. If the fetch fails, say the cached rules may be stale.
2. `~/.local/bin/gh` is first on PATH. Never bypass its upstream guard. PRs go on
   `dayfine/Arduino-Source` as drafts with base `agents`.
3. Merge, never rebase or force-push. Keep secrets, tokens and copyrighted game assets out of
   records; link screenshots by path unless the brief asks to commit them.

## Standing rules — the dispatch brief does not repeat these

<!-- standing-rules: pinned by bin/agent-standing-rules-check.sh; see .agents/rules/standing-rules.md -->
The dispatcher's brief carries only the task: the issue or PR and its tip SHA, the scope or the delta
since the last review, the prior review's location, and the rework iteration. The rules below apply
without being restated.

- **No waiting.** Run every check in the foreground with `timeout: 600000`, split into scoped calls
  when one call could run longer: `sh bin/agent-harness-check.sh` and
  `sh bin/agent-standing-rules-check.sh` when `.agents/`, `bin/` or `dev/` changed; a touched
  `tools/*.py` script's own self-test or `python3 -I tools/<script>.py --help`; a touched shell script
  under `sh -n`. Read the exit code from a file (`…; echo $? > rc`), never through a pipe.
  Never background a long command and end the turn waiting for it: nothing wakes a subagent. If a command
  does run in the background, keep polling its output in bounded loops until it finishes.
- **Bounded loops.** Every wait loop has an iteration cap and checks that the producing process is
  still alive.
- **Your own checkout.** Work only in your own worktree:
  `git -C /Users/difan/Projects/Arduino-Source worktree add -b agents-<topic> <your temp folder>/wt-<topic> origin/agents`.
  Never edit the dedicated notes worktree or a code worktree.
- **Commit early, finish in one turn.** Commit and push as soon as the scoped checks pass; the full
  build and test run comes after the push. Then, in the foreground and without ending the turn:
  verify → commit → push → open the PR → confirm the PR's file list. Never end with an unpushed commit.
  If the push or PR step fails twice, report the branch, the commit id and "PR NOT opened".
- **Your stage ends at the PR.** Never wait for CI or any other event after pushing: report the PR
  number and tip SHA and stop; the dispatcher waits for CI (`.agents/rules/long-chain-ownership.md`).

## Report

PR number, tip SHA, files written, checks run and their exit codes.
