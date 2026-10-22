# Where BBQ/FRLG PRs live

Rule set by the user on 2026-10-21. It applies to Codex and Claude.

## The rule

**Upstream (PokemonAutomation/Arduino-Source) only holds PRs that are ready for
review: no unmerged parent PR, built, and tested.** Everything else is a PR on
the fork (dayfine/Arduino-Source) until it is ready.

- **Never open a draft PR upstream.** Never open a PR upstream whose parent PR
  has not merged.
- **Open work-in-progress and stacked PRs on the fork.** On the fork, a PR's base
  can be another fork branch, so stacked PRs show only their own change.
  Upstream they cannot: they must target `main` and show all their parents'
  commits.
- **Base a stacked fork PR on its parent's branch**, e.g. a child of upstream
  #1632 uses base `bbq/save-milestone-tracker`. A PR with two parents uses one
  parent as its base, merges in the other, and says so in its banner.
- **When an upstream parent merges:**
  1. Rebase each child onto `main`.
  2. Rebuild and run its tests.
  3. Open (or reopen) its upstream PR, with a `Depends on: nothing` banner.
  4. Close the fork PR, linking the upstream one.

  Its own children stay on the fork, now based on its branch.
- **Moving a PR upstream is Claude's job** (see "Branches and ownership" in
  VALIDATION_WORKFLOW.md). Codex hands over branches and run records, and does
  not open or reopen upstream PRs.
- **"Draft PR" in VALIDATION_WORKFLOW.md means a PR on the fork.** When Codex
  opens a draft PR for new work (step 10), it opens it on dayfine/Arduino-Source,
  based on its parent's branch, never upstream.

## Linking

- **Upstream PR numbers and fork PR numbers overlap.** In a fork PR, `#1631`
  means fork PR #1631, which does not exist. Write upstream references in full:
  `PokemonAutomation/Arduino-Source#1631`.
- **Every PR description starts with a banner.** It names the base branch and
  the parent PRs (fork or upstream), and says whether the diff shows only this
  change.
- **Moved PRs keep their history.** A PR moved from upstream to the fork gets a
  comment on the closed upstream PR, linking the fork PR. That PR's review
  threads stay readable upstream.

## Writing the PR description

Write the description so a reviewer knows what to read and what to skim.
Maintainers review on github.com, which does not mark moved code.

- **Order:** start with the banner (see Linking above), then the problem, the
  change, and the tests. Keep it short. Run details and ledgers go in a linked
  run record, not in the PR.
- **Add a "How to review" section whenever a PR moves or restructures code**
  (refactors, seams, code moved to another file or class). It says:
  - which code is moved unchanged, and where it came from;
  - which parts are new or changed, and the order to read them in (usually the
    new logic first, then the tests);
  - the local command that colors moved lines apart from real changes, even
    when re-indented:

    ```
    git diff --color-moved=dimmed-zebra --color-moved-ws=allow-indentation-change main...<branch>
    ```
  - when code is only re-indented in place, that GitHub's "Hide whitespace"
    setting (`?w=1`) removes it from the diff.
- **Split a move from a change** into separate commits when the code allows it,
  and say "review commit by commit". When they can't be split (e.g. code moved
  into a new interface's methods), say so and rely on the "How to review"
  section.
- **Measure before claiming.** Count the moved lines with the command above,
  e.g. "about half of the changed lines are moved".

Example: upstream PokemonAutomation/Arduino-Source#1663.

## Current map (2026-10-21)

Open upstream, ready for review: #1628, #1631, #1632, #1636, #1640, FRLG #1663.

| Fork PR | Was upstream | Change | Fork base | Parents |
| --- | --- | --- | --- | --- |
| #2 | #1625 | Logging | `bbq/quest-identity` | upstream #1631 |
| #3 | #1629 | Failure screenshots | `bbq/diagnostics-and-baseline` | fork #2 |
| #4 | #1626 | Save milestones | `bbq/save-milestone-tracker` | upstream #1632 |
| #5 | #1639 | Endless-reroll guard | `bbq/save-milestone-tracker` | upstream #1632 |
| #6 | #1642 | Failed-quest memory (M4) | `bbq/save-milestone-tracker` | upstream #1632 |
| #7 | #1641 | Detector tests | `bbq/bp-reader-crop` | upstream #1628 |
| #8 | #1659 | Verified reroll | `bbq/quest-planner` | upstream #1640 |
| #9 | #1644 | Unsupported-quest rerolls (M5) | `bbq/verified-reroll` | fork #8 |
| #10 | #1645 | Out-of-retries action (M6) | `bbq/persistent-quest-retries` | fork #6, with fork #8 merged in |
| #11 | #1664 | FRLG evolution stall fix | `frlg/wild-battle-exit-seam` | upstream #1663 |

Recheck with `gh pr list -R PokemonAutomation/Arduino-Source --author dayfine`
and `gh pr list -R dayfine/Arduino-Source` before acting; this table goes
stale as PRs merge.

## For the validation loop

- **Use exact PR heads.** The lab baseline in VALIDATION_WORKFLOW.md should be
  built from the current heads of the fork and upstream PRs above, recorded by
  SHA.
- **Report results on the fork PR.** A run that verifies a fork PR goes in that
  fork PR. Claude carries it into the upstream PR when the PR moves up.
