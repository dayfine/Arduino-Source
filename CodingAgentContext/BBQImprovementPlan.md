# BBQ reliability and throughput plan

## Objective and evidence

Improve sustained net BP/hour on Switch 1 by first preventing stalled boards,
then reducing avoidable overhead, and finally improving target acquisition.
This addresses [ComputerControl #263](https://github.com/PokemonAutomation/ComputerControl/issues/263),
with task visibility related to [#264](https://github.com/PokemonAutomation/ComputerControl/issues/264).

The October 19–20 logs reconstruct approximately 2,050 BP earned and 90 BP spent
on rerolls. The longest run averaged about 401 net BP/hour before 05:05, then
stalled for 3.2 hours on photo-savanna / pickup-10 / sneak-up. All three were blue
quests: replacing the board would cost 30 BP before replacement difficulty and
menu time. These are inferred quest rewards, not measured balance snapshots;
reward-source disagreements and accounting limits are documented in the
[analysis report](../analysis/bbq-2026-10-20/report.md).

Build preparation is complete: the local arm64 app compiles and a `bbq-dev`
profile is prepared. The [M0 local baseline](../analysis/bbq-2026-10-20/m0/findings.md) completed
10 quests in 53m47s (~368 inferred net BP/hour). M1 diagnostic logging is now
implemented locally, with Switch validation pending; recovery behavior is unchanged.

Execution follows the [incremental workplan](BBQWorkplan.md): one behavior change
per milestone, validated before proceeding. The four stages below define scope
and priorities rather than four large implementation batches.

## 1. Prevent stalls and preserve observability

First establish that every known blocked quest has a bounded recovery path.

- Persist the configured retry budget across rounds for an outstanding quest.
  With one configured retry, allow two attempts total, not two per round.
  Clear that budget after verified completion or replacement, not a board reread.
- Include pickup-10 in the configuration and honor reroll policy for unsupported
  pickup/catch-flying/photo-flying quests. Unsupported status should not bypass
  the user's selected action.
- Apply a configurable fallback after retry exhaustion (reroll/skip/stop).
  Distinguish skipping from recovery: skipping can leave the board unproductive.
  Start with blue rerolls; keep an explicit policy for 50-BP red rerolls.
- Verify the board after rerolling. Handle insufficient BP or unchanged/uncertain
  state with bounded retries and a clear stop/report, rather than repeated charges.
- Add a board-level no-progress detector to catch combinations of failures and
  uncertain OCR that individual limits miss. Re-read and capture evidence before
  taking the configured recovery action. An unreadable fourth position can be
  an absent bonus quest; it is not automatically a payable reroll target.
- Only run battle handling after confirmed battle entry; only save/reset when
  the completion count advances to a save milestone.
- Log requested quest slug, attempt number, phase timings, result/recovery,
  reroll target/cost and before/after board. Read BP from the existing board
  snapshot to reconcile earnings without another panel-opening sequence.
  Capture landing and failure screenshots. Keep detailed inference/controller
  traces separate from these user-facing decisions; a global logger redesign
  is not a prerequisite for the BBQ fix.

Validation: reproduce the observed stalled board (photo-savanna / pickup-10 /
sneak-up); demonstrate that retry counters survive a round boundary, policy
rerolls replace blocked quests, and the board resumes progress without an
unbounded retry/spending loop. Check selection/retry transitions offline and
validate UI confirmation/OCR on the Switch. Keep route timing unchanged for
this experiment, so recovery-policy effects can be measured separately.

## 2. Measure quest velocity and choose work economically

Primary sample: the continuous **01:22:16–05:05:19** run. The approximate 401 net
BP/hour baseline refers to this period. Tables count outer task batches, each
including internal retries, failed attempts, travel/recovery and verification.
Between-task board management, reroll costs and incidental completions are not
charged into these task-only rates. Do not interpret them as whole-farm speed.

| Requested quest | Reward BP | Successes / finished batches | Mean batch min | Direct BP per task-hour |
| --- | ---: | ---: | ---: | ---: |
| Rock-type photo, red | 150 | 1 / 1 | 1.54 | 5,834 |
| Sour sandwich, red | 150 | 1 / 1 | 3.03 | 2,975 |
| In-flight photo, blue | 30 | 4 / 4 | 1.14 | 1,578 |
| Polar photo | 30 | 1 / 1 | 1.22 | 1,477 |
| Coastal photo | 30 | 1 / 1 | 1.22 | 1,471 |
| Make TM | 20 | 6 / 6 | 0.82 | 1,469 |
| Wild Tera battle, red | 100 | 1 / 1 | 4.88 | 1,230 |
| Catch Rock-type | 40 | 1 / 1 | 2.13 | 1,128 |
| Catch Ground-type | 40 | 1 / 1 | 2.74 | 877 |
| Travel | 20 | 8 / 8 | 1.38 | 867 |
| Auto-battle 30, red | 130 | 1 / 1 | 9.41 | 829 |
| Swimming photo | 30 | 6 / 7 | 2.13 | 725 |
| Catch-any | 20 | 1 / 1 | 2.91 | 412 |
| Auto-battle 10 | 20 | 9 / 9 | 5.40 | 222 |
| Catch Normal-type | 40 | 1 / 3 | 4.82 | 166 |
| Sneak-up | 20 | 4 / 26 | 3.22 | 57 |

Earlier pre-stall sessions add important failure cases: Tera-self defeat succeeds
in 1/6 finished batches, consuming 25.47 minutes for 30 BP (71 direct BP/task-hour),
with two more interrupted batches. Catch-poison fails 3/3 finished batches,
consuming 16.38 minutes. These are separate settings/session conditions and
should not be silently pooled into the continuous-run rates.

Findings and policy implications:

- Favor fast photos and crafting when available. Several red quests pay very
  well, but one observation is a candidate for further measurement, not a
  reliable success probability or repeatable farming rate. Reds are scarce.
- Catching is route-specific: Rock/Ground/Any work in this small sample while
  Normal performs poorly. Do not blacklist all catching from an aggregate rate.
- Sneak-up consumes 83.6 minutes of the 223-minute pre-stall period while only
  four batches succeed. Recovery can improve throughput before any full stall.
- Photo-savanna has no task in this continuous pre-stall sample, then fails
  in all 29 later batches. It did succeed once in an earlier session. This
  illustrates changing state and correlated retries, not a stationary p=0 or p=1.
- Auto-10 is reliable but expensive. Do not reroll solely because its direct
  rate is below 400: blue completion advances the next red quest, and rerolling
  does not. Five observed red rewards average 136 BP, suggesting an illustrative
  13.6-BP progress credit per blue completion. That brings auto-10's task-only
  value from 222 to about 374 BP/hour before overhead. This model credit is not
  extra earned BP and must not be added to the ledger alongside red rewards.
- A completed-only sandwich sample omits setup/fatal interruptions. The broader
  sample has two censored bitter-sandwich tasks; investigate before calling the
  route reliable despite its high successful-run return.

Use an observed net earning rate r and an estimated task's expected reward V
(including bonus progress) and duration T to compare: reroll opportunity gain
approximately r*(T - reroll-menu-time) - reroll-cost - V. Include replacement
difficulty and reroll chains; this is an initial decision model, not a guarantee.
At 400 BP/hour, 10 BP is 1.5 minutes and 50 BP is 7.5 minutes of productive time.

Start with a fixed, interpretable ranking plus live failure fallback. Collect
more outcomes before adding an adaptive scheduler. Measure by requested slug
and shared route, rather than translated labels. Repeat comparable runs, record
gross/spent/net BP, active and wall time, failure time, and empirical 10/30/60-minute
returns. Report small samples and temporal changes rather than treating repeated
attempts at the same missing spawn as independent observations.

Data: `analysis/bbq-2026-10-20/quest-velocity.csv`, generated by
`quest_velocity.py`; it contains both the continuous and all-pre-stall scopes,
censored counts and an explicitly illustrative bonus-adjusted value column.

## 3. Reduce avoidable overhead and fix easy route dependencies

Pickup rerolling belongs in stage 1, not here. After the recovery policy works:

- Avoid healing when no battle occurred; gate healing/recharging on actual
  HP/PP/Tera needs. Log why a visit is required.
- Remove duplicate flights only when the existing state satisfies the next
  route's position and camera assumptions. A same-named destination does not
  prove equivalence: healing may move the character, and flying again may be
  establishing a known anchor. Preserve or explicitly re-establish that anchor.
- Defer Travel if another selected task can complete it incidentally, then
  re-read before running a dedicated lap. Measure whether this improves the
  observed route mix rather than assuming progress carries over usefully.
- Add a bounded, configurable spawn-loading wait, followed by validation. A
  fixed extra delay everywhere can reduce throughput; compare Switch 1 outcomes.
- Inspect the Pride Rock camera-reset asymmetry: catch-any and Sneak-up press L
  after their final Switch 1 diagonal movement; the shared photo-savanna route
  does not. Treat a heading reset as a controlled experiment, not a proven fix.
- Diagnose radar-ball fallback and selected fly-point verification. Instrument
  the failure and test localization/console conditions before relying on
  subsequent timed routes.

Validation: measure phase durations to identify real redundant work, then change
one route/transition at a time. Require improved net BP/hour with preserved
arrival reliability, not merely fewer commands or faster successful cases.

## 4. Raise the sustainable rate beyond the recovered baseline

Separate three sources of improvement: more productive board time, less overhead,
and genuinely better target acquisition. Avoid setting 400 BP/hour as a ceiling:
stage 1–3 may already exceed it; stage 4 should be justified by the remaining
measured bottlenecks.

- Introduce bounded encounter phases: verified arrival → spawn readiness →
  camera sweep/lock-on → throw/engage → confirmed battle → result. Preserve
  failure reason and screenshots for each transition.
- Replace Tera-self's single Kleavor dependency with an ordinary nearby wild
  target. Keep target restrictions for typed catches, Sneak-up positioning,
  and shiny detection behavior.
- Recover locally after a miss when safe, try another target or known route,
  then return to reroll policy after a shared time budget. Do not convert a
  missing-spawn retry loop into an unbounded camera-search loop.
- Track shared route failures (Pride Rock serves Catch-any, Savanna photos and
  Sneak-up). Use evidence to decide whether to wait, reset orientation, respawn,
  choose a different location, or cool down that route.
- Then consider combining nearby objectives and reducing plaza trips with
  explicit navigation entry states. Ground-item collection is a separate later
  experiment: first a bounded known route or nearby-target detector, not general
  terrain exploration.

Validation: compare the same quest families on Switch 1 with matched setup,
record median/tail completion times and acquisition failures, and retain a
bounded fallback. A faster single catch is insufficient evidence of a better
overnight farmer. Use repeated long runs to assess sustained net BP/hour and
zero-return windows.

## Delivery and measurement

Start with observability, then the independent save/battle-entry guards, retry
persistence, unsupported-quest policy and bounded failed-quest rerolling. Validate
recovery on the actual stalled board before changing routes or scheduling.

For each behavior change, compare equivalent Switch 1 conditions and record net
BP/hour, time lost to failures, reroll costs and zero-return windows. Distinguish
source-level tests from successful gameplay validation. A single fast success
or one 1/1 sample is not sufficient to claim sustained improvement.

See [BBQWorkplan.md](BBQWorkplan.md) for commit-sized milestones and acceptance
criteria. Later route work remains conditional on the bottlenecks observed after
recovery fixes; 400 BP/hour is a baseline, not a promised ceiling or target cap.
