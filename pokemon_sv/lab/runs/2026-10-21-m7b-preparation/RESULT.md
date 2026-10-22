# M7b preparation, 2026-10-21 PDT

Status: CODE/BUILD/NATIVE CHECKS READY. Live M7b trial NOT STARTED.

- Feature/tested integrated revision: ea047a9309ce8c429d288cbfb77f88266f32d34d.
- Parent lab revision: 83fef69c6, containing M7a production 57f330fc9.
- Testing stack bbq/validation-stack fast-forwarded to ea047a930.
- Isolated build checkout: /Users/difan/Projects/Arduino-Source-m7b,
  branch bbq/m7b-replan. This is a lab-derived preparation branch, not a release PR.
- App: build-prep/app/SerialPrograms.app in that checkout.
- Final binary SHA-256: 8626deb7a10585cb3887a5166db95f8ffacef43aa20203c94207f79d5de7814b.
- Full arm64 build PASS. Native C++ registered tests: **58 PASS, 0 FAIL**.
- Red check: all three new regression cases failed against the old production
  loop while the existing 55 passed. Final checks include the adapted old
  zero-completion save case: now three failed partial rounds instead of one.
- Evidence local to this checkout: build-prep/build.log, native-red.log,
  rebuild-final.log, native-final-link.log, native-final.log.

## Resulting behavior

After a failed quest is rerolled, stop dispatching from the old queued batch.
Finish normal round actions (date handling and any crossed save milestone),
then read the board and plan the next batch. Thus a replacement such as Travel
can enter the next plan before old queued Photo/TM work continues.
Only two production-tree files change: farmer implementation and native tests.
No interface, payment verification, Skip/Stop behavior or M7a reuse contract
changes. No observation is cached across the round-end date/save work.
Native cases check replacement order, a full board requiring fresh reads after
round actions, and preserving a save milestone crossed before the failure.
Replanning uses the existing board-order planner; this does not implement ROI
priority or guarantee that Travel always goes first from every slot.

## Running M7a session remains separate

The active PID 660 still uses the original lab app, production 57f330fc9,
binary 9bc7b7b0da2396993615384a815fdb160e79ca53cbfc1ae893a73edbf1626603.
The original executable hash was rechecked after the lab branch fast-forward;
no rebuild or app switch changed it. Source HEAD ea047a930 does not describe
that running executable. Never infer the active revision from source HEAD.

As of 21:01:49 PDT, the user-restarted run had twelve completions and one save,
but **zero reuse logs**. Its only planner-exclusion reroll occurred with three
slots. Four-slot rerolls were on the failed-attempt adapter, which discards the
observation in M7a. This provides general/fallback evidence, not verification
of the optimized path. Further unstructured running has limited M7a value.

## Next-session preflight and acceptance

1. Stop the current run normally at a safe boundary; record final board, BP,
   party and log end. Preserve both original and changed-settings run records.
2. Verify Central Plaza, correct lead and full capture feed; open the exact M7b
   app above with an isolated profile. Recheck resource/stats paths and actual
   loaded UI values. A new binary may request camera/microphone approval again.
3. Local settings candidate: build-prep/m7b-preparation/settings-candidate.json.
   It is STAGED ONLY, not installed or loaded. It uses one retry / Reroll,
   save interval 10, zero eggs / Keep Going, and valid explicit Sneak Up / Reroll
   plus Pickup 10 / Reroll rows. Snapshot alone does not prove active settings;
   remap isolated stats/profile paths before loading. No hatch exclusion row.
4. M7a path: start a separately recorded phase with FOUR recognized quests and
   one visible blue quest configured Reroll. Choose the actual visible quest
   at preflight if Sneak Up is absent. Verify one paid target replacement,
   scheduling reuse, first-dispatch reuse, then fresh reads for later dispatches.
   If no complete four-slot reroll occurs, M7a stays INCONCLUSIVE.
5. M7b path: keep a known failing route runnable (e.g. Catch Any), so exhaustion
   triggers Reroll. Require the new discard-batch log, normal round ending and
   a new scheduling pass before any remaining old queued dispatch. Check that
   the replacement enters the plan and that completions/save milestones remain
   correct. Capture the before/after board and dispatch timing.
6. Pin settings per phase and record each boundary. Use 60-minute timed runs,
   extending only for a specific unmet trigger. Re-arm an exact run-identity
   observer and verify it survives; the former deadline stop remains unproven.

No new live result or feature PR is claimed. Extract a dedicated code-only fork
PR after evaluated live evidence, with M7a/M6 dependencies explicit; never open
it from the accumulating lab branch. All notes stay on agents.

## Handoff completed at 21:08 PDT

Previous program is STOPPED; game saved and idle at Central Plaza. Final balance
14,070 BP, Primarina Lv.100 321/321 leading. Three blue quests: Savanna Photo,
Make TM, Catch Any. Old M7a app remains open, so the next step is closing it and
opening this exact M7b build/profile; no M7b trial has started.
