# M7b/M7a combined trial, 2026-10-21 PDT

Status: both phases STOPPED normally. M7a reuse/invalidation and M7b
failed-reroll replanning OBSERVED. End balance 14,250 BP; game saved at Central
Plaza with the full-health Lv.100 Primarina lead. Targeted acceptance complete;
one-hour stability/deadline stopping NOT verified.

- Tested integrated revision ea047a9309ce8c429d288cbfb77f88266f32d34d.
- Binary SHA-256 8626deb7a10585cb3887a5166db95f8ffacef43aa20203c94207f79d5de7814b.
- Exact app /Users/difan/Projects/Arduino-Source-m7b/build-prep/app/SerialPrograms.app;
  PID 25737, isolated profile bbq-m7b. Old app PID 660 exited.
- Build/native checks: full arm64 PASS; 58 registered native C++ tests PASS.
- Native starting snapshot: 14,070 BP, Central Plaza, Savanna Photo / Make TM /
  Catch Any, three blue slots. Previous save/party handoff verified full-health
  Lv.100 Primarina lead and all six full HP.
- New app opened on Switch HOME; resumed the running Pokémon Violet game.
- Camera and microphone privacy prompts were explicitly allowed through their
  permission buttons after the user asked to handle them. Native feed verified.
- Loaded UI exclusion rows verified visibly: Sneak Up / Reroll and Pickup 10 /
  Reroll. No hatch row or accidental Auto Battle 10 skip row. Other settings:
  1000 quest capacity, one retry, exhausted quest Reroll, save interval 10,
  zero eggs / Keep Going; copied notifications disabled.
- User asked whether Sneak Up should be skipped: explained it is replaced via
  Reroll, to avoid the known missing-target route and exercise planner reuse.
  Catch Any remains runnable for M7b exhausted-quest replanning evidence.
- Startup confirmed RUNNING, controller matched and overworld detected.

## Observer and coverage

Local evidence directory in the m7b build checkout:
build-prep/m7b-live-2026-10-21/ contains settings-at-start.json,
start-board-14070BP.png, observe.py, status.json/checkpoints.jsonl and run.log.
Observer exec session 93328 records every five minutes. It pins exact PID/path/
profile and the sole Start event; detects new starts and relinquishes control.
It uses the latest program-state transition. Deadline 22:23:05 PDT: request
normal Stop at the next after-attempt/scheduling read, with an additional
ten-minute boundary allowance and STOPPED verification. No historical STOPPED
may be mistaken for the state of a newer run.

M7b acceptance: failed attempt rerolled → new discard-batch log → normal round
ending → scheduling before any remaining old queued dispatch. M7a acceptance:
FOUR recognized slots and a planner reroll → scheduling and first-dispatch reuse
logs, then fresh later-dispatch reads. Starting three-slot board is insufficient.
If naturally occurring paths do not trigger, prepare a separately recorded
controlled phase; never label general farming as optimized-path verification.
No release PR has been opened. Record ending BP, scoped log, trigger timestamps,
new issues and final classifications after the run.

## Phase one: M7b queue-delay path observed

21:23:05.963–21:31:12.357 PDT (8m06s); early boundary selected to set up the
explicit M7a acceptance path, not a failure or a completed hour of regression
coverage. First Savanna Photo exhausted two attempts; it rerolled to Travel:
- 21:27:27.004: replacement verified and new discard-remaining-batch log.
- 21:27:37.980: scheduling pass after normal round actions.
- 21:27:55.740: fresh pre-dispatch board read.
- 21:28:12.728: Travel 500m dispatched, ahead of old queued Make TM/Catch Any.
- 21:29:44.957: Travel completed, 92.23 seconds after dispatch.
- 21:31:12: Make TM completed; recorded count two. Stop at that stats boundary.

