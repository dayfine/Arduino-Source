# M7 live trial, 2026-10-21 PDT

Status: first run STOPPED manually at 19:49:03; M7 reuse INCONCLUSIVE.
User-restarted run from 19:49:04 STOPPED at 21:05:50 with changed policies.
Final balance 14,070 BP; saved at Central Plaza; M7a reuse INCONCLUSIVE.

- Started: 18:49:05 PDT; planned stop: 19:49:05 PDT.
- Tested production SHA: 57f330fc9 (integration base 8aa1821b4).
- Binary SHA-256: 9bc7b7b0da2396993615384a815fdb160e79ca53cbfc1ae893a73edbf1626603.
- Full arm64 app build PASS; 55 registered native C++ tests PASS, zero failures.
- App PID 660, isolated profile bbq-lab; upstream v0.71.3-user.
- Starting BP: **13,690**, directly visible in the native 1920x1080 snapshot.
- Starting location: Central Plaza, manually returned by user before handoff.
- Lead: full-health Lv.100 Primarina, 321/321; remaining five party members full HP.
- Starting board: Catch Any 20 BP, Sneak Up 20 BP, Savanna Photo 30 BP;
  three blue slots, no red quest. All three share a known problematic route.
- Settings: 1000 quest capacity; one retry; exhausted quests Reroll; save every
  10 completions; zero eggs/Keep Going; Hatch Skip; existing exclusions retained;
  notifications disabled. Snapshot stored locally before Start.
- Startup confirmed RUNNING, overworld detected, all three quests recognized,
  and first Catch Any dispatched toward Savanna Plaza.

## Observer and acceptance limits

Local observer records log checkpoints every five minutes and requests normal
Stop after one hour, verifying STOPPED in the log. It checks the exact app PID,
path and profile before Stop. It uses a bounded, shallow AX lookup; Screenshot
and Start were successfully exercised. Deadline Stop itself is not yet proven.
The earlier recursive AX stop helper missed the Flying Trial handoff; user
stopped and returned to Plaza manually. Do not reuse that helper as evidence.

M7 requires a fully recognized four-slot replacement board to reuse it. The
starting three-slot board exercises conservative fresh reads, not reuse. A run
with no eligible reuse trigger remains INCONCLUSIVE for live acceptance.
Check reuse scheduling/first-dispatch logs, subsequent fresh reads, completions,
rerolls, BP income/expense and existing route/navigation failures.

## Local evidence

Directory: build-prep/m7-live-2026-10-21/
- start-board-13690BP.png: native full capture, including BP.
- settings-at-start.json: local profile snapshot.
- observe.py: run orchestration only, not a Python test.
- status.json, checkpoints.jsonl and run.log: observer snapshots.
- Interactive observer process initially uses exec session 59162; verify liveness
  before relying on it. A prior detached nohup attempt exited and was replaced.

No release PR has been created. Ending BP, duration, trigger coverage and final
classification must be added after the run stops. No production rebuild or
code changes are allowed to alter this running binary during the trial.

## First-hour result, audited after the quota pause

The user stopped at 19:49:03.408 and restarted at 19:49:04.126 after clearing
some exclusions; they may have made other changes. Treat the restart as a new
configuration boundary, not a continuation of the controlled first run.
Duration: 59m58s. Six completions / 15 dispatches, nine exhausted dispatches.
All **75 observed boards had three recognized slots**, so reuse scheduling and
first-dispatch counters were both zero. Conservative fresh reads were exercised;
the optimized four-slot path was not. **No verified M7 PR claim is justified.**

Read-purpose counts: 11 scheduling, 15 pre-dispatch, 26 after attempt and 14
reroll verification. Remaining nine reads are transaction validation. OCR-only
intervals (Reading quests → closing B) totaled 6.39 minutes, mean 5.11 seconds;
these overlap dispatch times and must not be added to them.

| Completed quest | Time | Gross BP |
| --- | --- | ---: |
| Savanna Photo | 19:02:28 | 30 |
| Travel 500 | 19:09:28 | 20 |
| Canyon Photo | 19:11:13 | 30 |
| Catch Fighting | 19:18:43 | 40 |
| Swimming Photo | 19:20:23 | 30 |
| Swimming Photo | 19:34:57 | 30 |

Gross estimate 180 BP. Thirteen rerolls logged completion (130 BP); a fourteenth
paid reroll changed Tera Self Defeat to Catch Any before manual Stop, but was
interrupted before its final rerolled message. Estimated expense 140 BP, net
40 BP (~40 BP/hour); inferred ending BP 13,730. No screenshot was captured at
that boundary, so this is ledger inference, not a directly observed end balance.

### Configuration error found and acknowledged

