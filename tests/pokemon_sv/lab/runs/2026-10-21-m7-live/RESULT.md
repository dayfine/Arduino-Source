# M7 live trial, 2026-10-21 PDT

Status: RUNNING. No live acceptance path is verified yet.

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