Verified reroll → Travel dispatch gap **45.72 seconds**, including 10.98 seconds
of round-end actions and two fresh board reads. This fixes the observed deferred
queue pattern; do not call it a controlled throughput comparison with the old
4+ minute example, because the quest mix/round position differ.
Final phase-one screenshot: **14,100 BP**. Starting 14,070 + 20 Travel + 20 TM
− 10 blue reroll = +30, reconciled exactly. No M7a reuse logs in phase one;
all its transaction boards were three-slot until the second completion.

The second completion created the red Auto Battle 30 quest. End board: Auto
Battle 30 / Flying Photo / Auto Battle 10 / Catch Any. This is an eligible
four-slot board for controlled M7a validation. The application was closed
normally, preserving game state; no additional game save was claimed here.

## Phase two: controlled four-slot planner reroll

Started **21:34:47.158 PDT**, same tested binary/revision, new PID 27590 and
profile bbq-m7b-reuse. One additional valid policy row: Photo in Flight / Reroll;
Sneak Up / Reroll and Pickup 10 / Reroll retained. All other trial settings
unchanged. Start balance **14,100 BP**, prior native four-slot screenshot retained.
Camera/microphone were already granted when reopening the identical app.

Local evidence: build-prep/m7b-reuse-live-2026-10-21/ in the m7b checkout.
Observer session **9238**, independent start identity and output files. Deadline
**22:34:47 PDT**, then next safe read boundary (up to ten minutes allowance).
The original session 93328 exits on the earlier STOPPED and has no ownership of
this profile. Evaluate planner scheduling reuse/first-dispatch reuse and fresh
later reads, plus any subsequent failed-reroll replans. Record separate phase
end balances and do not combine configurations into a single velocity claim.

## Phase-two optimized path observed (stability run ongoing)

At 21:35:24.384806, the verified four-slot board changed only Flying Photo to
Coastal Photo. Scheduling reuse logged at 21:35:24.384839 and first-dispatch
reuse at 21:35:24.384898, immediately followed by Auto Battle 30. No board OCR
call occurred between verification and dispatch. Fresh post-attempt read at
21:52:47.243406, quest success at 21:52:57.356249, then a fresh pre-dispatch read
at 21:52:57.356296 before Coastal Photo at 21:53:07.544978. Thus both reuse and
subsequent invalidation are directly observed; general stability trial remains
in progress through its one-hour deadline.

Auto Battle 30 dispatched at 21:35:24 and returned success at 21:52:57, about
17m33s. There were 34 detected-kill events, including additional events after
its target was reached, followed by unconditional return/healing. This augments
M9, not a new M7 regression. Detected-kill events are a proxy; the actual quest
success is established by the post-attempt panel replacement.

## Dedicated release extraction checks

