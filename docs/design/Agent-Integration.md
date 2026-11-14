# Agent integration: an MCP lab server

Status: proposal (2026-11-01; rewritten 2026-11-13 from the week's incidents,
`~/BDSPLab/notes/2026-11-13-week-problems.md`). It builds on:
- the lab channel: fork #107, `SerialPrograms/Source/CommonFramework/Windows/Lab.md`
  (`Lab/inbox`, `Lab/outbox`, `Lab/state`, the owner token);
- run records: fork #116, `docs/design/Lab-RunRecords.md`;
- the lab scripts on `agents` (`lab/control`, `lab/scripts`) and in `~/BDSPLab`
  (`labrun.sh`, `labbuild.sh`, `checkin.sh`, `lablock.sh`, `heavy_gate.sh`, ...).

Nothing here is implemented except what those already have.

## Requirement

Agents drive console B through about forty shell scripts around the file channel. Each one
relearns the protocol and the rules, and the rules hold only as long as every agent
remembers them. This week that failed in eight ways (table below): stale frames, a start on
the HOME screen, setup errors found after a 2-10 minute reset, the app run without its
profile, and answers that cost four log greps.

The goal is one local server that owns console B and the lab's heavy work, answers state
questions from typed resources, and refuses a bad request before it costs a reset. Agents
talk to it over MCP; the user can use the same tools from a thin CLI.

Not in scope: remote control, console A, GitHub writes (agents keep their `gh` rules), video
streaming, and any change to how programs behave without a `Lab/` folder.

## Problems this solves

One row per incident from 2026-11-07..13. "R" = resource, "T" = tool, "E" = event.

| Incident | Prevented by |
|---|---|
| Reused shot tag returned a stale Safari frame while the player was at the Mansion | T `shot()` returns a new frame with its `seq`; never a reused file |
| PNG read while the app was still writing it | T `shot()` returns the image only once written |
| Selecting a settings panel through the inbox blocked the channel | T `run_program` takes program panels only; no raw `select` |
| Panel switch reset controller and video ("requires video feedback") | R `console://B/state` `controller`, `video`; `run_program` waits for both |
| No-HDMI colour bars went unnoticed until a frame was read | R `state.video.signal`; E `video_lost` |
| Game switches (HOME, close, pick tile) done by hand | T `switch_game(target)` |
| PLZA 03.08 started with the game closed and walked on HOME | precondition "game in front = program's game, known screen" |
| UI stop at 22:25 had to be guessed as the user's | E `stopped{source: ui}`; lease moves to `user` |
| "Hands off B while the user plays" held only by memory | lease: tools refuse while `user` holds it |
| Which save B holds lived in notes (Safari logo vs Mansion door, unsaved Parasect) | R `console://B/save` |
| Max Resets 10 on a long-shot target | schema range plus program precondition |
| Safari needs ≥ 4000 advances | precondition on `run_program` |
| FRLG license check not loaded (Game Settings panel never opened) | precondition: game settings loaded |
| No Sweet Scent user in the last slot | precondition, checked by the program's precheck before any reset |
| Restart on a post-run summary screen | precondition "known screen"; `switch_game` brings it there |
| PLZA LANGUAGE left on ja | precondition: language matches the console's game |
| Dev-only checkpoint hidden without Developer Mode shifted indices; wrong checkpoint started | options by key and checkpoint id, never by index; refuse a hidden row |
| Audio input not allowlisted for lab-B (UserSetupError spam) | server launch checks the lab profile's settings |
| App run without `--profile`, twice | agents never run the app; the server launches it |
| Rules retyped per brief and drifting | rules enforced by the server, not restated in briefs |
| Agents parked waiting on the heavy gate or the console | queued tools return a ticket; E `build_done`, `program_finished` |
| Linter rule changed mid-task | not addressed: `ci_status` names the linter ref it ran with, nothing more |
| One upstream sync = merges through 36 branches (+~40 descendants) | T `stack_plan(pr)` |
| CI red from SystemKeyboardDetector images never pushed to CommandLineTests | T `ci_status` classifies "missing test image" |
| Branches checked out in several worktrees went stale | T `stack_plan` lists each branch's worktrees and how far behind they are |
| "How far off is the RNG?" took four greps | R `runs://latest` carries RNG hits (target, hit, delta) |
| Results spread over notes, outbox, error dirs with spaces | R `runs://<id>`: one record, frame and video at clean paths |
| Sim passes, console fails (FRLG 13.01, 12.03; PLZA walk_to_objective) | partly: `runs://<id>` keeps the walked poses a sim test replays |
| Disk under the 30 GB floor | queued `build_lab` refuses under the floor |
| RAM swap from parallel builds | `build_lab` queue: one build, `-j4`, `nice`, waits while a program runs |

## Shape

