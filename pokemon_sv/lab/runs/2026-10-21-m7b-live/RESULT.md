# M7b/M7a combined trial, 2026-10-21 PDT

Status: RUNNING, started 21:23:05.962659 PDT. Neither feature is live-verified yet.

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
