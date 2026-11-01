# Run records

What one run of a program did, as a typed record the framework writes and
the Lab channel turns into files, so an agent that started a checkpoint can
read its outcome, the failing step and attempt, the error, the frame and the
log without guessing from `state=STOPPED` or regexes over the log. This
replaces the `sessions.*` prototype of fork PR #116. The scope review below
is the spec it implements; the sections that followed it (session IDs,
request IDs, evidence IDs, quotas) were dropped with the prototype.

## What is implemented

- **Model** (`RunRecord.h`, `CommonFramework`, no UI, JSON or game
  dependency): `RunRecord{run_id, program, start/end time, status
  running|stopping|terminal, result{outcome completed|cancelled|failed,
  error, frame}, events, log tail}`; `RunEvent{time, kind, checkpoint, step,
  attempt, text, frame, pose}`; the `RunRecorder` interface a program calls
  (`ProgramEnvironment::run_recorder().event({...})`); `RunRecordStore`, the
  last 32 records of this app instance; `SessionRunRecorder`, one session's
  end of it, which is also the `Logger` the session's and its consoles'
  loggers chain through, so the tail holds that run's lines only.
- **Execution boundary** (`ProgramSession`): a run is accepted in
  `start_program()` once `check_validity()` passes, before the program
  thread exists; `run_program_and_record()` closes it with exactly one
  result whichever way `run_program()` ends: normal return and
  `ProgramFinishedException` are `completed`; the stop path is `cancelled`;
  setup, download, connection and execution errors are `failed` with the
  exception's message and, for `FatalProgramException` and
  `OperationFailedExceptionWithScreenshot`, its owned frame. A stop after an
  observed failure does not overwrite it; a normal return racing a stop stays
  completed; a stop before the thread got going, or during a download, is
  cancelled; a dispatch failure closes the record and leaves the session
  stopped. `report_error()` becomes an `error` event, a diagnostic rather
  than the outcome. UI-started runs are recorded like any other.
- **Run IDs**: `<app start time to the microsecond>-<counter>`, opaque,
  sorting in start order, never reused by a restarted app.
- **Bounds**: 32 records; 4096 events per run (more are counted as dropped);
  the frames of a run's newest 8 events; all frames only for the active run
  and the newest 4 terminal runs; a 64 KiB log tail.
