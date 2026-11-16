> **DRAFT.** This page describes an optional feature that is not built yet. It is updated if it lands.
> Status (2026-11-15): planned (Slice 8), optional.

# Local worker for routine operations

A separate process that runs heavy local jobs (builds, test runs) as queued tickets, with the
existing guards: the heavy-work gate, the disk floor, `-j4` and `nice`.

## What you will get

- Builds and tests queue instead of colliding; refusals say why (disk low, memory, a console
  run in progress).
- A console run is never slowed down or stopped by a build.
- It does not touch GitHub, PRs or worktrees, and the existing daily cleanup jobs stay.

## Example (sketch, not built)

*"Build the PLZA lab tree."* → `{"ticket": 41, "state": "queued", "ahead": 1}`; later
`{"ticket": 41, "state": "done", "exit": 0, "log": "~/BDSPLab/build-plza.log"}`, or
`{"ticket": 41, "state": "refused", "reason": "console run in progress"}`.
