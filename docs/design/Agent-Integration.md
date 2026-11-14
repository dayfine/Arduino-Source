# Agent integration: an MCP lab server

Status: proposal (2026-11-01; revised 2026-11-14 from the week's incidents,
`~/BDSPLab/notes/2026-11-13-week-problems.md`, and an implementation review; checked against the
lab scripts and the user's console rules on 2026-11-14). It builds on:
- the lab channel: fork #107, `SerialPrograms/Source/CommonFramework/Windows/Lab.md`
  (`Lab/inbox`, `Lab/outbox`, `Lab/state`, the owner token);
- run records: fork #116, `docs/design/Lab-RunRecords.md`;
- the lab scripts on `agents` (`lab/control`, `lab/scripts`) and in `~/BDSPLab`
  (`labrun.sh`, `labbuild.sh`, `checkin.sh`, `lablock.sh`, `heavy_gate.sh`, ...).

Nothing here is implemented except what those already have. In particular, the app does
not yet expose a frame sequence, a generic game/screen detector, a preflight API, or an
action source. Those are deliverables below, not existing integration points.

## Requirement

Agents drive console B through about forty shell scripts around the file channel. Each one
relearns the protocol and the rules, and the rules hold only as long as every agent
remembers them. This week that failed in eight ways (table below): stale frames, a start on
the HOME screen, setup errors found after a 2-10 minute reset, the app run without its
profile, and answers that cost four log greps.

The goal is one local server that owns console B control, answers state questions from typed
resources, and refuses a bad request before it costs a reset. Agents talk to it over MCP;
the user can use the same tools from a thin CLI.

Not in scope for the console server: remote control, console A, GitHub writes, branch/PR
maintenance, CI diagnosis, video streaming, or arbitrary game selection from HOME. Heavy
build/test orchestration can be added later as a separate local-operations worker; it must
not expand the console daemon's credentials or failure domain.

## Problems this solves

One row per incident from 2026-11-07..13. "R" = resource, "T" = tool, "E" = event.

| Incident | Prevented by |
|---|---|
| Reused shot tag returned a stale Safari frame while the player was at the Mansion | T `shot()` returns a new frame with its `seq`; never a reused file |
| PNG read while the app was still writing it | T `shot()` returns the image only once written |
| Selecting a settings panel through the inbox blocked the channel | T `run_program` takes program panels only; no raw `select` |
| Panel switch reset controller and video ("requires video feedback") | R `console://B/state` `controller`, `video`; `run_program` waits for both |
| No-HDMI colour bars went unnoticed until a frame was read | R `state.video.signal`; E `video_lost` |
| Game switches (HOME, close, pick tile) done by hand | deferred: arbitrary title selection has no reliable app seam yet |
| PLZA 03.08 started with the game closed and walked on HOME | T `ensure_game(expected)` and the program's dynamic preflight refuse the run |
| UI stop at 22:25 had to be guessed as the user's | E `stopped{source: ui}`; lease moves to `user` |
| "Hands off B while the user plays" held only by memory | lease: tools refuse while `user` holds it |
| Which save B holds lived in notes (Safari logo vs Mansion door, unsaved Parasect) | R `console://B/save` |
| Max Resets 10 on a long-shot target | schema range plus program precondition |
| Safari needs ≥ 4000 advances | precondition on `run_program` |
| FRLG license check not loaded (Game Settings panel never opened) | precondition: game settings loaded |
| No Sweet Scent user in the last slot | precondition, checked by the program's precheck before any reset |
| Restart on a post-run summary screen | precondition "known screen" refuses the run; recovery remains explicit |
| PLZA LANGUAGE left on ja | precondition: language matches the console's game |
| Dev-only checkpoint hidden without Developer Mode shifted indices; wrong checkpoint started | options by key and checkpoint id, never by index; refuse a hidden row |
| Audio input not allowlisted for lab-B (UserSetupError spam) | server launch checks the lab profile's settings |
| App run without `--profile`, twice | agents never run the app; the server launches it |
| Rules retyped per brief and drifting | rules enforced by the server, not restated in briefs |
| Agents parked waiting on the console | long console operations return a request/ticket; E `program_finished` |
| Linter rule changed mid-task | not addressed by this design |
| One upstream sync = merges through 36 branches (+~40 descendants) | out of scope: keep this in PR-maintenance tooling |
| CI red from SystemKeyboardDetector images never pushed to CommandLineTests | out of scope: keep this in CI-diagnosis tooling |
| Branches checked out in several worktrees went stale | out of scope: keep this in PR-maintenance tooling |
| "How far off is the RNG?" took four greps | R `runs://latest` carries RNG hits (target, hit, delta) |
| Results spread over notes, outbox, error dirs with spaces | R `runs://<id>`: one record, frame and video at clean paths |
| Sim passes, console fails (FRLG 13.01, 12.03; PLZA walk_to_objective) | partly: `runs://<id>` keeps the walked poses a sim test replays |
| Disk under the 30 GB floor | future local-operations worker refuses a build under the floor |
| RAM swap from parallel builds | future local-operations worker serializes builds and applies the existing limits |

## Shape

```
 agent hosts (Claude Code, Codex)       user shell (thin lab CLI)
              │ MCP Streamable HTTP on 127.0.0.1 │
              └──────────────────┬───────────────┘
                                 ▼
                    pa-lab console daemon
              ┌──────────────────┼───────────────────┐
              │                  │                   │
      console-B adapter    SQLite control DB      audit export
      Lab/ files only      leases, events,        JSONL (derived)
                          requests, tickets
              │
              │ Lab/inbox → Lab/outbox
              │ Lab/state + Lab/state.json, Lab/save.json, Lab/runs/
              ▼
       SerialPrograms (`--profile lab-B`)
```

The daemon is one long-lived Python process because leases, idempotency and the event
sequence must be shared by every client. It uses MCP Streamable HTTP bound only to
`127.0.0.1`, checks an unguessable bearer token and validates `Origin`; stdio would create
one independent lease database per client. Its allowlist is a server-owned config with one
entry, B. It must not reuse the operator's broader `~/BDSPLab/consoles.json`, which also
describes A.

The durable control plane is SQLite. JSONL is an audit/export format, not the source of
truth for leases, idempotency or event cursors. The daemon initially attaches to an app
started by the existing guarded launch path. It becomes the app supervisor only after that
launch behavior has been extracted and tested; it must reuse the profile, direct-exec and
process-identity guards rather than reimplementing them.

The console daemon has no GitHub token and does not inspect worktrees. A future
local-operations worker may expose queued build/test tickets over a separate process and
configuration, reusing the existing heavy-gate scripts. `ci_status` and `stack_plan` remain
outside this design.

The daemon lives with the lab tooling on `agents` (`lab/server/`), in a venv like the
code-health one, with its fake-lab tests beside it. App-side slices go on branches from
`lab/tooling` (#107), where the lab channel lives, so this design PR stays documentation only.

## Resources (read-only, first implementation slice)

The app keeps the legacy key/value `Lab/state` for existing scripts and atomically writes a
new versioned `Lab/state.json`. The daemon rejects an unsupported `schema` and overlays only
server-owned data such as the lease. Example app file:

```json
{
  "schema": 1,
  "written_at": "2026-11-14T06:10:12.124Z",
  "instance": {"id": "d8ea6a9d-1eab-4a40-b355-d88072b289a1", "pid": 4411},
  "panel": {"id": "PokemonFRLG:AutoStory", "kind": "program"},
  "program": {
    "state": "stopped",
    "run_id": null,
    "last_transition": {
      "action": "stop",
      "source": "ui",
      "request_id": null,
      "at": "2026-11-14T06:10:10.002Z"
    }
  },
  "controller": {"state": "ready"},
  "video": {
    "state": "live",
    "frame_seq": 918274,
    "captured_at": "2026-11-14T06:10:12.091Z",
    "signal": "unknown"
  },
  "observation": {
    "game": {"value": "unknown", "observed_at": null},
    "screen": {"value": "unknown", "observed_at": null}
  }
}
```

`written_at` is a heartbeat even when no logical field changes. `instance.id` changes on
each app start, so a reset frame sequence cannot be mistaken for a newer frame from the old
process. `frame_seq` increases for every frame accepted by the app's frame cache and
`captured_at` is that frame's capture time, not the time the state file was serialized.
The daemon computes frame age from `captured_at`; it does not persist a decaying
`last_frame_age_ms` value. `program.state` carries the legacy channel's values in lower case
(`stopped`, `running`, `stopping`, `not_ready`, `no_program`), so one app enum feeds both
files.

Version 1 distinguishes transport freshness (`live`, `stale`, `missing`) from visual signal
classification (`ok`, `black`, `colour_bars`, `unknown`). The first slice can truthfully
publish `unknown`; black/colour-bar detection becomes a later detector, not a guessed
boolean. Game and screen observations are also `unknown` until a named detector has run.
Every non-unknown observation must include `observed_at` and, when useful, detector and
confidence fields.

`program.last_transition.source` is emitted where the action originates, not inferred from
the resulting state: `ui`, `lab`, `program`, or `error`. A lab action carries the daemon's
request UUID. This is how the daemon distinguishes a user's Stop click from its own Stop
request even if both race.

**`console://<id>/state`** is the parsed app state plus:

```json
{"lease": {"kind": "agent", "owner": "agent:frlg-lane", "acquired_at": "...", "expires_at": "..."}}
```

It also reports daemon/app reachability and the exact refusal reasons that currently block
an action. Unknown facts stay unknown; absence is never interpreted as success.

**`console://<id>/save`** is a fact with provenance, not a promise that every game save was
observed. Programs that know they saved atomically write `Lab/save.json` with `schema`,
`game`, `slot`, `checkpoint`, structured position, `saved_at`, `run_id`, and
`producer` (program id and build). The daemon exposes a status of `known`, `stale`, or
`unknown` and the reason. It marks the fact stale after a user lease, a CFR batch, or any
program that may save but does not publish the hook. Only a new program-authored save fact
can return it to known.

**`runs://latest`, `runs://<id>`**: #116's run record, summarised:
`verdict` (`passed|failed|stopped`, reason, stop source), checkpoint lines
(`13.01 failed mansion_holes after 2 attempts`), RNG hits (`target 4123, hit 4127,
delta +4`), and the failure frame and video copied to `~/BDSPLab/runs/<id>/` (no spaces).

## Leases, requests and events

Reads need no lease. Every tool that can press a button, start/stop a program or mutate app
selection requires a live lease token, not merely the caller-supplied owner name. The
SQLite state machine is:

| State | Entry | Exit |
|---|---|---|
| `free` | initial state or explicit release | successful `acquire_console` |
| `agent-idle` | acquired with bounded TTL | renew, explicit release, TTL expiry, run start, or observed UI action |
| `agent-running` | the leased caller starts a run | program terminal event, explicit leased stop, or observed UI action; it never expires mid-run |
| `user` | app reports a UI-originated Start/Stop or the operator explicitly claims B | explicit operator release only |

A UI action revokes any agent lease before a user lease is installed. There is no guessed
30-minute user timeout: absence of UI telemetry is not proof that the user is done. On
daemon restart, leases and request results survive in SQLite; the daemon reconciles them
against `instance.id`, current program state and the latest transition before allowing a
mutation.

**Until Slice 7 the existing locks stay in force and the daemon works beside them.**
`labrun.sh` launches the app under `lablock.sh`, and the launcher's train holds that lock for
the app's lifetime; `labctl.sh` and `cfrun.sh` check it before writing a request. From Slice 5
on, the daemon claims the channel's `owner` token (Lab.md) when it attaches and re-claims it
after every app restart, since that token dies with the app; the app then refuses any
mutating request that does not come from the daemon, which is what makes "raw writes to
`Lab/inbox` are not exposed" hold for stray scripts too. Three gates coexist during the
transition: `lablock` on launching the app, the owner token on writing the channel, the lease
on which agent may act. Slice 7 folds the first into the daemon; the owner token stays as the
app/daemon boundary.

Every mutating call requires a client-generated request UUID. The unique key
`(client_id, tool, request_id)` stores the normalized arguments and terminal result. An
exact retry returns the stored result; reuse with different arguments is refused. This
prevents a transport retry from starting a program or pressing a button twice.

Events are rows in SQLite with a monotonically increasing 64-bit `seq`, event time, console,
app instance, request id and JSON payload. MCP resource-update notifications are a wake-up
hint; `next_events(after_seq, timeout <= 60 s)` is the durable catch-up API and returns the
next cursor. JSONL can be exported for humans, but truncating it cannot corrupt cursors.

| Event | Fields |
|---|---|
| `program_finished`, `program_failed` | run id, verdict, checkpoint |
| `stopped` | source: `ui`, `server`, `program`, `error` |
| `video_lost`, `video_back` | last frame seq |
| `autosave`, `save` | the new `console://B/save` |
| `lease_changed` | old and new owner |
| `request_completed`, `request_failed` | request id, tool, normalized result or refusal |

Agents do not park on a tool call. Long operations return a ticket/request id; a later
client consumes events or reads request status. The daemon may keep recent resource
notifications in memory, but correctness always comes from the durable cursor.

## Tools

Each tool validates before it acts and returns JSON, or a refusal naming the failed check.

| Tool | Validates / does |
|---|---|
| `acquire_console(console, owner, ttl)` | Atomically acquires `free`; returns an opaque lease token and expiry. Owner is descriptive, not authority. |
| `renew_console(lease, ttl)` / `release_console(lease)` | Operate only on the matching token. A user lease is released through an operator-authenticated CLI action, not an agent token. |
| `shot(console, max_age_ms=2000, timeout_ms=5000)` | Lease not needed. Records `(instance.id, frame_seq)` at call start and waits for a strictly newer frame. Returns the completed PNG plus the new token and `captured_at`; never reuses a caller tag. Refuses on timeout, stale transport, or a classified bad signal. |
| `ensure_game(console, expected, allowed_screens)` | Read-only verification. Runs/reads the named detectors and returns the observation and age. It never navigates HOME or launches a title. |
| `describe_program(console, panel)` | Returns a stable program id, typed option schema, defaults and declared preconditions. Refuses settings panels and hidden/unavailable programs. |
| `preflight_program(console, panel, options)` | Lease required because detectors may use app resources, but performs no controller action. Checks selection, typed options, static facts and the program's dynamic checks; returns every check and evidence. |
| `run_program(console, panel, options, preflight_token)` | Lease. Requires a still-current successful preflight bound to app instance, panel, normalized options, observation versions and lease. Starts once and returns request/run ids. |
| `stop_program(console)` | Lease. Requests Stop once; success is the matching `last_transition{source: lab, request_id}` or a terminal run event, not merely a stopped snapshot. |
| `cfr_batch(console, lines)` | Lease, program stopped. Debug-only allowlisted command batch; marks the save fact stale. Raw writes to `Lab/inbox` are not exposed. |

`switch_game` is deliberately absent from the first design. Current common routines can
resume or start the selected HOME tile, but cannot reliably select an arbitrary title by
identity. Add a separate proposal only after the app exposes a tested title-selection seam.
Until then, `ensure_game` turns the dangerous case into an early refusal and leaves recovery
to the operator.

The `shot` freshness rule closes both old races: its token is newer than call start, and the
daemon responds only after the atomically published PNG can be opened. An app-side snapshot
command should serve this read-only path without requiring a ready controller.

### Program contract and preflight

There is no generic program preflight API today. It is introduced as two layers rather than
simulating a Start/Stop cycle:

1. A static contract beside the program descriptor gives stable option keys, types,
   constraints, defaults, visibility and declarative fact requirements. The daemon validates
   and normalizes options without touching the console. UI row indices and display strings
   are never API identifiers.
2. A virtual dynamic preflight on the program instance checks facts only the program can
   establish: screen readers, party layout, loaded game settings, save compatibility, and
   similar state. It returns structured checks and must not press a controller button,
   reset, save, or alter options.

The result is a short-lived preflight token hashing the app instance, program id, normalized
options, relevant state/save observation versions and lease. `run_program` refuses a stale
token. Programs without a dynamic implementation declare that explicitly; absence never
means a successful check.

| Program | Preconditions |
|---|---|
| FRLG RNG (static, gift, egg, Safari) | a free party slot (gift, egg); Sweet Scent user in the last slot (wild); Safari advances ≥ 4000; Game Settings loaded so the license check is on; game on a known screen, not a summary |
| FRLG / BDSP / PLZA AutoStory | game in front is the program's game; start checkpoint chosen by id and visible (dev-only rows need Developer Mode); `console://B/save` matches the start checkpoint and is not stale; LANGUAGE matches the cartridge |
| Every program | lab profile has audio input allowed or off; controller/video transport live |

## Enforcement

- **One authenticated local entry point.** Streamable HTTP binds to loopback, requires a
  bearer token with owner-only file permissions, validates `Origin`, caps request sizes and
  records the authenticated client id. The token is not placed in logs or MCP results.
- **Agents never run the app once supervision lands.** The server launches it with
  `--profile lab-B` using the extracted `labrun.sh` behavior (direct exec, never a bare
  binary) and quits only the PID/instance it created. Before that milestone, it is an
  attach-only observer and the existing guarded launcher remains authoritative. Until then
  the only guard is the standing rule: no Claude Code hook refuses a Bash call that names the
  binary today (checked 2026-11-14). A PreToolUse hook that refuses the binary without
  `--profile` and any write into `Lab/inbox` needs nothing from this design and should land
  first, as its own change to the agent harness.
- **Console A is never exposed.** The server's own allowlist has no entry for it; no
  resource, no tool. A user-supplied console id cannot become a filesystem path.
- **One train.** Leases replace `lablock.sh`; the app still refuses presses while a program
  runs.
- **Request boundaries are narrow.** Panel ids and options are validated against the app's
  contract. CFR commands are allowlisted. The server never accepts raw inbox paths, shell
  commands or app binary paths.
- **Audit is durable and redacted.** Every authenticated call, normalized arguments, state
  decision and terminal result is a SQLite record. A JSONL export omits bearer/lease tokens
  and other secrets.
- **Heavy work stays separate.** A future worker owns disk floors, serialization, `-j4` and
  `nice`; the console daemon neither shells out to arbitrary builds nor gains GitHub access.

## Work plan

Each slice is independently reviewable and keeps the existing scripts working until its
replacement passes a console-B soak. App slices go on branches from `lab/tooling` (#107),
server slices on `agents`; changes to this proposal are handed to #131's owner rather than
pushed onto its branch.

### Slice 0 — freeze contracts and fixtures

- Commit version-1 JSON examples for app state, save facts, requests, refusals and events.
- Define enums, required/optional fields, timestamp format, unknown-value rules and schema
  compatibility. Decide which fields are app-authored and which are daemon overlays.
- Add golden parse/round-trip tests before wiring live state.

Acceptance: malformed, missing-version and future-version examples fail with a field-level
error; all golden examples round-trip without losing unknown fields needed for diagnostics.

### Slice 1 — app observation seam (first coding branch)

- Keep `Lab/state` byte-compatible and add atomically replaced `Lab/state.json`.
- Give each app process an instance UUID and heartbeat; expose panel id/kind, program state,
  controller state, last transition, frame sequence and capture timestamp.
- Thread the existing video-cache sequence/timestamp to the lab seam. Do not invent black,
  colour-bar, game or screen observations; publish `unknown` until their detectors exist.
- Add a read-only fresh-snapshot command that does not require controller readiness and
  returns the published frame token.

Acceptance: native tests cover serialization and transition-source mapping; a manual test
restarts the app and proves `(instance, seq)` changes; a snapshot requested during a panel
or controller fault is either a newer openable PNG or a typed timeout, never an old image.

App-side slices follow the repo's hard rules (AGENTS.md), which this design must not bend:

- the heartbeat and the `Lab/state.json` writer run on the lab channel's existing worker, no
  new `std::thread`; locking through `PokemonAutomation::Mutex`;
- files through `Common/Cpp/Filesystem` (atomic replace is write-beside-then-rename) and the
  project's JSON classes; no `std::filesystem`;
- the transition source is set where the session's start/stop is called: the widget passes
  `ui`, the lab channel passes `lab` with its request id; widgets hold no state;
- program contracts and the dynamic preflight are program code: Qt-free, declared next to
  the program descriptor and instance.

### Slice 2 — attach-only read server

- Implement the loopback Streamable HTTP endpoint, bearer/Origin checks, B-only allowlist,
  strict state/save parsers and read resources.
- Implement `shot` with the newer-than-call-start rule and `ensure_game` as verification
  only. Expose `runs://` by adapting #116 once its format is available.
- Treat stale heartbeat, app instance replacement, partial files and unsupported schemas as
  explicit unavailable states.

Acceptance: fake-lab tests exercise partial writes, stale files, sequence resets, app
restarts, timeouts and concurrent readers; a console-B session can replace `shotB.sh` and
status/log greps without granting mutation authority.

### Slice 3 — durable control plane

- Add SQLite migrations for leases, requests/idempotency, events and tickets.
- Implement acquire/renew/release, user takeover, restart reconciliation,
  `next_events` and request-status reads.
- Add bounded retention/compaction that preserves every active cursor and unresolved ticket.

Acceptance: crash/restart and concurrent-client tests prove one lease winner, stable retry
results, no running-lease expiry and gap-free event catch-up.

### Slice 4 — typed program contract

- Add `describe_program` app support with stable ids and typed options; keep UI concerns out
  of the contract.
- Add the side-effect-free dynamic preflight interface and structured check results.
- Implement it first for FRLG RNG and one AutoStory family, including save provenance and
  known-screen checks; missing implementations report `unsupported`. Two PRs: the contract
  and preflight interface with FRLG RNG, then the first AutoStory family.

Acceptance: native tests cover option normalization, ranges/enums, hidden checkpoint ids and
side-effect-free preflight. Deliberately bad Safari, party, language, save and HOME-screen
setups are refused before a reset.

### Slice 5 — leased actions

- Implement `preflight_program`, token binding, `run_program`, `stop_program` and the
  restricted `cfr_batch` over request UUIDs.
- Emit the action source at the UI/lab/program/error origin and correlate lab transitions
  with the request id. Make terminal events authoritative for completion.
- Leave `switch_game` out. Do not make selection-by-visible-index part of the protocol.

Acceptance: race tests cover duplicate requests, UI takeover during start/stop, lease loss,
app restart between preflight and start, and a terminal event arriving before the response.
A console-B soak demonstrates that every mutation has exactly one audit/request record.

### Slice 6 — save facts and run-record adoption

- Add the versioned save hook to the initial AutoStory/RNG programs and mark all unhooked
  mutation paths stale with a reason.
- Adapt #116 run records instead of defining a competing run format; carry stop source,
  checkpoint and RNG hit summaries through resources/events.
- Copy/link failure media only after publication is complete, with clean run-id paths.

Acceptance: a known save survives daemon restart, becomes stale after UI/CFR activity, and
returns to known only after a hooked save. A failed and a stopped run each resolve to one
record with the correct source.

### Slice 7 — supervised app lifecycle

- Extract the existing `labrun.sh` profile, audio-allowlist, direct-exec, build-selection and
  PID identity checks into reusable guarded operations.
- Let the daemon start/stop only the process instance it owns; reconcile crashes and reject
  a second instance. Preserve an operator escape hatch.
- Only after the soak, update hooks/rules so agents use the server instead of launching the
  app or writing `Lab/inbox` directly.

Acceptance: tests use fake processes/PIDs; manual tests cover stale pidfiles, wrong profile,
missing audio permission, crash/restart and refusal to kill an unrelated process.

### Slice 8 — optional local-operations worker

- In a separate process and config, wrap the existing heavy gate, disk floor, `-j4`, `nice`
  and slot-build scripts behind durable tickets.
- Do not add GitHub/PR or worktree mutation to either service. Keep `disk_gc.sh` and existing
  launchd maintenance jobs until separately replaced.

Acceptance: concurrent fake jobs serialize, disk/RAM refusals are typed, cancellation is
safe, and a console run cannot be starved or stopped by the worker.

### Rollout and retirement

Run old scripts and the new read resources in parallel for at least three console sessions,
then compare frame tokens, state transitions and run verdicts. Enable mutation for one
program family at a time. Retire a script only when its replacement has native/fake-lab
coverage, a successful console-B session and a documented operator fallback. Do not delete
the file channel: it remains the app/daemon compatibility boundary.

| Slice | Retires once soaked |
|---|---|
| 2 | `checkin.sh`, `lab_status.sh`, `shotB.sh`, `rec_shots.sh`, log greps for verdicts |
| 3 | `switch_guard.sh`, `run_watch.sh`, `watch_run.sh`, `stage_watch.sh`, `full_run_watch.sh`, `plza_watch*.sh` |
| 5 | `stopB.sh`, `run_from_save.sh`, `reload.sh`, `preflight_B.sh`, `stage_run.sh`, `cfB.sh`, `cfrB.sh`; `labctl.sh` and `cfrun.sh` as agent entry points |
| 7 | `labrun.sh`, `lab_panel.sh`, `start_B.sh`, `stage_launch.sh`, `lablock.sh` |
| 8 | `labbuild.sh`, `heavy_gate.sh`, `make_slot_b.sh`, `plza_rebuild.sh` |

`disk_gc.sh` and the launchd jobs stay, as before.

## Decisions and open questions

Settled by the user's rules:

- User leases end only by explicit operator release. A Start or Stop clicked in the app
  window, not sent through the channel, means the user holds console B until they say it is
  free (2026-11-13); the `user` lease is that rule in code, and no idle timer ends it.
- Console A is never exposed, and the app is never run without `--profile`.

Proposed here, for the user to confirm:

- Static option/precondition data lives with the program contract; dynamic evidence lives in
  the program instance. The server orchestrates and caches results but does not duplicate a
  per-program rules table.
- The generic app-side observation/contract seams are candidates for upstream review behind
  the existing Lab/Developer boundary; whether and when to hand them upstream is the user's
  call (no agent opens an upstream PR). The Python daemon, console inventory, credentials and
  local policies remain fork/local tooling.

Separate designs, not hidden follow-ups inside the console server: arbitrary game switching,
CI diagnosis and stack/worktree planning.