```
 agent hosts (Claude Code, Codex)   user shell (lab CLI)
            │ MCP (localhost)              │
            ▼                              ▼
   pa-lab server (one process, launchd; owns leases, queues, audit log)
     ├─ resources: console://B/state, console://B/save, runs://
     ├─ tools:     shot, switch_game, run_program, stop_program, build_lab, ci_status, stack_plan
     ├─ events:    resource updates + next_events()
     └─ config:    consoles.json (B only; A has no entry)
            │ files: Lab/inbox → Lab/outbox, Lab/state, Lab/save.json, Lab/runs/<id>/
            ▼
   SerialPrograms, launched by the server with --profile lab-B
```

One long-lived process, not one per client: leases, the build queue and the event log must
be shared by every agent. It is Python with the MCP SDK, in a venv like the code-health one,
and lives with the lab tooling on `agents` (`lab/server/`). App-side changes stay on
`lab/tooling`.

## Resources (read-only, first PR)

**`console://<id>/state`**, from `Lab/state` plus new app fields:

```json
{"console": "B", "game": "PokemonFRLG", "screen": "overworld",
 "panel": "PokemonFRLG:AutoStory", "program_state": "STOPPED", "controller": "ready",
 "video": {"signal": true, "last_frame_age_ms": 33, "frame_seq": 918274},
 "lease": {"owner": "agent:frlg-lane", "since": "22:10", "expires": "22:40"},
 "app": {"pid": 4411, "build": "slot-B-frlg/2026-11-13T21:02"}}
```

`signal` is false for no frames, black or colour bars. `game` and `screen` come from
detectors (HOME, title, overworld, summary), `unknown` when none match.

**`console://<id>/save`**: written by programs to `Lab/save.json` on every save:
`game`, `slot`, `checkpoint`, `position` (`"<map> (x, y)"`), `time`, `run`. It turns
`stale: true` when anything that could save ran unrecorded since: a user lease, the CFR, a
program without the hook.

**`runs://latest`, `runs://<id>`**: #116's run record, summarised:
`verdict` (`passed|failed|stopped`, reason, stop source), checkpoint lines
(`13.01 failed mansion_holes after 2 attempts`), RNG hits (`target 4123, hit 4127,
delta +4`), and the failure frame and video copied to `~/BDSPLab/runs/<id>/` (no spaces).

## Leases and events

`acquire_console(owner, ttl)` returns a lease; `renew` and `release` take it. Every tool
that presses, starts or stops needs the caller's live lease; reads need none. The lease is
the `lablock.sh` lock and the channel's owner token in one.

When the app's program state changes without a server request (Start or Stop clicked in the
window), the server gives the lease to `user` and emits `lease_changed{owner: user}`. Agent
tools then refuse. The user lease ends when the user releases it from the CLI, or after
30 minutes with the program stopped and no UI input.

Events go to an append-only `events.jsonl`, as MCP resource-update notifications, and through
`next_events(since, timeout ≤ 60 s)` for clients without subscriptions:

| Event | Fields |
|---|---|
| `program_finished`, `program_failed` | run id, verdict, checkpoint |
| `stopped` | source: `ui`, `server`, `program`, `error` |
| `video_lost`, `video_back` | last frame seq |
| `autosave`, `save` | the new `console://B/save` |
| `lease_changed` | old and new owner |
| `build_done`, `tests_done` | ticket, result |
| `ci_done` | PR, SHA, result, missing images |

Subagents still never wait: they read the ticket and stop. The main session's background
waits read `events.jsonl` instead of polling logs.

## Tools

Each tool validates before it acts and returns JSON, or a refusal naming the failed check.

| Tool | Validates / does |
|---|---|
| `shot(console)` | Lease not needed. Refuses if `last_frame_age_ms` > 2000 or no signal. Returns the image and `frame_seq`, `time`; the file name carries the seq. |
| `switch_game(console, target)` | Lease, program stopped. HOME, close the current game, launch `target` by title (not tile position), wait for its title screen. |
| `run_program(console, panel, options)` | Lease. Panel is a program; options checked against the program's schema (keys, enum labels, ranges); declared preconditions hold; controller and video healthy. Selects, sets, starts; returns the run id. |
| `stop_program(console)` | Lease. Stops; the event records `source: server`. |
| `cfr_batch(console, lines)` | Lease, program stopped. Command File Runner batch for debugging only; marks the save stale. |
| `build_lab(game, ref)`, `run_tests(slot, filter)` | Queued: one at a time, `-j4`, `nice 15`, waits while a program runs, refuses under the disk floor. Returns a ticket. |
| `ci_status(branch \| pr)` | Per workflow: result, failing tests; a `FileException` on a test image that no CommandLineTests ref holds is reported as `missing_image`, not a code failure. |
| `stack_plan(pr)` | Read-only: parents, children, which branches need a merge after a parent moved, in order; each branch's worktrees and how far they lag. Merges stay with pr-maintenance agents. |

