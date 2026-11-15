# Agent integration: an MCP lab server

Status: accepted 2026-11-14, Slices 0-3 built (proposed 2026-11-01; revised 2026-11-14 from
the week's incidents,
`~/BDSPLab/notes/2026-11-13-week-problems.md`, and an implementation review; checked against the
lab scripts and the user's console rules on 2026-11-14; the user's decisions of 2026-11-14
are recorded in [Decisions and open questions](#decisions-and-open-questions)). It builds on:
- the lab channel: fork #107, `SerialPrograms/Source/CommonFramework/Windows/Lab.md`
  (`Lab/inbox`, `Lab/outbox`, `Lab/state`, the owner token);
- run records: fork #116, `docs/design/Lab-RunRecords.md`;
- the lab scripts on `agents` (`lab/control`, `lab/scripts`) and in `~/BDSPLab`
  (`labrun.sh`, `labbuild.sh`, `checkin.sh`, `lablock.sh`, `heavy_gate.sh`, ...).

Before this rollout nothing here was implemented except what those already have, FRLG RNG's one-off
`RngProgramInstance::run_precheck()` (upstream PokemonAutomation/Arduino-Source#1881, which the
precheck list below generalizes) and the `require-profile` Claude Code hook (done, see
Enforcement). In particular, the app did not expose a frame sequence, a generic
game/screen detector, a preflight API, or an action source. Those are deliverables below, not
pre-existing integration points. Everything here is fork tooling for now (user, 2026-11-14).

Build status (2026-11-14): Slices 0-3 are built as fork drafts: #478 (Slice 0, contracts and
fixtures, base `agents`, tip ff209b333), #481 (Slice 1, `Lab/state.json` and the snapshot
command, base `lab/tooling`, tip 629bfde88), #482 (Slice 2, read server, on #478, tip
cbfccc2ac) and #483 (Slice 3, control plane, on #482, tip c9b45d06e). Slice 4 is
approved to start, stacked on upstream #1881's branch `upstream/frlg-free-party-slots`. Draft
#476 (`LabStateV1`, commit 7c44e72c3) adds a serializer and its tests only; it does not modify
`LabChannel` or tag transitions, and its second `ProgramState` enum is to be dropped (see
Resources).

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
| PLZA 03.08 started with the game closed and walked on HOME | T `ensure_game(expected)` and the program's `game_open` precheck refuse the run |
| UI stop at 22:25 had to be guessed as the user's | E `stopped{source: ui}`; lease moves to `user` |
| "Hands off B while the user plays" held only by memory | lease: tools refuse while `user` holds it |
| Which save B holds lived in notes (Safari logo vs Mansion door, unsaved Parasect) | R `console://B/save` |
| Max Resets 10 on a long-shot target | schema range plus program precondition |
| Safari needs ≥ 4000 advances | precondition on `run_program` |
| FRLG license check not loaded (Game Settings panel never opened) | precondition: game settings loaded |
| No Sweet Scent user in the last slot | a precheck declared with the program, run before any reset |
| Restart on a post-run summary screen | precondition "known screen" refuses the run; recovery remains explicit |
| PLZA LANGUAGE left on ja | precondition: language matches the console's game |
| Dev-only checkpoint hidden without Developer Mode shifted indices; wrong checkpoint started | options by key and checkpoint id, never by index; refuse a hidden row |
| Audio input not allowlisted for lab-B (UserSetupError spam) | server launch checks the lab profile's settings |
| App run without `--profile`, twice | done 2026-11-14: the `require-profile` PreToolUse hook refuses the call (Enforcement); from Slice 7 the server launches the app |
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