- M7a branch bbq/reuse-quest-board, head 8dbace6c7. Base the eventual fork PR on
  bbq/exhausted-quest-actions (fork #10); merges M5 f7e3ba6f0 (fork #9).
  Feature commit 3f1f79f0a, plus restoration of two pre-existing out-of-BP native
  fixture fields lost during extraction. Full arm64 build PASS; **41 native
  tests PASS**. Local evidence in Arduino-Source-m7a-release/build-prep/.
- M7b branch bbq/replan-after-reroll, head 30e84abef. Base eventual fork PR on
  bbq/reuse-quest-board; also merges save milestones 548ac951c (fork #4).
  Carries native save-scenario adaptation 0ec332559 and feature 2ff0fdd65.
  Full arm64 build PASS; **46 native tests PASS**. Local evidence in
  Arduino-Source-m7b-release/build-prep/.
- Save parent is necessary for intended behavior: without it, newly ended
  zero-completion batches would hit the pre-existing save-at-zero bug.
- Production feature patch identity matches tested lab code using stable
  zero-context patch IDs: M7a f7407f30c7c67cce768563b8a6afb43a9f0f639e;
  M7b 382744314cf342618dd23223566725ca15410618. M7b all-file feature patch ID
  also matches d1e8e53d6c0d2d2a955eecd4c2d1275708440ca9.
  Range-diff evidence is saved in the M7b release build-prep directory.
- Neither release branch includes notes under tests/ in its diff; actual unit
  tests remain in the repository's native PokemonSV_Tests.cpp.
- Release builds ran in the background during phase two. Treat the live timing
  as an observed sequence, not a controlled performance/throughput benchmark.
- Branches pushed to the fork; PRs not yet opened. Existing PR branches were
  not pushed or rewritten. Running binary remained the pinned lab M7b build.

## Early-stop decision and BP status request

At 22:08 PDT the user requested BP earned/spent in Current Run and asked to
consider stopping after analysis. Both intended M7 paths were already directly
observed, including invalidation. Additional time was dominated by known route
failures: Auto30 17m33s; Catch Any two failures and reroll about 5m19s; Swimming
Photo two failures and reroll about 2m40s. Travel subsequently completed at
22:05:50 (four recorded phase-two completions). Catch Fire uses the same failing
route and was in progress. Chose an early normal Stop at the next fresh
after-attempt/planning boundary rather than extending solely to one hour.
This is targeted live acceptance with limited regression runtime, not a full
one-hour stability result or a controlled throughput comparison.

New requested follow-up: [M14 BP status counters](../../../M14.md): gross earned,
reroll expense, net and rate, with incidental credit and payment deduplication.
No running binary or PR feature was changed for this request.

## Final phase-two result and balance reconciliation

- Start 21:34:47.158120; normal STOPPED 22:10:39.689496: **35m52.53s**.
  Early stop at a fresh post-attempt read, with exact executable/profile/PID
  verified. Original observer also saw the latest STOPPED and relinquished.
- Current Run: **4 recorded completions, 0 program saves**. Six completions
  across both phases, whose counters reset independently.
- Rewards: Auto30 130 + Coastal Photo 30 + incidental Auto10 20 + Travel 20
  = **200 BP earned**. Five verified blue rerolls at 10 BP = **50 BP spent**:
  Flying Photo policy (21:35:24), failed Catch Any (22:00:02), Sneak policy
  (22:00:37), Pickup policy (22:00:58), failed Swimming Photo (22:03:56).
- Observed balance **14,100 → 14,250**, exactly **+150 net BP**; approximately
  **251 net BP/hour** for this phase. This is observed yield under its quest mix,
  policies and background builds, not a forecast or an M7 speedup comparison.
- Both phases combined: 14,070 → 14,250 = **+180 BP**, 240 gross / 60 expense.
  Active runtime about 43m59s; wall time including profile transition 47m34s.
  Keep phase-specific rates because settings changed at the phase boundary.
- Catch Fire's second attempt was interrupted at its post-attempt read before
  result processing. No completion or additional reroll payment is claimed;
  ending board still has Make TM / Catch Fire / Savanna Photo.
- M7a: one eligible planner observation reused for scheduling and first
  dispatch, with later reads fresh; two redundant reads avoided on that path.
- M7b: phase-one replacement Travel dispatched before stale queued TM/Catch;
  two additional failed-reroll discard/replan events in phase two.
- Main throughput limits remain Auto30 route/trips (M9), Catch Any/Fire target
  acquisition (M12), and two failed Swimming Photo attempts. Exact swimming
  failure cause remains unproven; capture framing/target evidence next trial.
- Central Plaza confirmed visually. All six party members full HP, lead
  Primarina Lv.100 321/321. Manual game save confirmed by the success dialog
  at 22:11:53; this is separate from the program's zero-save counter.

Local phase-two evidence includes run-final.log, end-board-14250BP.png,
end-party-full-health.png, and save-confirmed.png under the documented
build-prep/m7b-reuse-live-2026-10-21 directory. Native snapshots establish
visible ending balance and saved handoff. No release code changed during run.