### Preconditions

A program declares its preconditions as data next to its option schema. The server checks
the ones it can answer from state, save and options; the rest run in the program's own
precheck (as FRLG `RngProgramInstance::run_precheck()` already does) through a `dry_run`
start that reads the screen and stops before any reset, in seconds.

| Program | Preconditions |
|---|---|
| FRLG RNG (static, gift, egg, Safari) | a free party slot (gift, egg); Sweet Scent user in the last slot (wild); Safari advances ≥ 4000; Game Settings loaded so the license check is on; game on a known screen, not a summary |
| FRLG / BDSP / PLZA AutoStory | game in front is the program's game; start checkpoint chosen by id and visible (dev-only rows need Developer Mode); `console://B/save` matches the start checkpoint and is not stale; LANGUAGE matches the cartridge |
| Every program | lab profile has audio input allowed or off |

## Enforcement

- **Agents never run the app.** The server launches it with `--profile lab-B` (the
  `labrun.sh` way: direct exec, never a bare binary) and quits only its own process. A
  Claude Code PreToolUse hook and the standing rules refuse Bash calls that name the
  SerialPrograms binary or write into `Lab/inbox`, so the server is the only path.
- **Console A is never exposed.** `consoles.json` has no entry for it; no resource, no tool.
- **One train.** Leases replace `lablock.sh`; the app still refuses presses while a program
  runs.
- **Heavy work is queued.** The disk floor (25 GB to start, 30 GB target) and the heavy
  gate are properties of `build_lab` and `run_tests`, not of agent discipline.
- **Audit.** Every call, its arguments and its result go to `audit.jsonl`.

## Work plan

| PR | Content | Size | Retires |
|---|---|---|---|
| 1 | This design | doc | — |
| 2 | App (`lab/tooling`): `Lab/state` gains `video` (signal, frame age, seq), `stop_source`, `screen`; programs write `Lab/save.json` (FRLG and BDSP AutoStory, FRLG RNG first) | ~250 C++ + tests | — |
| 3 | Server: read-only resources `console://B/state`, `console://B/save`, `runs://`, and `shot()` | ~400 Py + fake-lab tests | `checkin.sh`, `lab_status.sh`, `shotB.sh`, `rec_shots.sh`, log greps for verdicts |
| 4 | Leases and events: `acquire_console`, user lease from UI stops, `events.jsonl`, `next_events` | ~300 Py | `lablock.sh`, `switch_guard.sh`, `run_watch.sh`, `watch_run.sh`, `stage_watch.sh`, `full_run_watch.sh`, `plza_watch*.sh` |
| 5 | App: option `schema` lab command, declared preconditions, `dry_run` precheck; FRLG RNG programs and AutoStory first | ~300 C++ + tests | — |
| 6 | Server: `run_program`, `stop_program`, `switch_game`, `cfr_batch`; launches the app | ~450 Py | `labrun.sh`, `lab_panel.sh`, `start_B.sh`, `stopB.sh`, `run_from_save.sh`, `reload.sh`, `preflight_B.sh`, `stage_run.sh`, `stage_launch.sh`, `cfB.sh`, `cfrB.sh`, `labctl.sh` and `cfrun.sh` as agent entry points |
| 7 | Queued heavy tools: `build_lab`, `run_tests`, disk floor | ~250 Py | `labbuild.sh`, `heavy_gate.sh`, `make_slot_b.sh`, `plza_rebuild.sh` |
| 8 | `ci_status`, `stack_plan` | ~300 Py | `wait_for.sh checks`, `ciwait.sh`, hand-made stack lists |
| 9 | Skills and standing rules point at the tools; drop rules the server enforces | docs | rule text in briefs |

Order: PRs 2-3 answer "what is the console doing and which save does it hold" without
touching it, which alone covers the stale-frame, save and grep incidents. PR 4 makes
ownership explicit. PRs 5-6 move setup errors from minute 10 to second 1. PRs 7-8 are
independent of the console and can go in parallel with 5-6. `disk_gc.sh` and the launchd
jobs stay.

Testing: every server tool is unit-tested against a fake lab folder (a fixture loop that
consumes requests and writes journals, state and save files); app-side fields have C++ unit
tests; each console milestone ends with one scripted session on console B that leaves a run
record.

## Open questions

- **Where the user lease ends:** CLI release or 30 idle minutes, as above, or only the CLI?
- **Upstream or fork-only?** The lab channel and the server are developer tooling; upstream
  may want the app-side parts behind Developer Mode, or not at all.
- **Precondition home:** data next to each program's options (proposed), or one table in the
  server per program id?
