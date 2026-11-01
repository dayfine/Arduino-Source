# Lab run records

Design for fork PRs #132 (framework) and #116 (Lab files), 2026-11-01.
"Today" means the code on those two branches; anything not built yet is
marked **proposed**. Paths are relative to `SerialPrograms/Source/` unless
they start with `devtools/`, `docs/` or `SerialPrograms/`. The decisions the
user took on the first draft are listed at the end.

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
  outcome (`completed`, `cancelled`, `failed`), in an app whose profile has a
  `Lab/` folder.
- The run's own log lines, the frames of failed attempts, and the probes
  (`shot`, `detect`) taken during or right after the run.
- The build (version and git commit) and the program's settings.
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
- **Long-term retention.** The newest 32 folders stay; there is no archive.
  Reports carry what matters past that.
- **What the `sessions.*` prototype had** (the first version of #116): a
  separate `sessions.*` method family, request IDs with a 128-reply
  deduplication window and owner epochs, evidence IDs with byte quotas, MCP,
  long-poll or subscriptions. A lab app runs one program, and Lab's `select`,
  `start`, `stop` and `state` already address it; the inbox is a serialized
  queue whose file names are the request IDs, and its journal answers a
  resend. Files under `Lab/runs/` survive an app restart for free, which the
  prototype's in-memory store did not. A later MCP adapter can read the same
  files.
- Verifying story effects: `completed` means the program said so.

## 3. High-level design

### How it is used

Today, with #107, #132 and #116:

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
and the paths of `events.jsonl` and `log.txt` (`devtools/lab/labctl.sh`). It
replaces `run_watch.sh`'s log regexes in the `console-iteration` loop.
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
  game code does not call `run_recorder()` yet. `log.txt` has the whole run,
  both attempts' route lines included, and `meta.json` names the commit and
  the settings. The agent reads `log.txt` instead of filtering
  `SerialPrograms.log` by date, but still finds the attempt by hand.
- **With the proposed hooks**, `events.jsonl` has `checkpoint 00.08 start`,
  a `step` per story event, `route_plan` and `leg` lines with poses, five
  `battle` lines, and an `attempt` line for each failed attempt with
  `frame-<n>-00.08-s<step>-a1.png`, the screen on the gate's mat. `wait`
  prints the second attempt and its frame. The agent opens the frame, runs
  `python3 -m devtools.maps routes Lab/runs/<run_id> ...` (#125) to see the
  walk end on the mat, sends a `shot` (filed into the same folder as a probe),
  and writes `runs.py report <run_id>` into the notes. The user reads that
  report and the frame and decides; the fix goes in with the report as its
  evidence.

### What a record contains and when it is written

The store lives in memory (`CommonFramework/RunRecord.h`, #132) and records
nothing until it is enabled. The Lab channel enables it when the app finds
the profile's `Lab/` folder at startup, and copies it to files on its
500 ms poll (`Windows/LabChannel.cpp`, `Windows/LabRunFiles.h`, #116). Layout
of `Lab/runs/<run_id>/`:

| File | Content | Written |
|---|---|---|
| `meta.json` | run ID, program, `version`, `commit` (git describe, `-dirty` with local changes), `settings` (the program's settings JSON) | at start |
| `result.json` | status, times, outcome, error, frame name, last event, event and log line counts | at start; rewritten as status or last event change; final once `status` is `terminal` |
| `events.jsonl` | one JSON object per event: `time`, `kind`, and as available `checkpoint`, `step`, `attempt`, `text`, `pose`, `frame`; probes add `output` | appended on each poll after an event |
| `log.txt` | the run's own log lines (program and its consoles) | appended on each poll, up to 8 MiB; a last line counts the lines past that |
| `frame-*.png` | `frame-failure.png` / `frame-end.png` (the exception's frame), `frame-<n>[-<cp>][-s<step>][-a<attempt>].png` (event frames) | with the event or the end |
| `probe-*.png` | copies of `shot`s within the run or 120 s after it | when the probe runs |

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

**RunRecordStore and SessionRunRecorder** (today, #132,
`CommonFramework/RunRecord.h`). The typed model (`RunRecord`, `RunEvent`,
`RunResult`) and a thread-safe in-memory store of the last 32 runs; every
call is short and copies, so the writer never holds the lock while writing
files. The app's store starts disabled (`set_enabled()`); disabled,
`begin()` returns `""` and nothing is kept. Limits: 4096 events per run (more
are counted in `dropped_events`), frames kept for a run's newest 8 events and
only for the active run and the newest 4 finished ones, log lines numbered
with a 256 KiB tail in memory and at most 8 MiB per run (more are counted in
`dropped_log_lines`). Each record carries `version` (`PROGRAM_VERSION`),
`commit` (`PROGRAM_COMMIT`) and `settings`. `SessionRunRecorder` is both the
`RunRecorder` a program calls and the `Logger` that copies the session's
lines into the record. No UI, JSON or game dependency.

**Build commit** (today, #132, `CommonFramework/BuildCommit.h`,
`SerialPrograms/cmake/BuildCommit.cmake`). A custom target runs
`git describe --always --dirty --abbrev=12` on every build and rewrites
`<build>/generated/BuildCommit.cpp` only when the text changes, so a new
commit recompiles one file. Without git the commit is `unknown`.

**Execution boundary** (today, #132, `CommonFramework/ProgramSession.cpp`). A
run opens in `start_program()` once `check_validity()` passes, with the
session's `settings_json()` (the console sessions return their settings-file
JSON); `run_program_and_record()` closes it with exactly one result: a normal
return or `ProgramFinishedException` is `completed`, the stop path is
`cancelled`, any other error is `failed` with the exception's message and,
for `FatalProgramException` and `OperationFailedExceptionWithScreenshot`, its
frame. A stop after an observed failure does not overwrite it; a dispatch
failure closes the record and leaves the session stopped. `report_error()`
becomes an `error` event. Tested through a scripted fake program in
`RunRecord_Tests.cpp`.

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

**LabRunWriter** (today, #116, `Windows/LabRunFiles.h`). Called from the Lab
channel's poll; writes `meta.json` when a run first appears, appends the
events and log lines the files don't have yet, attaches probes, prunes to
the newest 32 folders. It runs between requests, so a long `cfrun.sh` batch
delays it: log lines that leave the store's 256 KiB tail before the next
poll are replaced in `log.txt` by a line saying how many were missed, and
with 8 frames per run held in memory, a burst of more than 8 framed events
loses the older frames. Fine for one train per console; noted, not fixed.

**CFR probes** (today, #116, and **proposed**). CFR stays the hand-probing
tool and shares Lab's transport since #107. A `shot` or `detect` taken while
a run is active, or within two minutes after, already lands in that run's
folder as a probe. **Proposed:** a `bdsp_pose` / `frlg_pose` command that
reports the program's best estimate of the player's tile and facing, so
"where are we" is one command instead of a screenshot read against the grid.
The old `CommandFiles/` folder can go once Lab carries every CFR command;
that is cleanup, not part of this design.

**Run index and CLI.** No index file: folder names sort by start time and
`result.json` has the summary. `labctl.sh wait` exists. **Proposed:**
`devtools/lab/runs.py` (standard library only, like `devtools/maps`):
`list [-n N]` (ID, program, outcome, last checkpoint, duration), `show RUN`
(result plus the last events), `report RUN` (Markdown: outcome, commit,
settings, checkpoint table, failed attempts with frame links, probe list),
for notes and PR evidence.

## 5. Milestones

The framework half does not depend on Lab, and the AutoStory programs on
#120 and #128 need `run_recorder()` without pulling in the lab-only #107, so
the first slice is two PRs:

| # | PR | Base / depends on | Scope | Size (excl. tests) |
|---|---|---|---|---|
| 1 | #132 `common/run-records` (done) | `main` | the framework half of the old #116, unchanged in its first commit; then the build commit, settings, the 8 MiB numbered log and the disabled-by-default store | ~700 |
| 2 | #116 (done) | `lab/run-records-base` (#107 + `main` + #132) | the Lab half: `LabRunFiles`, `LabChannel` enabling the store, `meta.json`, `log.txt` appended as it goes, `Lab.md`, README, `labctl.sh wait` | ~600 |
| 3 | new `lab/runs-cli` | #116 | `devtools/lab/runs.py` list/show/report | ~250 |
| 4 | commits on #114, #120, #128 | #132 | `SegmentHooks.event`, route log events, per-game forwarding with attempt frames | ~80 shared, ~40 per game |
| 5 | commit on #125 | 4 | `route_trace.py` reads `events.jsonl` | ~80 |

1 and 2 are reviewed together; 3 needs only 2; 4 needs only 1; 5 needs 4
and #125. `lab/run-records-base` is `lab/tooling` with `main` and #132
merged in (the four files where `main` and the lab channel disagree are
taken from `lab/main`); it is re-merged when either moves. Nothing here
opens an upstream PR; whether #132 ever goes upstream is the user's call.

**Testing.** Unit: `add_tests_RunRecord()` (store, disabled store, build and
settings, the numbered log and its cap, session recorder, lifecycle through a
fake program) on #132; `add_tests_LabChannel()` (file shapes, `meta.json`,
the writer with streamed log, probes and pruning) and `labctl_wait_test.sh`
on #116; `runs.py` gets `unittest` cases that build a record in a temporary
folder. CI runs the native tests (dispatched on the fork, drafts skip it).
Lab smoke test after #116: start and stop a program on console B and check
that `wait` prints `cancelled` and that the folder has `meta.json` with the
lab build's commit and a growing `log.txt`. AutoStory proof after step 4
(the decision gate from the prototype's review, kept): one FRLG 00.08 run and
one BDSP checkpoint whose failed attempt shows in `events.jsonl` with its
step, attempt, frame and pose alongside the terminal error, a `shot` after
the failure filed in the same folder, and #125 drawing the walk from
`events.jsonl` alone. If that holds, the console-B runs adopt
`runs.py report` in the notes.

## 6. Decisions (2026-11-01)

1. #116 is split: the framework half is #132 on `main`; #116 keeps the Lab
   half.
2. `log.txt` is appended as the run goes, capped at 8 MiB.
3. Runs are recorded only in an app whose profile has a `Lab/` folder.
4. The build carries its git commit (`cmake/BuildCommit.cmake`).
5. 32 run folders are kept.
