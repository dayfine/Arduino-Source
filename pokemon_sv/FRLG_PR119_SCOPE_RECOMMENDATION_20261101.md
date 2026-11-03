# FRLG navigation PR scope recommendation

Date: 2026-11-01. Recommendation only; no PR branches changed.

Reviewed fork PR #119 at `08829d226be2946171cf7a916c906bbde1f88e4f`,
against #118 (`frlg/walk-wild-legs`). The diff contains 1,422 additions and
78 deletions across 12 files. Its scope extends from fixing route execution
to recovering position and planning replacement routes.

## Recommended distribution

Very little else should move into #114. The shared planner change already
moved there in `7008f8c67259d09a85513625a87cdfe4f2d11a06`: per-step/leg
`wild` tracking, optional `per_open_wild_stop`, and tests. That is the right
placement; the weight defaults to zero.

| Destination | Changes | Reason |
|---|---|---|
| #114: shared core | Keep the generic wild-leg tracking and optional stop cost already added. | Game-independent planner capability. |
| #117: FRLG navigation MVP | Menu/message precondition checks; stop on a trainer battle rather than attempt to flee. | Basic executor correctness. Keep these as clearly identified fix commits, since #117 currently claims unchanged collected code. |
| #118: wild-step and obstacle handling | Detect wild tiles across map boundaries; wait for late battles; distinguish warp fades from battle transitions. | These complete #118's existing promise to watch the correct legs for battles. |
| #118: obstacle handling | Prevent holds into ledges, with tests. | Closely related to its existing "never hold into a sign" fix. Conservatively reject ambiguous ledges rather than assume their direction from the currently loaded maps. |
| Separate recovery PR, possibly narrowed #119 | `OverworldReanchor`, candidate positions, wall-hold search, automatic replanning, `hold_to_warp`, recovery limits and planning-context inputs. | Introduces a new navigation policy and needs its own contract and live validation. |
| Remove | Unused tile-by-tile walking, `step_window`, `settle`, delayed-step attribution and their dedicated tests. | FRLG abandoned this approach; retaining it expands the shared API without a production user. |

Keep the FRLG weight of `300` separate from the generic capability. Put that
policy choice in the recovery PR, with evidence for the resulting routes.
Cross-map wild detection belongs in #118 independently.

Keep the shared walker's new stop/result hooks with their first consumer.
`stop_on_interrupt`, `stop_if_blocked`, and the corresponding result handling
support recovery. They belong in the recovery PR, even though the files live
under `Pokemon/Navigation`; shared location alone does not make them #114
material.

The resulting stack is:

**#114 shared capabilities → #117 baseline FRLG navigation → #118 encounter/obstacle fixes → #119 explicit position recovery.**

## Recovery review requirements

The recovery PR should answer: given an uncertain position, can we safely
establish a known position and resume under the caller's original constraints?

Before enabling recovery:

- Fix `plan_reanchor()` narrowing its candidate spread from ±4 tiles toward
  zero merely because a wider search found no recovery. Search failure is not
  evidence that the player is closer to the planned line. Keep the uncertainty
  set fixed unless an observation justifies narrowing it.
- Preserve the caller's planning context: effective maps/event flags, costs
  and any required intermediate route constraints. The current implementation
  replans to the final destination with default costs and, without supplied
  maps, loads maps with default event flags.
- Test the complete walk → interruption → recovery → resume sequence, including
  cases where no safe recovery exists. Planner tests alone do not establish
  executor correctness.
- Demonstrate recovery on console before enabling it by default. At review
  time the PR description said reanchoring had not been run on console; the
  current-head Mac/Windows/Ubuntu workflows were still queued or running.

The source review did not run the console or post GitHub comments.

## References

- [#114: shared core](https://github.com/dayfine/Arduino-Source/pull/114)
- [#117: FRLG navigation MVP](https://github.com/dayfine/Arduino-Source/pull/117)
- [#118: wild-step and sign handling](https://github.com/dayfine/Arduino-Source/pull/118)
- [#119: walker fixes and recovery](https://github.com/dayfine/Arduino-Source/pull/119)
