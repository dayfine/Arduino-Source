# M7b/M7a combined trial, 2026-10-21 PDT

Status: phase one STOPPED at 21:31:12 after its M7b trigger; controlled phase
two RUNNING from 21:34:47. M7a optimized path still awaiting observation.

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