- **Lab files** (`Windows/LabRunFiles.h`, written by `LabChannel` on its
  poll): `Lab/runs/<run_id>/result.json`, `events.jsonl`, `log.txt`,
  `frame-*.png`, the newest 32 folders kept. `start` prints `run=<id>`; the
  `state` file has a `run=` line. A `shot` or `detect` taken while a run is
  active, or within two minutes after it ended, is appended to that run's
  `events.jsonl` as a `probe` and its screenshot copied in as `probe-*.png`.
  Shapes and semantics: [Windows/Lab.md, "Run records"](Windows/Lab.md#run-records).
- **Script**: `devtools/lab/labctl.sh wait RUN_ID` polls `result.json` with
  a deadline and prints the outcome, the last event's checkpoint, step and
  attempt, the error and the frame, events and log paths
  ([devtools/lab/README.md](../../../devtools/lab/README.md#run-records)).
- **Tests**: `add_tests_RunRecord()` (the store, the session recorder, and
  the lifecycle through a scripted fake program: rejected start, immediate
  completion, setup failure, failure with a frame, cancellation, late stop,
  dispatch failure, a second run preserving the first);
  `add_tests_LabChannel()` (`result.json` and `events.jsonl` shapes, frame
  names, the writer with probes and pruning, `run=` in the state text);
  `labctl_wait_test.sh`.

Left out on purpose, per the review: the `sessions.*` methods, request IDs
and their deduplication window, evidence IDs and byte quotas, MCP, long-poll
or subscriptions, multi-session, and any game code: `StoryEvents` and the
walkers adopt `run_recorder()` in their own PRs. `completed` still means the
program said so; no story effect is verified here.

## Scope review against the AutoStory runs (2026-11-01)

This section reads the proposal against what the BDSP AutoStory loop actually
spent its time on (runs r1 to r251, `docs/design/AutoStory-Lessons.md`, and the
`console-iteration` skill). It narrows the first slice and moves two pieces.

### What we want out of it

One iteration of the loop is: load the save, start one checkpoint with a known
configuration, wait, and on failure know **which step and attempt failed, with
what error, what the screen showed at that moment, and where the player was**;
then probe by hand with CFR, fix once, rerun. Today the agent gets this from
`run_watch.sh`: it remembers the log's line count, waits for a regex
(`Completed: 00.05`, `attempt N failed`, `Program State: STOPPED|ERROR`), prints
the `[NN.NN step i/n]`, `[Route]` and `[Event]` lines, and lists the newest files
under `Screenshots/AutoStory/failures/`. The verdict is PASS / FAIL / UNKNOWN,
and UNKNOWN happened whenever the program stopped without either line.

So the deliverable is not a new API; it is **one place that says what a run did**:

1. A run identity the agent can keep (`run_id`) and a terminal outcome that is
   never inferred from log text or from `state=STOPPED`.
2. The failure, as a bundle: primary error, the frame the exception carried,
   the step/attempt/event it belongs to, and the last lines of that run's log.
3. Per-checkpoint results inside one run. Run 3 (new game to Floaroma, every
   batched map and route change verified together) is the next real use: its
   value is a list "00.01 ok, 00.02 ok, ..., 01.08 failed at step 4 with ...",
   not one outcome for the whole run.
4. The console left in a state the agent can probe: program stopped, game open
   at the failing screen (memory `stop-and-close-game-on-failure`). CFR then
   does the investigation. No new interactive surface is needed.

### How it would have helped, run by run

| Runs | What cost time | Would the proposal have helped? |
|---|---|---|
| r109 Sandgem `andgemltown` | Finding which screenshot matched the failing OCR; the banner was right, the matcher was narrow. | Yes: error + owned frame in one record. Small gain, the failures folder already had the frame; the gain is correlation, not the frame. |
| r84 Water Gun at 0 PP, four minutes | Nobody noticed repetition until the log was read. | Only with typed progress events the agent can watch; a terminal outcome arrives four minutes late. |
| r105/r106 "pass" after talking to a rock | The program declared completion; the story had not moved. | No. Completion stays program-reported; the fix was a story-effect check in the program. |
| r109a Start click missed | GUI clicks on a resized window. | Already solved by Lab `start`/`stop`/`state` (#107). |
| r178 to r215 Oreburgh 01.03/01.04 | What movement really is: tap counts, wall stops, stillness. Dozens of CFR `walk` probes and screenshots read by hand. | Not by the run API. The evidence that mattered was the walker's own view (blocked taps, tiles counted, stillness) per leg. That is a program event stream. |
| r216 to r251 Route 204 | Map rows wrong (image-traced grids); routes replanned; "where are we" after each leg. | Same: per-leg pose and leg end evidence from `StoryEvents`/the walker, drawn on the grid. Run-level evidence says only "01.08 failed". |
| Every run since r28 (lessons 8) | "Every run that failed in a new way was explained by one screenshot." | Yes, if that screenshot is the one the exception carried and it is named by run, step and attempt, so the agent opens the right one first. |

Two conclusions. First, the frame-plus-error record is worth having but is the
smaller half; the half that paid for itself every day is **structured progress
from inside the program** (checkpoint, step, attempt, leg end, pose, recover
action). `StoryEvents` already produces these as `[Event]` log lines through
`EventLogger`; the walker logs `[Route]` lines. Second, AutoStory retries
inside the program, so a ProgramSession-level record sees one continuing run
and only the last failure. The design notes this; for us it is the common case,
not the edge case, so the progress hook belongs in the first slice, not after
the decision gate.

### What to change

- **Keep:** run identity; exactly one terminal outcome per accepted run
  (`completed` / `cancelled` / `failed`); the primary error with its owned frame;
  a run-tagged log tail; `runs.get` style retrieval that works after the next
  run starts; UI-started runs recorded too; Lab's owner checks unchanged.
- **Add to the first slice:** a typed progress channel the program writes to
  (`run.event(kind, checkpoint, step, attempt, text, optional frame, optional
  pose)`), fed by `StoryEvents::EventLogger` and the walker, stored under the run.
  The Sandgem proof then shows the failing attempt, not only the final error.
  `completed` plus the list of checkpoint events is the run 3 report.
- **Drop from the first slice:** `sessions.*` as a separate method family. A lab
  app runs one program; Lab's `select`/`start`/`stop`/`state` already address it.
  Return `run_id` from `start` and show `run=` in the `state` file instead.
- **Drop:** the request ID with a 128-reply deduplication window and owner
  epochs. Lab's inbox is a serialized queue of named request files; the request
  file name is the request ID, and the journal already answers a resend. One
  local client does not need idempotent retries with expiry semantics.
- **Simplify storage:** Lab is a file protocol. Write each run to
  `Lab/runs/<run_id>/` (`result.json`, `events.jsonl`, `log.txt`, `frame-*.png`)
  and keep the last N directories. This gives durability across app restarts
  and crashes for free, which the in-memory store defers, and it removes the
  opaque evidence IDs, byte quotas and encode queues from the first slice. The
  rule "do not accept arbitrary paths as input" still holds; writing files into
  the lab folder is what the channel already does for `shot`.
- **Keep deferred:** MCP, long-poll or subscriptions, multi-session. The agent
  polls `result.json`; a `labctl.sh wait <run_id>` wrapper hides the polling.
- **Framework boundary:** the typed run model, the execution boundary with
  exactly-once finalization and the owned-frame capture stay in
  `CommonFramework` next to `ProgramSession`, as proposed. Event recording is a
  small interface the program calls (`RunRecorder&` reachable from the
  environment); `StoryEvents` and the walker depend on that interface, not on
  Lab. Lab serializes it to files; a later MCP adapter reads the same files.

### CFR

CFR stays the hand-probing tool and already shares Lab's transport since #107
(`press`, `dpad`, `walk`, `shot`, `detect`, `wait_for`, `expect`, game commands).
Two upgrades make sense, neither of them a new protocol:

- `shot` and `detect` results taken while a run is active, or right after one
  failed, are tagged with the `run_id`, so probing lands in the same run folder.
- A `bdsp_pose` / `frlg_pose` command that reports the program's best estimate
  of the player's tile and facing (from the walker's count and the last
  confirmed anchor), so "where are we" is one command instead of a screenshot
  read against the grid.

The old `CommandFiles/` folder can go once Lab carries every CFR command; that
is cleanup, not part of this design.

### How it will be used

In the `console-iteration` skill's loop, replacing `run_watch.sh`:

```sh
sh labctl.sh send run15 <<'REQ'      # select, set, bdsp_load_save, start
...
REQ
run=$(sh labctl.sh state | sed -n 's/^run=//p')
sh labctl.sh wait "$run"             # polls Lab/runs/$run/result.json, prints it
# outcome=failed  checkpoint=01.08 step=4/7 attempt=2
# error: expect_failed: the Route 204 sign's box did not open
# frame: .../Lab/runs/run15-7/frame-01.08-s4-a2.png
# pose: route_204_south (12, 41) facing N, certainty inexact
# events: .../Lab/runs/run15-7/events.jsonl   log: .../log.txt
```

The agent opens the frame, draws the pose on the grid, probes with CFR in the
same run folder, fixes once, and reruns under a new `run_id`. Run 3 reads
`events.jsonl` for the checkpoint list and the report writes itself. Verdict
UNKNOWN disappears because the outcome comes from the execution boundary.

### Decision gate, restated

Keep slice 4's live demonstration, with one addition: the failing attempt's
event (step, attempt, frame) must be in the record alongside the terminal
error, from the program's own hook, and a CFR `shot` sent after the failure
must land in the same run. If that works on one BDSP checkpoint, run 3 adopts
it; FRLG's Brock MVP uses the same recorder through its own events.
