# Lab builds base on `lab/main`

Every lab build (BDSP AutoStory on console B, FRLG nav on console A, any
future lab) is built from a branch that merges `lab/main`. Lab-only tooling
lives there, not in fork `main`.

This applies once fork PR #92 (`lab/main-setup` into `lab/main`) is merged.
Until then `lab/main` is a plain copy of fork `main`.

## Why a separate base

Fork `main` mirrors upstream `main`, and fork PR branches (some of which go
upstream) are based on it. Tooling that only the lab uses must not reach those
branches, so it gets its own long-lived branch.

## What `lab/main` contains

- fork `main`, merged in every morning by `~/Projects/pa-tools/sync_branches.py`
  (the 7:00 launchd job) right after it syncs fork `main` with upstream;
- the Command File Runner (`lab/lab-console`, fork PR #58), with `walk`
  (`lab/cfr-walk`, fork PR #84) and `history` (`lab/cfr-history`).

The lab control folder is not listed: it is already in fork `main` (fork
PR #76). The FRLG CFR commands (`frlg_route`, `frlg_max_repel`,
`frlg_rng_reset`) need the FRLG nav code, so they stay on `lab/frlg-nav-cfr`.

`git log --oneline origin/main..origin/lab/main` must show lab tooling only.

## Rules

- **Lab branches merge `lab/main`** (`bdsp/*` lab branches, `lab/frlg-nav-cfr`,
  and new ones). They get fork `main` through it; don't merge `main` directly
  to pick up lab tooling. Merge, never rebase.

  ```sh
  git fetch origin
  git merge origin/lab/main
  ```

- **Upstream-bound branches never merge `lab/main`**, directly or through a
  lab branch. A feature branch meant for a PR is based on `main`; the lab
  branch merges the feature branch, not the other way round.
- **`lab/main` is never merged into `main`**, and never force-pushed.

## Adding lab-only tooling

1. Put it on its own branch from `origin/lab/main` (e.g. `lab/cfr-<command>`),
   CRLF line endings as in the rest of the source.
2. Open a fork draft PR with base `lab/main`, and dispatch CI on the branch
   (fork drafts skip CI):

   ```sh
   for w in mac windows-clang ubuntu-default; do
     gh workflow run cpp-ci-serial-programs-$w.yml -R dayfine/Arduino-Source --ref <branch>
   done
   ```

3. After it merges, lab branches pick it up with `git merge origin/lab/main`.

A one-line fix to existing lab tooling may be committed and pushed to
`lab/main` directly; say so in the run record.

Tooling that depends on a feature still under review (like the FRLG CFR
commands) stays on that feature's lab branch until the feature is in `main`.

## Upstreaming the Command File Runner later

CFR goes upstream as a separate, small PR from a branch based on `main`, not
from `lab/main`, and only after it has been pitched on the Discord, as
upstream's CONTRIBUTING asks for new programs. The user publishes it; agents
only prepare the branch and the handoff (see
[PR_WORKFLOW.md](../pokemon_sv/PR_WORKFLOW.md)). Once it is in `main`, the
daily sync brings it into `lab/main` and the lab copy goes away.
