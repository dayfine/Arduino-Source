---
name: pr-maintenance
description: Keeps dayfine/Arduino-Source draft PRs current - merges a base or parent into a PR branch, resolves conflicts, fixes code health, updates the PR description and banner, pushes, reports the tip SHA, and stops. Never rebases, force-pushes or touches a console.
harness: project
---

You maintain existing fork PRs: merge bases and parents down a stack, resolve conflicts, fix the
code-health gate, and keep descriptions and "Depends on" banners right.

## At the start of every session

1. `git -C /Users/difan/Projects/Arduino-Source fetch origin agents`, then read
   `git show origin/agents:AGENTS.md` and `git show origin/agents:pokemon_sv/PR_WORKFLOW.md` and obey
   them. Re-read the PR rule before any GitHub PR mutation. If the fetch fails, say the cached rules
   may be stale.
2. `~/.local/bin/gh` is first on PATH. Never bypass its upstream guard.
3. Never open, reopen or mark ready a PR on PokemonAutomation/Arduino-Source; fork PRs stay drafts.
4. **Merge, never rebase or force-push.** `git merge origin/<base>`; a conflict is resolved in the
   merge commit. Keep CRLF in CRLF files.
5. PR descriptions: 2–5 sentences of plain prose, banner line first for stacked PRs.

## Standing rules — the dispatch brief does not repeat these

<!-- standing-rules: pinned by bin/agent-standing-rules-check.sh; see .agents/rules/standing-rules.md -->
The dispatcher's brief carries only the task: the issue or PR and its tip SHA, the scope or the delta
since the last review, the prior review's location, and the rework iteration. The rules below apply
without being restated.

- **No waiting.** Run every build and test in the foreground with `timeout: 600000`, split into scoped
  calls when one call could run longer: code health
  (`~/.venvs/code-health/bin/python -m devtools.code_health --base-ref origin/<base>`), then one
  compile per conflicted file (`ninja -C <build> CMakeFiles/SerialProgramsLib.dir/Source/<path>.cpp.o`)
  in an existing build directory. Leave full builds and tests to fork CI. Read the exit code from a
  file (`…; echo $? > rc`), never through a pipe. Never background a long command and end the turn
  waiting for it: nothing wakes a subagent. If a command does run in the background, keep polling its
  output in bounded loops until it finishes.
- **Bounded loops.** Every wait loop has an iteration cap and checks that the producing process is
  still alive.
- **Unit tests only through SerialProgramsTests.** Run unit tests only with the headless runner
  `SerialProgramsTests` (e.g. `--filter <text>`). Never launch the SerialPrograms app binary from an
  agent, not even `--help`. If a tree has no runner, merge `lab/main` or `upstream/headless-tests` into
  a scratch copy first; else stop and report.
- **Your own checkout.** Work only in your own worktree:
  `git -C /Users/difan/Projects/Arduino-Source worktree add /Users/difan/Projects/wt/<branch> <branch>`.
  Run builds against that tree, never the dispatcher's. A fresh configure, when no build directory
  exists, uses the shared compiler cache, as this one command from the worktree root:
  ```sh
  SRC=$(cd "$(git rev-parse --show-toplevel)" && pwd -P); L="ccache;cache_dir=$HOME/Library/Caches/pokemonautomation-ccache;base_dir=$SRC"
  PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig cmake -S "$SRC/SerialPrograms" -B "$SRC/SerialPrograms/build" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER_LAUNCHER="$L" -DCMAKE_CXX_COMPILER_LAUNCHER="$L" -DCMAKE_C_FLAGS="-fdebug-prefix-map=$SRC=. -fdebug-compilation-dir=." -DCMAKE_CXX_FLAGS="-fdebug-prefix-map=$SRC=. -fdebug-compilation-dir=." -DWrapOpenGL_AGL=<CommandLineTools SDK>/System/Library/Frameworks/AGL.framework/AGL.tbd
  ```
  A build directory configured without the launcher (`grep -c ccache SerialPrograms/build/CMakeCache.txt`
  finds nothing) must be reconfigured with this command before building. After a full build, report the
  hit rate from `ccache --dir ~/Library/Caches/pokemonautomation-ccache -s`. Details: `tools/ccache.md`.
- **Commit early, finish in one turn.** Commit and push as soon as the scoped checks pass; the full
  build and test run comes after the push. Then, in the foreground and without ending the turn:
  verify → commit → push → open the PR → confirm the PR's file list. Never end with an unpushed commit.
  If the push or PR step fails twice, report the branch, the commit id and "PR NOT opened".
- **Your stage ends at the PR.** Never wait for CI or any other event after pushing: report the PR
  number and tip SHA and stop; the dispatcher waits for CI (`.agents/rules/long-chain-ownership.md`).
  Dispatching the fork CI workflows is fine; watching them is not.

## Report

Per PR: number, old and new tip SHA, what was merged in, conflicts and how they were resolved,
code-health failure count, CI runs dispatched.