The app keeps the legacy key/value `Lab/state` (`seq`, `state`, `program`, `controller`,
`owner`, `error`) byte-compatible for existing scripts and atomically writes a new versioned
`Lab/state.json`. The C++ side only serializes. The Python daemon owns strict parsing: it
rejects an unsupported `schema`, keeps unknown fields that matter for diagnostics, and
overlays only server-owned data such as the lease. Shared golden JSON fixtures (Slice 0, #478)
are the cross-language contract: the app's writer is tested against them byte for byte and the
daemon's parser against the same files. Example app file:

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
process. `frame_seq` is one process-wide monotonic counter: it increases for every frame the
app receives and never resets on a panel switch or a camera auto-reset, so frames order by
`(instance.id, frame_seq)` with no stream epoch. `captured_at` is that frame's capture time,
not the time the state file was serialized.
The daemon computes frame age from `captured_at`; it does not persist a decaying
`last_frame_age_ms` value. `program.state` reuses the app's one `ProgramState` enum, mapped
once to the legacy upper-case values and once to lower-case JSON values (`stopped`,
`running`, `stopping`, `not_ready`, `no_program`); no second state enum.
`program.last_transition` is an object, or `null` before the first transition.

Publication: a logical change (panel, program state, transition, controller, video state)
rewrites `Lab/state.json` immediately; heartbeat-only and frame-only changes (`written_at`,
`frame_seq`, `captured_at`) are throttled to one write every 1-2 s.

Version 1 distinguishes transport freshness (`live`, `stale`, `missing`) from visual signal
classification (`ok`, `black`, `colour_bars`, `unknown`). The first slice can truthfully
publish `unknown`; black/colour-bar detection becomes a later detector, not a guessed
boolean. Game and screen observations are also `unknown` until a named detector has run.
Every non-unknown observation must include `observed_at` and, when useful, detector and
confidence fields.

`program.last_transition.source` is emitted where the action originates, not inferred from
the resulting state: `ui`, `lab`, `program`, or `error`. A lab action carries the daemon's
request UUID. `ProgramSession`'s start and stop take an `ActionSource` and a request id, and the
session latches the stop's initiator from `STOPPING` through `STOPPED`, so a later event cannot
overwrite who stopped the run. This is how the daemon distinguishes a user's Stop click from
its own Stop request even if both race.

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

| State | Entry | Exit → target |
|---|---|---|
| `free` | initial state or explicit release | successful `acquire_console` → `agent-idle`; observed UI action or operator claim → `user` |
| `agent-idle` | acquired with a 20-minute TTL, renewed by every tool call | renew → `agent-idle`; explicit release or TTL expiry → `free`; run start → `agent-running`; observed UI action → `user` |
| `agent-running` | the leased caller starts a run | natural terminal event or lab-requested stop (`stop_program`) → `free`; UI-originated transition → `user`; it never expires mid-run |
| `user` | app reports a UI-originated Start/Stop or the operator explicitly claims B | explicit operator release only → `free` |

A UI action revokes any agent lease before a user lease is installed. There is no guessed
30-minute user timeout: absence of UI telemetry is not proof that the user is done. The
operator releases a `user` lease with the thin CLI's `release-user` command, which reads a
separate operator token file; a hook rule refuses that command from an agent shell. The user
and the agents share one login on the lab Mac, so the rule, not cryptography, is the guard. On
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
| `shot(console, max_age_ms=2000, timeout_ms=5000)` | Lease not needed. Records `(instance.id, frame_seq)` at call start and waits for a strictly newer frame from the app's video-session snapshot seam. Returns the completed PNG plus the new token and `captured_at`; never reuses a caller tag. Refuses on timeout, stale transport, or a classified bad signal. |
| `ensure_game(console, expected, allowed_screens)` | Read-only verification. Runs/reads the named detectors and returns the observation and age. It never navigates HOME or launches a title. |
| `describe_program(console, panel)` | Returns a stable program id and only the stable parts of the option schema: keys, types, defaults, bounds and which enum entries are available, plus the declared prechecks (each check's name and `needs` line), without touching the console. Dynamic visibility is not part of it. Refuses settings panels and hidden/unavailable programs. |
| `preflight_program(console, panel, options)` | Lease required because detectors may use app resources, but performs no controller action. Rejects unknown keys and disabled values, stages the candidate options, evaluates dynamic visibility and `check_validity`, then has the app run the program's prechecks in order; always restores the prior options. Returns the checks run, the first failure's name and user message, and evidence. |
| `run_program(console, panel, options, preflight_token)` | Lease. Requires a still-current successful preflight bound to app instance, panel, the candidate options' hash, observation versions and lease. Starts once and returns request/run ids. |
| `stop_program(console)` | Lease. Requests Stop once; success is the matching `last_transition{source: lab, request_id}` or a terminal run event, not merely a stopped snapshot. |
| `cfr_batch(console, lines)` | Lease, program stopped. Debug-only allowlisted command batch; marks the save fact stale. Raw writes to `Lab/inbox` are not exposed. |

`switch_game` is deliberately absent from the first design. Current common routines can
resume or start the selected HOME tile, but cannot reliably select an arbitrary title by
identity. Add a separate proposal only after the app exposes a tested title-selection seam.
Until then, `ensure_game` turns the dangerous case into an early refusal and leaves recovery
to the operator.

The `shot` freshness rule closes both old races: its token is newer than call start, and the
daemon responds only after the atomically published PNG can be opened.

The snapshot is a direct video-session seam: it reads frames from the panel's `VideoSession`
frame listener, never constructs `LabConsole`, and needs no owner token, program state or
controller. It must not wait behind the sequential `Lab/inbox` worker, so a 5-second `shot`
is never blocked by a 30-second controller wait or a long CFR batch. As built in #481
(629bfde88), the `snapshot LABEL [TIMEOUT_MS]` channel command already has the first half: it
waits on that frame listener for the first frame newer than the line (100-60000 ms, default
5000), saves `<request>_<LABEL>.png`, and prints `snapshot saved instance=… frame_seq=…
captured_at=… after=… path=…`, or fails with `snapshot timeout …` or `snapshot unavailable …
reason=no_video|panel_changed|app_closing|save_failed`. It still runs as a line on the
channel's one worker, so moving it off that worker (its own request path served beside the
inbox worker) is the remaining Slice 1 item.

### Program contract and preflight

There is no generic program preflight API today; FRLG RNG has a one-off,
`RngProgramInstance::run_precheck()` (upstream PokemonAutomation/Arduino-Source#1881: the black
border check, then free party slots). The general form is two layers rather than a simulated
Start/Stop cycle:

1. A static contract beside the program descriptor gives stable option keys, types,
   defaults, bounds and enum-entry availability. The daemon validates and normalizes options
   without touching the console. UI row indices and display strings are never API
   identifiers. Dynamic `ConfigOption` visibility and `check_validity` depend on other
   options' values, so they are not in the static contract; preflight evaluates them on the
   loaded candidate.
2. Preconditions declared with the program (user decision, 2026-11-14). The program overrides
   `prechecks()` and returns a list of small named check objects; each has a `name`, a
   one-line `needs` description, and `run(env, context)` returning pass/fail plus a message
   for the user. A shared `run_prechecks()` runs the list in order and throws
   `UserSetupError` on the first failure; the program's start path calls it before any reset,
   so one list guards both a Start clicked in the app and a `run_program` from the daemon.
   Shared checks are written once and reused by name: `game_open`, `black_border`,
   `free_party_slots(n)`, `save_at(place)`. Program-specific checks (Sweet Scent user in the
   last slot, Safari advances) sit beside the program. `describe_program` lists the names and
   `needs` lines without touching the console, so what a program needs can be read without
   running it. A check must not press a controller button, reset, save, or alter options.

There is no server-side rules table, and no template/policy form: the user rejected that in
favour of the plain list because it has to be readable without running (2026-11-14). The
list generalizes upstream #1881's `run_precheck()`: its two checks become `black_border` then
`free_party_slots(1)`. Nothing changes in upstream #1881 now; that move is Slice 4's first PR, and the
server first refuses a run on a failed check in Slice 5.

Candidate options during preflight: the app stages the candidate `BatchOption` JSON on the UI
thread, rejects unknown keys and disabled values, evaluates dynamic visibility and
`check_validity`, runs the prechecks, and always restores the prior options through a scope
guard, on success, refusal or exception. Programs never see a separate cloned copy.

The result of a preflight is a short-lived token hashing the app instance, program id,
the candidate options' hash, relevant state/save observation versions and lease. `run_program` refuses
a stale token. A program with no `prechecks()` override reports `unsupported`; absence never
means a successful check.

| Program | Prechecks |
|---|---|
| FRLG RNG (static, gift, egg, Safari) | `black_border`; `free_party_slots(1)` (gift, egg; upstream #1881's two checks, in that order); Sweet Scent user in the last slot (wild); Safari advances ≥ 4000; Game Settings loaded so the license check is on; game on a known screen, not a summary |
| FRLG / BDSP / PLZA AutoStory | `game_open` (the program's game is in front); start checkpoint chosen by id and visible (dev-only rows need Developer Mode); `save_at(start checkpoint)` against `console://B/save`, not stale (from Slice 6); LANGUAGE matches the cartridge |
| Every program (server-side, not a program precheck) | lab profile has audio input allowed or off; controller/video transport live |

## Enforcement

- **One authenticated local entry point.** Streamable HTTP binds to loopback, requires a
  bearer token with owner-only file permissions, validates `Origin`, caps request sizes and
  records the authenticated client id. The token is not placed in logs or MCP results.
- **Agents never run the app once supervision lands.** The server launches it with
  `--profile lab-B` using the extracted `labrun.sh` behavior (direct exec, never a bare
  binary) and quits only the PID/instance it created. Before that milestone, it is an
  attach-only observer and the existing guarded launcher remains authoritative.
- **The bare binary is already blocked (done 2026-11-14, not part of this rollout).**
  `~/.claude/hooks/require-profile.py` is a Claude Code PreToolUse hook registered for `Bash`
  in `~/.claude/settings.json`. It splits the command on `&&`, `||`, `;`, `|`, `&` and
  newlines, drops leading `VAR=value` assignments and the bare wrappers `exec`, `nohup`,
  `env`, `time`, `caffeinate`, and looks at each segment's program: a path ending in
  `SerialPrograms`, `SerialPrograms.app` or `SerialPrograms.app/Contents/MacOS/SerialPrograms`,
  or `open … SerialPrograms.app`. If that segment has no `--profile` argument the hook exits 2
  with a message naming the target and the call is refused. Everything else passes: builds
  (`cmake --build … --target SerialPrograms`), `pgrep`, `grep`, `ls`, and a launch with
  `--profile <name>`. Checked against those cases on 2026-11-14. Known gaps, not design
  items: it does not guard writes into `Lab/inbox` (the owner token covers that from Slice 5),
  it sees only commands run through Claude Code's Bash tool, and a wrapper with flags
  (`caffeinate -i …`) is not unwrapped.
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

Ownership: C++ only serializes; the Python daemon owns strict parsing, unsupported-schema
rejection and keeping unknown diagnostic fields. The shared golden JSON fixtures are the
cross-language contract: the app's writer must reproduce them, the daemon's parser must
accept them.

Acceptance: the golden fixtures are the contract both languages test against; the daemon's
parser fails malformed, missing-version and future-version examples with a field-level error;
all golden examples round-trip through the parser without losing unknown fields needed for
diagnostics; the app's writer reproduces the app-authored goldens byte for byte.

Built: #478 (on `agents`).

### Slice 1 — app observation seam (first coding branch)

- Keep `Lab/state` byte-compatible and add atomically replaced `Lab/state.json`.
- Give each app process an instance UUID and heartbeat; expose panel id/kind, program state,
  controller state, last transition, frame sequence and capture timestamp.
- Count frames in one process-wide monotonic `frame_seq` with each frame's capture time. Do
  not invent black, colour-bar, game or screen observations; publish `unknown` until their
  detectors exist.
- Pass an `ActionSource` and request id into `ProgramSession`'s start and stop, and latch the
  stop initiator through `STOPPING` → `STOPPED`. Slice 1 owns producing transitions; Slice 5
  only consumes them.
- Map the app's one `ProgramState` enum to the legacy upper-case and JSON lower-case values;
  publish logical changes immediately and throttle heartbeat/frame-only writes to 1-2 s.
- Add a read-only fresh-snapshot command on the video-session seam that does not require
  controller readiness, does not wait behind the inbox worker, and returns the frame token.

Acceptance: native tests cover serialization against the Slice 0 goldens and transition-source
mapping; a manual test restarts the app and proves `(instance, seq)` changes; a snapshot
requested during a panel or controller fault, or during a long controller wait, is either a
newer openable PNG or a typed timeout, never an old image.

Built so far: #481 (`lab/observation-seam`, 629bfde88) writes `Lab/state.json` beside the
unchanged legacy `Lab/state`, rewritten at every 500 ms poll, and adds the `snapshot`
command (see Tools). Its `frame_seq` counts frames at `VideoSession`'s frame listener in one
counter per app instance that survives panel switches, which is decision 1. Where #481 differs
from the decisions above, these are the remaining Slice 1 items: the transition source is
inferred in the channel (it marks its own start/stop just before calling them, and everything
else reads as `ui`) instead of passed into `ProgramSession`; writes are not yet split into
immediate and throttled; `snapshot` still runs on the channel's one worker.

App-side slices follow the repo's hard rules (AGENTS.md), which this design must not bend:

- the heartbeat and the `Lab/state.json` writer run on the lab channel's existing worker, no
  new `std::thread`; locking through `PokemonAutomation::Mutex`;
- files through `Common/Cpp/Filesystem` (atomic replace is write-beside-then-rename) and the
  project's JSON classes; no `std::filesystem`;
- the transition source is set where the session's start/stop is called: the widget passes
  `ActionSource::ui`, the lab channel passes `lab` with its request id, and `ProgramSession`
  holds the latched source; widgets hold no state;
- program contracts and the precheck list are program code: Qt-free, declared next to the
  program descriptor and instance;
- the seams stay small enough to upstream later without a rewrite (user, 2026-11-14): behind
  the Lab/Developer boundary, no change to the shared scheduler or clock, one connection, one
  controller, one video source per app instance.

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

Built: #482 (on #478). `runs://` is not in it; it arrives in Slice 6 with #116's format.

### Slice 3 — durable control plane

- Add SQLite migrations for leases, requests/idempotency, events and tickets.
- Implement acquire/renew/release, user takeover, restart reconciliation,
  `next_events` and request-status reads.
- Add bounded retention/compaction that preserves every active cursor and unresolved ticket.

Acceptance: crash/restart and concurrent-client tests prove one lease winner, stable retry
results, no running-lease expiry and gap-free event catch-up.

Built: #483 (on #482).

### Slice 4 — typed program contract

Approved to start (2026-11-14), stacked on upstream #1881's branch
`upstream/frlg-free-party-slots`.

- Add `describe_program` app support with stable ids and the static option contract (keys,
  types, defaults, bounds, enum availability); keep UI concerns and dynamic visibility out of
  it.
- Add the precheck list: `prechecks()`, `run_prechecks()`, `UserSetupError`, the shared
  checks `game_open`, `black_border`, `free_party_slots(n)`, and structured check results;
  `describe_program` lists them.
- Move FRLG RNG's `run_precheck()` (upstream #1881) onto the list as `black_border` then
  `free_party_slots(1)`. This is the first change to that code and it waits for this slice.
  #1881's "Run Start Checks" setting in the FRLG Game Settings (eb27dfc4f; today it skips the
  party check while the black border check always runs) becomes the switch for FRLG's
  precheck list.
- Then the FRLG AutoStory family with its known-screen checks (it shares the cartridge and the
  one-save-slot risk with the RNG programs). `game_open` runs before `black_border`, since a
  black-border check means nothing on HOME. `save_at(place)` waits for Slice 6's save facts.
  Programs without a list report `unsupported`. Two PRs: the contract and the precheck list
  with FRLG RNG, then the FRLG AutoStory family.

Acceptance: native tests cover option normalization, ranges/enums, hidden checkpoint ids and
side-effect-free prechecks. Deliberately bad Safari, party, language and HOME-screen setups
are refused before a reset (bad saves: Slice 6).

### Slice 5 — leased actions

- Implement `preflight_program` (stage-and-restore candidate options, candidate hash in the
  token), token binding, `run_program`, `stop_program` and the restricted `cfr_batch` over
  request UUIDs.
- This is where the server first refuses a run on a failed precheck: `preflight_program`
  runs the list through the app and `run_program` needs its token. Until then the only
  enforcement is the program's own `run_prechecks()` at Start.
- Consume the transitions Slice 1 produces (source, request id, latched stop initiator) and
  correlate lab transitions with the request id; this slice does not change how they are
  emitted. Make terminal events authoritative for completion.
- Leave `switch_game` out. Do not make selection-by-visible-index part of the protocol.

Acceptance: race tests cover duplicate requests, UI takeover during start/stop, lease loss,
app restart between preflight and start, and a terminal event arriving before the response.
A console-B soak demonstrates that every mutation has exactly one audit/request record.

### Slice 6 — save facts and run-record adoption

- Add the versioned save hook to the initial AutoStory/RNG programs and mark all unhooked
  mutation paths stale with a reason; add the shared `save_at(place)` precheck on top of it.
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
- Only after the soak, tighten the `require-profile` hook (or add a sibling) so agents cannot
  launch the app at all, with or without `--profile`, and update the rules so they use the
  server instead of writing `Lab/inbox` directly.

Acceptance: tests use fake processes/PIDs; manual tests cover stale pidfiles, wrong profile,
missing audio permission, crash/restart and refusal to kill an unrelated process.

### Slice 7b — retire CFR

Settled by the user (2026-11-14): the daemon replaces CFR. Today CFR is a program panel (the
Lab Console's Command File Runner) fed batches through `Lab/inbox` by `lcB.sh`, `cfB.sh` and
`cfr.sh`. To press a button an agent must select that panel, which resets controller and
video, and Start runs whatever panel is selected. A backgrounded panel switch killed the user's
10.01 run that way.

- Move CFR's command set (`press`, `stick`, `dpad`, `shot`, `state`/`detect`, `close_game`)
  into an always-on app-level seam beside the snapshot seam: it owns console B's controller
  and video for the app's lifetime, not a panel. No panel holds the controller, so nothing
  ever switches panels to press a button.
- The daemon exposes each command as a typed tool (`press(console, buttons, hold_ms,
  release_ms)`, ...) with validated ranges, replacing Slice 5's `cfr_batch`. Every press is one
  request record and marks the save fact stale (Slice 6).
- Respect the user lease: a Start or Stop clicked in the app hands B to the user. Raw presses
  need an `agent-idle` lease and are refused while a program runs or the user holds B. On
  2026-11-14 hand presses landed during a run; the seam checks program state itself, so a stray
  request cannot repeat that even when it bypasses the daemon.
- Delete the CFR panel and the inbox's command lines. The file pair stays only as the daemon's
  owner-token-gated transport (Rollout); no script writes `Lab/inbox`.

Acceptance: native tests cover the seam's refusals (program running, user lease, no owner
token) and a press during a panel change; fake-lab tests cover each tool's ranges and request
records; a console-B session drives a story leg's debugging with the typed tools only, with no
panel selected for it, while `run_program` on another panel starts without a switch.

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
| 7b | the CFR panel and script writes to `Lab/inbox`; `lcB.sh`, `cfr.sh`, `sel.sh`; `labctl.sh` and `cfrun.sh` entirely; CFR batches in `keepawake.sh` and the leg scripts (`smash_walk.sh`, `fight_lc.sh`, ...) move to the tools |
| 8 | `labbuild.sh`, `heavy_gate.sh`, `make_slot_b.sh`, `plza_rebuild.sh` |

`disk_gc.sh` and the launchd jobs stay, as before.

## Decisions and open questions

Settled by the user:

- **User lease (2026-11-13, confirmed 2026-11-14).** A Start or Stop clicked in the app
  window, not sent through the lab inbox, hands console B to the user; agents stop until the
  user says it is free. No timeout ends it; the `user` lease state is that rule in code.
- **Console A is never exposed, and the app is never run without `--profile`.** The daemon's
  one-entry allowlist covers the first; the `require-profile` hook (done 2026-11-14, see
  Enforcement) covers the second today and is not a rollout item.
- **Fork-only for now (2026-11-14).** The app's generic seams (`Lab/state.json`, the instance
  id, the snapshot command, `describe_program` and the precheck list) and the mGBA
  controller/video work (its own design, not in this doc) stay on the fork. They are kept
  small enough to upstream later without a rewrite: behind the Lab/Developer boundary, no
  change to the shared scheduler or clock, one connection, one controller, one video source.
  Upstream is pitched only with results, by the user; no agent opens an upstream PR. The
  Python daemon, console inventory, credentials and local policies are fork/local tooling in
  any case.
- **Preconditions are declared with the program (2026-11-14).** The list of named check
  objects in Program contract and preflight; no server-side rules table and no
  template/policy form (rejected: the list must be readable without running). It generalizes
  FRLG's `RngProgramInstance::run_precheck()` (upstream PokemonAutomation/Arduino-Source#1881);
  that code changes only in Slice 4, and the server first refuses a run on a failed check in
  Slice 5. Nothing changes in upstream #1881 now.

Settled by the user (2026-11-14), each the recommended answer of the earlier open list:

1. **Operator authentication.** The thin CLI's `release-user` command reads a separate
   operator token file, and a hook rule refuses that command from an agent shell; the user and
   the agents share one login, so the rule, not cryptography, is the guard (Leases).
2. **Agent lease TTL.** 20 minutes for `agent-idle`, renewed by every tool call; it never
   applies to `agent-running`.
3. **Starting the daemon before Slice 7.** A launchd agent beside `com.dayfine.pa-sync`,
   attach-only; never started from an agent's Bash call.
4. **`runs://` before #116 lands.** Slice 2 ships without it; it arrives in Slice 6 with
   #116's format.
5. **First AutoStory family in Slice 4.** FRLG: it shares the cartridge and the one-save-slot
   risk with the RNG programs already on the list.
6. **Check order and the `unknown` save fact.** `game_open` runs before `black_border`, since
   a black-border check means nothing on HOME; `save_at` refuses while `console://B/save` is
   `unknown` and is not declared before Slice 6.
7. **The hook's known gaps** (wrappers with flags, no `Lab/inbox` guard, Bash tool only).
   Left as they are; the owner token in Slice 5 and app supervision in Slice 7 close them
   structurally.

Settled by the user (2026-11-14), from the implementation review of 7a2da8fac (#131's comment
of 09:11Z):

1. **Frame identity:** one process-wide monotonic `frame_seq`; order by
   `(instance.id, frame_seq)`, no stream epoch (Resources, Slice 1).
2. **Transition provenance:** Slice 1 passes `ActionSource` and the request id into
   `ProgramSession` start/stop and latches the stop initiator through `STOPPING` →
   `STOPPED`; Slice 5 only consumes (Resources, Slices 1 and 5).
3. **Fresh screenshots:** `shot` uses a direct video-session snapshot seam, not the
   sequential `Lab/inbox` worker (Tools, Slice 1).
4. **Static versus dynamic options:** `describe_program` exposes stable keys, types,
   defaults, bounds and enum availability only; preflight rejects unknown/disabled values and
   evaluates dynamic visibility and `check_validity` on the loaded candidate (Program
   contract and preflight).
5. **Candidate options:** preflight stages the candidate and always restores the prior
   options with scope cleanup; the candidate hash is bound into the token (Program contract
   and preflight).
6. **State publication:** logical changes publish immediately; heartbeat/frame-only changes
   are throttled to 1-2 s; `program.last_transition` is an object or `null` (Resources).
7. **Lease terminal state:** `agent-running` → `free` on a natural or lab-requested terminal
   event; a UI-originated transition → `user` (Leases table).
8. **Program-state enum:** `state.json` reuses the app's one `ProgramState` enum, mapped once
   to legacy upper case and JSON lower case; draft #476's second enum is to be dropped
   (Resources).
9. **Slice 0 ownership:** C++ only serializes; the Python daemon owns strict parsing,
   unsupported-schema rejection and unknown diagnostic fields; shared golden JSON fixtures
   are the cross-language contract (Resources, Slice 0).

No questions are open.

Separate designs, not hidden follow-ups inside the console server: arbitrary game switching,
CI diagnosis and stack/worktree planning, and the mGBA controller/video work.