Codex inserted hatch-egg/skip into the settings JSON to disable hatching.
BBQuests_database() explicitly excludes hatch-egg from the policy table.
IntegerEnumDropdownCell::load_json() ignores an unknown slug, retaining the
row's default auto_10. Thus that row became **Auto Battle 10 / Skip**. The quest
occupied slot one from 18:54:46 through the end and was skipped on ten planning
passes, never attempted. This is a preflight configuration error, not an M7
production regression. The user clearing exclusions removed the blocker; the
new run attempted Auto Battle immediately. NUM_EGGS=0 with the existing
OUT_OF_EGGS policy already gates hatching; do not add an unsupported table row.
The starting JSON snapshot is retained unchanged as evidence of the error.
The on-disk profile still contains the stale row because current UI edits have
not yet been saved; do not reload it over the user's active settings.

### Additional evidence and operational corrections

- Correct-lead Catch Any: 3 exhausted dispatches, 6 attempts, 14.45 minutes.
- Correct-lead Sneak Up: 4 exhausted dispatches, 8 attempts, 14.30 minutes.
- All 14 attempts logged no battle entry. Combined 28.75 minutes (~48% of run).
  This strengthens M12 even after the party was repaired.
- Savanna Photo completed once after a retry (3.20 min), then exhausted once
  (3.21 min); target/framing issues are intermittent, not universal absence.
- Catch Fighting succeeded after a retry in 7.40 minutes; its first attempt had
  11 unable-to-detect-battle-slot messages. Preserve for later diagnosis.
- Observer state detection incorrectly treated any historical STOPPED as the
  current state. At the deadline it exited without requesting Stop after the
  manual stop/restart. Deadline stopping is therefore still NOT VERIFIED.
  Local helper corrected to inspect the latest state and relinquish ownership
  when a new run begins; retain observe-original.py. No newer run was stopped.
- Quota pause: no sampling work from 19:00:24 to 20:03 PDT; log observer ran.

## User-restarted run, from 19:49:04 PDT

Same binary, changed policy settings. User confirmed clearing exclusions but
cannot recall all other edits. Active UI differs from the original disk JSON;
exact changed rows remain unexported, so avoid an exact configuration claim.
At 20:04:32 it had three completions: Auto Battle 10, Travel 500 and Make TM.
At 20:08:10 a native screenshot shows **13,770 BP**, red Salty Sandwich plus
Travel 500, Sneak Up and Swimming Photo. Combined directly observed gain from
18:48:33 is +80 BP. A ledger of 240 gross and 160 reroll expense across both runs
matches this balance; the first-run boundary remains inferred.

New four-slot boards make reuse eligible in principle, but no reuse log had
occurred as of the 20:10 checkpoint. A read-only five-minute observer (session
76940) now records restarted-status.json/restarted-checkpoints.jsonl and the
new run log. It sends no Stop or game input and exits on Stop/new restart or
at the two-hour logging horizon. This is not a new stop timer. User retains
control of the restarted run. About 3,952 discontinuous-audio timestamp errors
were logged in the early restarted run; causality is not established.

Local evidence: first-hour.log, first-hour-events.txt, dispatches.json,
active-settings-details.txt, and screenshot-20261021-200810053717.png under the
profile's Screenshots directory. No binary rebuild or release PR was made.

## Final stop and saved handoff, 2026-10-21 21:08 PDT

User authorized stopping the current program, returning to Central Plaza and
saving for the next M7b session. The identity-checked shallow AX helper pressed
Stop at the next after-attempt panel boundary. STOPPED was verified at
21:05:50.136589. Restarted-run duration: 76m46s. Last recorded stats: twelve
completions and one program save; the final Catch Any attempt was interrupted
before its completion check and Catch Any remains on the final board.
No M7a reuse event occurred. No M7b executable was started.

The program had already returned to Central Plaza before that boundary; a
manual flight was unnecessary. Native final board snapshot at 21:06:05 shows
**14,070 BP**, Savanna Photo, Make TM and Catch Any, all incomplete, no red.
This is a directly observed **+380 BP** from the original 13,690 starting
snapshot across both runs and the manual restart boundary. Do not attribute
this whole balance change to either run separately without the inferred ledger.

Party snapshot: Primarina Lv.100 321/321 still leads; all six full HP. Iron Hands
Lv.100, Corviknight Lv.77, Chien-Pao Lv.99, Magmar Lv.71 and Golett Lv.67.
Manual save performed through main menu R, confirmed Yes. The 21:07:31 snapshot
shows the explicit save-completed dialog. Menus then closed; the 21:08:14
snapshot verifies overworld at Central Plaza. Controller is idle.

Local evidence copied to lab build-prep/m7-live-2026-10-21/:
- final-board-14070BP.png
- final-party.png
- final-save-confirmed.png
- final-central-plaza.png
- restarted-final.log (scoped to the second run before STOPPED).

The app left open is still the old M7a lab app, stopped. **Do not press Start
expecting M7b**: switch to the built Arduino-Source-m7b app and validate its
isolated profile before the next trial. See the M7b preparation record.
