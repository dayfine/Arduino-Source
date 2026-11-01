# Lab run records

Design for fork PR #116, 2026-11-01. "Today" means the code on #116 at
`5ffec9d1b` or on its base #107 (`lab/tooling`); anything not built yet is
marked **proposed**. Paths are relative to `SerialPrograms/Source/` unless
they start with `devtools/` or `docs/`.

## 1. Requirement

When a lab run of a program fails, the person or agent who started it has to
reconstruct what happened from a shared, app-wide `SerialPrograms.log`
filtered by time, a `shot` taken through the outbox after the fact, and
memory. Three things get lost: the screen at the moment an attempt failed (a
program such as AutoStory retries internally, and its final exception carries
no frame: `PokemonFRLG_AutoStory.cpp` on #120 throws
`OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, ...)`), which build
and options the run used, and a report that does not have to be assembled by
hand into notes such as `~/FRLGLab/notes/newgame-firered-console-B.md`. A
run record is one folder per run that answers these on its own.

Users: **agents** driving console B, who need a machine-readable outcome and
the failing step without regexes over the log; **the user**, who reads the
failure frame and the summary when a run needs a decision; **reviewers** of a
fork PR, who get a run's summary as evidence instead of a paraphrase.

## 2. Scope

In scope:

- One record per accepted start, UI-started runs included, with exactly one
  outcome (`completed`, `cancelled`, `failed`).
- The run's own log lines, the frames of failed attempts, and the probes
  (`shot`, `detect`) taken during or right after the run.
- Build and option metadata (**proposed**).
- Structured progress events from programs: checkpoints, steps, attempts,
  route plans and legs (**proposed** hooks; the API exists).
- A script to list runs, show one, and write a Markdown report (**proposed**).

Not in scope:

- **Uploading** anywhere. Records stay in the profile; anything worth keeping
  is a report committed to the `agents` notes branch by hand. Uploading
  raises privacy questions this lab does not need answered.
- **A UI.** The readers are scripts and people with a file browser.
- **Remote consoles or several sessions per app.** Lab runs one app per
  console on this Mac; `lablock.sh` and the owner token already serialize it.
- **Automatic video.** Lab's `history` command already saves a clip on
  request; an always-on recording per run costs disk for little gain.
- **Long-term retention.** The newest 32 folders stay (today); there is no
  archive. Reports carry what matters past that.
- MCP, long-poll, request IDs: dropped with the `sessions.*` prototype
  (`CommonFramework/RunRecords.md`, "Scope review").
- Verifying story effects: `completed` means the program said so.

## 3. High-level design

### How it is used

Today, with #107 and #116:

```sh
export LAB_PROFILE="$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B"
export LAB_OWNER=lab-B LAB_CONSOLE=B
sh devtools/lab/lablock.sh run frlg-0008 -- sh -c '
  sh devtools/lab/labctl.sh send r20 <<EOF
select PokemonFRLG:AutoStory
set START_CHECKPOINT 8
set END_CHECKPOINT 8
set NEW_GAME_FROM_TITLE false
start
EOF
  sh devtools/lab/labctl.sh wait "$(sh devtools/lab/labctl.sh state | sed -n s/^run=//p)"'
```

`labctl.sh wait` polls `Lab/runs/<run_id>/result.json` and prints the
outcome, the last event's checkpoint, step and attempt, the error, the frame
and the paths of `events.jsonl` and `log.txt` (`devtools/lab/labctl.sh`).
**Proposed:** `devtools/lab/runs.py list | show RUN | report RUN` for
everything after that.

### Example journey

An agent starts FRLG AutoStory at 00.08 (Viridian Forest's entrance to Pewter
Center) on console B, as above. In attempt 1 the walker fights five wild
battles in the forest; on the north gate's mat the battle detector fires on
the dark room and the program presses A/B for ten minutes, gives up, reloads
and fails again (run r15 in the FRLG notes, when the forest was part of 00.07).

- **Today**, `wait` prints `outcome=failed` and the error
  `AutoStory stopped at 00.08 <event>: ...`; `frame:` is missing because
  the exception carried none, and no event names the attempt, because the
  game code does not call `run_recorder()` yet. `log.txt` holds only the last
  64 KiB, so the first attempt's route lines are gone. The agent still greps
  `SerialPrograms.log`.
- **With the proposed hooks**, `events.jsonl` has `checkpoint 00.08 start`,
  a `step` per story event, `route_plan` and `leg` lines with poses, five
  `battle` lines, and an `attempt` line for each failed attempt with
  `frame-<n>-00.08-s<step>-a1.png`, the screen on the gate's mat. `wait` prints
  the second attempt and its frame. The agent opens the frame, runs
  `python3 -m devtools.maps routes Lab/runs/<run_id> ...` (#125) to see the
  walk end on the mat, sends a `shot` (filed into the same folder as a probe),
  and writes `runs.py report <run_id>` into the notes. The user reads that
  report and the frame and decides; the fix goes in with the report as its
  evidence.

### What a record contains and when it is written

The store lives in memory in every app (`CommonFramework/RunRecord.h`). The
Lab channel copies it to files on its 500 ms poll, only in an app whose
profile has a `Lab/` folder (`Windows/LabChannel.cpp`,
`Windows/LabRunFiles.h`). Layout of `Lab/runs/<run_id>/`:

| File | Content | Written |
|---|---|---|
| `result.json` | run ID, program, status, times, outcome, error, frame name, last event | at start; rewritten as status or last event change; final once `status` is `terminal` |
| `events.jsonl` | one JSON object per event: `time`, `kind`, and as available `checkpoint`, `step`, `attempt`, `text`, `pose`, `frame`; probes add `output` | appended on each poll after an event |
| `frame-*.png` | `frame-failure.png` / `frame-end.png` (the exception's frame), `frame-<n>[-<cp>][-s<step>][-a<attempt>].png` (event frames) | with the event or the end |
| `probe-*.png` | copies of `shot`s within the run or 120 s after it | when the probe runs |
| `log.txt` | the run's own log lines (program and its consoles), last 64 KiB | at the end only |
| `meta.json` | **proposed**: app version, executable path, the program's options JSON, console | at start |

**Proposed** change to `log.txt`: append it on each poll, up to 8 MiB, so a
crash keeps the log and a long AutoStory run keeps its first attempt.

Shapes: `Windows/Lab.md`, "Run records". Run IDs are
`<app start time to the microsecond>-<counter>`: they sort in start order
and are never reused (`RunRecord.cpp`).

### Relation to the log, ErrorReports and DebugDumps

- `SerialPrograms.log` stays the app-wide log. `log.txt` is the subset that
  passed through this session's `SessionRunRecorder`, which the session's and
  its consoles' loggers chain off (`ProgramSession.h`,
  `GameConsole/Framework/ConsoleSystemSession.h`); lines logged to the global
  logger from elsewhere are not in it.
- `ErrorReportsLocal/<timestamp>/` (`ErrorReports/ErrorReports.h`) is the
  upstream report a user may send: screenshot, log, video. It is unchanged.
  Programs that pass `NO_ERROR_REPORT`, as AutoStory does, write none; the
  run record is what the lab reads instead. **Proposed:** an `error` event
  names the report folder when one was written.
- `DebugDumps/` (`Tools/DebugDumper.h`) are detector images dumped for
  development, by label, not by run. Unchanged; a program that wants such an
  image in the record sends it as an event frame.

## 4. Major components

**RunRecordStore and SessionRunRecorder** (today, `CommonFramework/RunRecord.h`).
The typed model (`RunRecord`, `RunEvent`, `RunResult`) and a thread-safe
in-memory store of the last 32 runs; every call is short and copies, so the
writer never holds the lock while writing files. Limits: 4096 events per run
(more are counted in `dropped_events`), frames kept for a run's newest 8
events and only for the active run and the newest 4 finished ones, a 64 KiB
log tail. `SessionRunRecorder` is both the `RunRecorder` a program calls and
the `Logger` that copies the session's lines into the tail. No UI, JSON or
game dependency.

**Execution boundary** (today, `CommonFramework/ProgramSession.cpp`). A run
opens in `start_program()` once `check_validity()` passes;
`run_program_and_record()` closes it with exactly one result: a normal return
or `ProgramFinishedException` is `completed`, the stop path is `cancelled`,
any other error is `failed` with the exception's message and, for
`FatalProgramException` and `OperationFailedExceptionWithScreenshot`, its
frame. `report_error()` becomes an `error` event. Tested through a scripted
fake program in `RunRecord_Tests.cpp`.

**Program hook** (API today in `Tools/ProgramEnvironment.h`; callers
**proposed**). `env.run_recorder().event({.kind = "attempt", .checkpoint =
..., .step = ..., .attempt = ..., .text = ..., .frame = ..., .pose = ...})`.
Nothing in game code calls it yet. Proposed callers:
`Pokemon::SegmentHooks` (`Pokemon/Navigation/Pokemon_StorySegments.h`, owned
by #114) gains an `event` callback beside `log`, with a nav-local struct so
the shared code does not depend on `CommonFramework`; each game's AutoStory
program (FRLG #120, BDSP #128) forwards it to `run_recorder()` and attaches a
fresh frame to failed attempts. The shared route log
(`Pokemon/Navigation/Pokemon_RouteLog.h`, #114) emits `route_plan` and `leg`
events through the same callback.

**Event schema.** `kind` and `time` always; the rest optional. Kinds:

| Kind | Sent by | Fields used |
|---|---|---|
| `error` | today: `ProgramSession::report_error()` | `text` |
| `probe` | today: Lab, for `shot`/`detect` | `text` (command), `output`, `frame` |
| `checkpoint` | proposed: story segment | `checkpoint`, `text` (`start`, `ok`, `failed`) |
| `step` | proposed: story segment | `checkpoint`, `step` (`n/total name`), `text` |
| `attempt` | proposed: story segment, on a failed attempt | `checkpoint`, `step`, `attempt`, `text`, `frame`, `pose` |
| `route_plan` | proposed: route log | `text` (the legs), `pose` (start) |
| `leg` | proposed: route log | `text` (how it ended: walked, assumed, unknown), `pose` |
| `battle` | proposed: walkers | `text` (wild, trainer, result), `pose` |
| `screenshot` | proposed: any program, for a frame worth keeping | `text`, `frame` |

`pose` stays a string, `"<map> (x, y) facing D[, certainty]"`, the form
`Lab.md`'s examples use. Today #125's `route_trace.py` reads route lines from
a log, not poses. **Proposed:** it reads the `route_plan` and `leg` events of
`events.jsonl` first and falls back to `log.txt`.

**LabRunWriter** (today, `Windows/LabRunFiles.h`). Called from the Lab
channel's poll; writes what the store has that the files do not, attaches
probes, prunes to the newest 32 folders. It runs between requests, so a long
`cfrun.sh` batch delays it; with 8 frames per run held in memory, a burst of
more than 8 framed events during such a batch loses the older frames. Fine
for one train per console; noted, not fixed. **Proposed:** `meta.json` and the
streamed `log.txt`.

**Run index and CLI.** No index file: folder names sort by start time and
`result.json` has the summary. `labctl.sh wait` exists. **Proposed:**
`devtools/lab/runs.py` (standard library only, like `devtools/maps`):
`list [-n N]` (ID, program, outcome, last checkpoint, duration), `show RUN`
(result plus the last events), `report RUN` (Markdown: outcome, build,
options, checkpoint table, failed attempts with frame links, probe list),
for notes and PR evidence.

## 5. Milestones

Splitting #116 is the main decision. Its framework half (`RunRecord.*`,
`ProgramSession`, `ProgramEnvironment`, the logger chain, about 570 lines
plus tests) does not depend on Lab, but #116 sits on #107, which only lab
builds use. The AutoStory programs on #120 and #128 need `run_recorder()`
and must not pull in lab tooling. So:

| # | PR | Base / depends on | Scope | Size (excl. tests) |
|---|---|---|---|---|
| 1 | new `common/run-records` | `main` | framework half of #116, unchanged, plus `meta` fields (version, executable, options JSON) in `RunRecord` | ~630 |
| 2 | #116 (kept) | #107, merges 1 | Lab half: `LabRunFiles`, `LabChannel`, `Lab.md`, README, `labctl.sh wait`; adds `meta.json` and the streamed `log.txt` | ~600 |
| 3 | new `lab/runs-cli` | #116 | `devtools/lab/runs.py` list/show/report | ~250 |
| 4 | commits on #114, #120, #128 | 1 | `SegmentHooks.event`, route log events, per-game forwarding with attempt frames | ~80 shared, ~40 per game |
| 5 | commit on #125 | 4 | `route_trace.py` reads `events.jsonl` | ~80 |

1 and 2 can be reviewed in parallel; 3 needs only 2; 4 needs only 1; 5
needs 4 and #125. Nothing here opens an upstream PR; whether 1 ever goes
upstream is the user's call.

**What #116 becomes:** the Lab half (row 2). Its code is kept; the framework
files move to PR 1 (split, not rewritten), and `CommonFramework/RunRecords.md`
is folded into this document. Next step, not done in this commit: create PR 1
from `main` with the framework files, merge it into #116, retarget #116's base
so its diff is the Lab half only.

**Testing.** Unit: `add_tests_RunRecord()` (store, recorder, lifecycle
through a fake program) and `add_tests_LabChannel()` (file shapes, writer,
probes, pruning) exist; PR 1 adds meta, PR 2 the streamed log;
`labctl_wait_test.sh` exists; `runs.py` gets `unittest` cases that build a
record in a temporary folder. CI runs the native tests (dispatched on the
fork, drafts skip it). Lab smoke test after PR 2: start and stop a program on
console B and check that `wait` prints `cancelled` and that the folder has
`meta.json` and `log.txt`. AutoStory proof after PR 4: one FRLG 00.08 run and
one BDSP checkpoint whose failed attempt shows in `events.jsonl` with its
frame and pose, a `shot` after the failure filed in the same folder, and #125
drawing the walk from `events.jsonl` alone. If that holds, the console-B runs
adopt `runs.py report` in the notes.

## 6. Open questions

1. Split #116 into a framework PR on `main` (recommended), or keep it whole
   and let the game branches wait?
2. `log.txt`: stream with an 8 MiB cap, or keep the 64 KiB tail?
3. Should the in-memory store run in every app, as today, or only when the
   `Lab/` folder exists?
4. Build identity: version plus executable path (lab builds sit in named
   folders), or bake the git commit into the build (a CMake change)?
5. Retention: are 32 folders enough once attempts carry frames?
