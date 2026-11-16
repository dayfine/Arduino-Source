# pa-lab console daemon (`lab/server`)

The local server designed in `docs/design/Agent-Integration.md` (fork #131, branch
`design/agent-integration`). Slice 0: the version-1 contracts, golden examples and a parser.
Slice 2: the attach-only read server ([below](#slice-2-the-attach-only-read-server)). Slice 3: the
durable control plane ([below](#slice-3-the-durable-control-plane)): leases, request records,
tickets and events in SQLite. Slice 5a: leased `run_program` and `stop_program`
([below](#slice-5a-run_program-and-stop_program)); Slice 5b: `preflight_program`, options bound to
its token, and `cfr_batch` ([below](#slice-5b-preflight_program-options-on-run_program-cfr_batch)).
All are served only by `serve --actions`, which nothing is wired to yet.

New here? Start with [GUIDE.md](GUIDE.md): what pa-lab is and what it gives you today.

| Path | What |
|---|---|
| `pa_lab/contracts.py` | Version-1 specs for every document kind, `parse` / `validate` / `dump` / `describe` |
| `pa_lab/legacy.py` | The legacy `Lab/state` key=value file and its mapping to `Lab/state.json` |
| `pa_lab/__main__.py` | `python3 -m pa_lab check <kind> <file>`, `describe <kind>`, `kinds` |
| `contracts/v1/<kind>/*.json` | Golden examples, one folder per kind; canonical JSON (2-space indent, `\n` at end) |
| `contracts/v1/legacy/*.state` | Legacy `Lab/state` twins of three `app_state` goldens (`stopped_ui` is the real lab-B file of 2026-11-14) |
| `contracts/v1/invalid_cases.json` | Documents that must fail: name, kind, input, expected field path and message |
| `tests/test_contracts.py` | Golden parse, round-trip, invalid-case and legacy-mapping tests |
| `pa_lab/config.py` | Server config: the one-entry allowlist (B), loopback port, owner-only token file |
| `pa_lab/lab_reader.py` | Strict reads of the app's state and save fact (through its channel); `console://B/state` and `/save` |
| `pa_lab/channel.py` | The app channel the core sees: typed commands (`Start`, `Stop`, `Snapshot`, ...), typed answers (`Journal`, `AppError` kinds), `AppChannel` |
| `pa_lab/channel_file.py` | The version-1 channel over `Lab/`: the only module that knows its layout, request and journal lines and error text |
| `contracts/v1/channel.md` | The app channel's files, line formats, ordering (state.json after the journal) and retention |
| `tests/fake_channel.py`, `tests/test_channel_{core,file}.py` | An in-memory channel for core tests; the file channel's own tests |
| `pa_lab/tools.py`, `pa_lab/shot.py` | `ensure_game` (verification only) and `shot` (newer than the call) |
| `pa_lab/mcp_server.py` | MCP over Streamable HTTP on `127.0.0.1`: bearer, Host/Origin, sessions, size caps |
| `pa_lab/app_query.py` | `describe_program` and `game_open`: one-line reads through the app's channel (#504) |
| `tests/fake_lab.py` | A fake `Lab/` folder and a fake app answering `snapshot`, `describe_program`, `game_open`, `set` + `start`, `stop`, the proposed `preflight` and console lines, in temp dirs |
| `tests/test_{lab_reader,ensure_game,shot,config,mcp_server}.py` | Slice 2's fake-lab tests |
| `tests/test_app_query.py` | `describe_program` and `game_open` against the fake app |
| `pa_lab/db.py` | The SQLite file: migrations (`PRAGMA user_version`), owner-only file, `BEGIN IMMEDIATE` writes |
| `pa_lab/control.py` | Leases, idempotent requests, tickets, events, `next_events`, reconciliation, compaction |
| `tests/test_{db,control,control_concurrency,control_mcp}.py` | Slice 3's tests: restarts, threads and processes racing, kills mid-write |
| `pa_lab/actions.py` | Slice 5a: `run_program` / `stop_program` through the control plane and the app's `start` / `stop` channel commands |
| `tests/test_actions.py` | Slice 5a's refusals, retries and races against the fake app |
| `pa_lab/preflight.py`, `pa_lab/cfr.py` | Slice 5b: options against the static contract, the `preflight` reply, the token's binding; the `cfr_batch` allowlist |
| `tests/test_{preflight,cfr_batch,cfr}.py` | Slice 5b: option refusals, verdicts, restore, stale tokens; batches, the save mark; the allowlist |
| `pa_lab/status.py`, `pa_lab/stdio_server.py`, `smoke_readonly.py` | Read-only stdio mode for `.mcp.json`: `console_status` ([below](#read-only-stdio-mode-mcpjson)) |
| `tests/test_status_readonly.py` | `console_status` and the read-only gate against a fake profile |

Stdlib only (Python 3.8+), so no venv: the MCP transport is small enough to write on
`http.server`. Run the tests from the repo root:

```sh
python3 -m unittest discover -s lab/server/tests -t lab/server -v
```

Check a real file (from `lab/server`):

```sh
python3 -m pa_lab check app_state "$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B/Lab/state.json"
python3 -m pa_lab describe app_state     # the full field tree of any kind
```

## Kinds and who writes them

The daemon never edits an app-authored document. Its resources embed one whole
(`console_state.app`, `save_resource.fact`) and put their own fields beside it, so the authorship
boundary is the object boundary.

| Kind | Writer | Where |
|---|---|---|
| `app_state` | app | `Lab/state.json`, rewritten atomically on every change and as a heartbeat |
| `save_fact` | app (a program that saved) | `Lab/save.json` |
| `console_state` | daemon | `console://B/state`: `app` + availability, lease, current refusals |
| `save_resource` | daemon | `console://B/save`: `known` / `stale` / `unknown`, reason, last fact |
| `request` | client | a mutating tool call (`acquire_console` ... `cfr_batch`) |
| `request_record` | daemon | request status and the stored result an exact retry gets back |
| `refusal` | daemon | a tool's error result, naming the failed check or field |
| `event` | daemon | one SQLite events row (and a JSONL export line) |
| `event_page` | daemon | the result of `next_events(after_seq)` |

## Rules for every kind

- **Version.** Every document has an integer `schema`; this reader supports `1`. A missing,
  non-integer or unsupported version fails at `schema` before any other field is read. A
  document embedded in another (`console_state.app`) carries its own `schema`.
- **Timestamps** are UTC strings with milliseconds and `Z`: `2026-11-14T06:10:12.124Z`. No
  offsets, no epoch numbers. They round-trip as the exact string.
- **UUIDs** (`instance.id`, `request_id`) are lower-case `8-4-4-4-12`.
- **Required vs nullable.** "Required" means the key is present. A required key may still allow
  `null` where the field tree says `| null`; `null` means "none" or "not known", never "ok".
  Optional keys are omitted, not set to `null`, when a writer has nothing.
- **Unknown values.** Enums are closed within a version: a value the reader does not know fails
  at that field (`video.signal: 'no_hdmi' is not one of ok, black, colour_bars, unknown`).
  Observations that have no detector yet use the literal value `unknown`; a non-`unknown`
  observation needs `observed_at` and `detector`.
- **Unknown fields.** Documents the app or daemon writes are *open*: a field the reader does not
  know is kept verbatim, listed in `Parsed.unknown` and written back by `dump`, so diagnostics
  survive a newer writer. Requests are *closed*: an unknown top-level key or tool argument is
  refused, because requests are the daemon's input boundary.
- **Errors** name the field: `program.last_transition.request_id: required when source is lab`.
  One parse reports every field error it found, except that a version error stops first.

### Compatibility

- Adding an optional field to an open object needs no version change; old readers keep it.
- Adding an enum value, adding a required field, removing, renaming or retyping a field, or
  changing what a value means needs `schema: 2`, and the daemon keeps reading `1` until the app
  stops writing it. A reader refuses versions it does not list; the daemon reports that as
  `availability.reason: unsupported_schema`, not as a guess.

## `app_state` (`Lab/state.json`) and the legacy `Lab/state`

The app keeps writing the legacy file unchanged and writes `Lab/state.json` from the same state.
Every legacy field maps; only the owner token is reduced to a yes/no, so the token is not copied
into a second file.

| Legacy `Lab/state` | `Lab/state.json` |
|---|---|
| `seq=fs012327` | `channel.last_request` (`null` before the first request) |
| `state=STOPPED\|RUNNING\|STOPPING\|NOT_READY\|NO_PROGRAM` | `program.state`, lower case; `no_program` exactly when `panel.kind` is `other` |
| `program=PokemonFRLG:AutoStory` | `panel.id` when `panel.kind` is `program` (legacy writes empty otherwise) |
| `controller=ready\|none\|` | `controller.state` `ready` / `not_connected` / `no_console` |
| `owner=<token>` | `channel.owner_claimed` (bool); the token stays only in the legacy file |
| `error=` | `channel.last_error` (`null` when the last request succeeded) |
| `paused=<label>` | `program.paused` (optional; present only in builds with breakpoints) |

New in `state.json`: `written_at` (heartbeat), `instance.{id,pid,profile}`, `panel.kind`,
`program.run_id`, `program.last_transition.{action,source,request_id,at}`,
`video.{state,frame_seq,captured_at,signal}`, `observation.{game,screen}`.
`legacy.legacy_view()` is the mapping as code; `tests/test_contracts.py` checks the three twins.

## What Slice 1 (the app writer) must produce

- `Lab/state.json` replaced atomically (write beside, then rename) whenever the legacy file is
  written, and at least every few seconds as a heartbeat with a fresh `written_at`.
- A new lower-case UUID in `instance.id` per app start; `instance.pid`; `instance.profile`.
- `program.last_transition` set where Start/Stop is called: `ui` from the widget, `lab` with the
  inbox request's `request_id` (a UUID; the daemon will put it on the request), `program` when the
  program ends itself, `error` when it ends by exception. `request_id` is `null` unless `lab`.
- `video.frame_seq` / `captured_at` from the frame cache; `state: live` requires both.
  `signal`, `observation.game` and `observation.screen` stay `unknown` until their detectors exist.
- Lower-case enums exactly as listed by `python3 -m pa_lab describe app_state`.
- Checked by `python3 -m pa_lab check app_state <file>` and by a native serialization test
  against the goldens in `contracts/v1/app_state/`.

## Slice 2: the attach-only read server

```sh
python3 -m pa_lab init-token ~/.config/pa-lab/token          # 0600; never overwritten
python3 -m pa_lab serve --config ~/.config/pa-lab/server.json
```

```json
{"schema": 1, "host": "127.0.0.1", "port": 8765, "token_file": "~/.config/pa-lab/token",
 "consoles": {"B": {"lab_dir": "~/Library/Application Support/SerialPrograms/Profiles/lab-B/Lab",
                    "profile": "lab-B"}}}
```

The config is the server's own, closed, and must list exactly `B`; it never reads
`~/BDSPLab/consoles.json`. A Claude Code client adds it as an HTTP MCP server at
`http://127.0.0.1:8765/mcp` with the header `Authorization: Bearer <token>`.

**Transport.** One endpoint, `POST /mcp`, JSON-RPC answered as `application/json` (MCP
2025-06-18 or 2025-03-26). `GET` is 405 (nothing is pushed), `DELETE` ends a session. Each request
needs the bearer token (compared in constant time, never logged), a `Host` of
`127.0.0.1:<port>` or `localhost:<port>` (DNS rebinding), and no `Origin` but the server's own;
bodies over 1 MiB are refused. `initialize` opens an `Mcp-Session-Id` session and records the
client's name, which the log carries with each call.

**Resources.** `console://B/state` is `console_state`: the app's `Lab/state.json` exactly as
parsed, availability, the reported lease and `blocked`. `console://B/save` is `save_resource`.
A console id is looked up in the allowlist, never turned into a path.

| What the daemon finds | `console://B/state` |
|---|---|
| no `Lab/state.json` | `unavailable`, `no_state_file` |
| torn/partial file, duplicate key, `NaN`, > 1 MiB, a contract break, another profile's app | `unavailable`, `malformed` (the message names the field) |
| `schema` it does not read | `unavailable`, `unsupported_schema` |
| `instance.pid` not running | `unavailable`, `app_not_running` |
| `written_at` older than 10 s | `unavailable`, `stale_heartbeat` |

The heartbeat limit is 10 s, not 1 s, because the app writes `state.json` between channel
requests and a long request (`walk`) holds it. A new `instance.id` is the new app: the resource
shows it with its own `frame_seq`; `shot` refuses a call that spans the restart.

`save.json` missing is `unknown` / `no_fact`. A save fact is `known` / `hooked_save` until the
app shows a UI Start/Stop after `saved_at`, then `stale` / `user_lease`. A `save.json` that
exists but cannot be read is a JSON-RPC error (`-32001`, the refusal in `data`), not `unknown`.

**Lease, reported only.** With no lease store yet, the lease comes from `Lab/hold` and the app's
last transition: a hold, or a UI Start/Stop, means `user` (no timeout), otherwise `free`.
`console_status` also names the hold (`hold`: owner, reason, acquired_at, or null). A user lease is not
remembered across an app restart until Slice 3's store.

**`ensure_game(console, expected, allowed_screens?, max_age_ms=5000)`** reads the app's
`observation.game` / `.screen` and returns them with their ages and `verified` with a `reason`
(`game_unknown`, `game_mismatch`, `screen_unknown`, `screen_not_allowed`, `observation_stale`).
`unknown` never verifies; until the app has detectors, every call says `game_unknown`.

**`shot(console, max_age_ms=2000, timeout_ms=5000)`** returns the PNG (MCP image content), its
path and the exact `frame_seq` and `captured_at` of that frame. The daemon checks `state.json`
once (the app is up; its `instance.id`), then writes the fixed request `request <uuid>` /
`snapshot frame <timeout_ms>` into `Lab/inbox` at once. The app's `snapshot` command (#481,
`Lab.md` "Snapshot") needs no owner token, program state or controller: it waits for the first
frame after the line starts, saves it and prints `snapshot saved instance=... frame_seq=...
captured_at=... after=... path=...`. The daemon opens only the path it chose, never the one the
journal names, and only when complete (signature and `IEND`). Refusals: `timeout` (the app's
`snapshot timeout`, evidence `{instance_id, frame_seq: after}`; or the app did not answer within
`timeout_ms` + 2 s, and the request is withdrawn from the inbox), `video_stale` (`reason=no_video`,
or the saved frame is older than `max_age_ms`), `video_bad_signal`, `app_unavailable` (a reply
from another instance: `evidence.reason` `instance_replaced`; `panel_changed`, `app_closing`,
`save_failed` as `evidence.reason`; a line error or an incomplete PNG).

**`describe_program(console)`** and **`game_open(console)`** send the app's channel commands of
the same names (#504, `Lab.md` "Describe program" and "Screen"); both read only and need no
lease, owner token or program state. Each reads `state.json` (the app is up; its `instance.id`),
writes the fixed request `request <uuid>` / `<command>` into `Lab/inbox` as `shot` does, waits
up to 5 s + 2 s for the journal, and reads `state.json` again. Arguments are closed: only
`console` (no `game_open image`).

- `describe_program` parses `describe_program <JSON>` and returns it as `program`:
  `{"id", "options", "prechecks", "schema": 1}` (the option entries as the app prints them) with
  `prechecks_supported`. `prechecks` is the string `unsupported` when the program declares no
  list; that is a valid reply, not an error, and never means the checks passed. Otherwise it
  is the program's checks in run order, each `{"name", "needs", "skippable"}` (#511): `name` a
  non-empty string, unique in the list; `needs` the one line of what the program needs;
  `skippable` whether the user can turn the check off. An empty list means the program needs
  nothing. The list is passed through.
- `game_open` parses `game_open yes` / `game_open no REASON` and returns `open` and `reason`
  (`null`, `home`, `user_select`, `black` or `no_video`). `open: false` is an answer, not a tool
  error; it does not tell one game from another (`ensure_game` does).

| What the daemon finds | Refusal |
|---|---|
| `describe_program: the selected panel is not a program.` | `panel_not_program` |
| `schema` other than 1 | `unsupported_schema` |
| a failed line, no single reply line and `ok` line, bad JSON, a missing `id`/`options`/`prechecks`, a precheck entry without a name, `needs` or boolean `skippable`, a name twice, an unknown `game_open` reason | `app_unavailable` (the journal in `evidence`) |
| a reply from another instance | `app_unavailable`, `evidence.reason` `instance_replaced` |
| no journal within 7 s | `timeout`; the request is withdrawn from the inbox |

## Read-only stdio mode (`.mcp.json`)

`python -m pa_lab serve-stdio --profile-dir <profile>` serves MCP over stdio for a Claude Code
`.mcp.json` entry: no port, token or daemon. It is read-only by construction (`LabMcp(read_only=True)`):
the `console://B/state` and `/save` resources and one tool, `console_status`
(`pa_lab/status.py`), which reports what `~/BDSPLab/checkin.sh` does: the app's pid and deployment,
the legacy `Lab/state` fields (owner token dropped), `console://B/state`, the last `Program State:`
and run lines, the last press, the last `keepawake.log` line, the newest failure frame and
screenshot paths and the last log lines. `shot`, `describe_program`, `game_open` (they write into
`Lab/inbox`) and every control tool are neither listed nor callable. `serve --read-only` gives the
same tool set over HTTP.

The project entry in `~/Projects/Arduino-Source/.mcp.json` (local, never committed):

```json
{"mcpServers": {"pa-lab": {"type": "stdio", "command": "/Users/difan/.venvs/lab-claude/bin/python",
  "args": ["-m", "pa_lab", "serve-stdio",
           "--profile-dir", "/Users/difan/Library/Application Support/SerialPrograms/Profiles/lab-B",
           "--keepawake-log", "/Users/difan/BDSPLab/keepawake.log"],
  "env": {"PYTHONPATH": "/Users/difan/Projects/Arduino-Source-agents/lab/server"}}}}
```

Smoke test (reads files only): `~/.venvs/lab-claude/bin/python lab/server/smoke_readonly.py [profile]`
starts the server as `.mcp.json` does and prints `console_status` once. Tests: `tests/test_status_readonly.py`.

## Slice 3: the durable control plane

SQLite (`db_file` in the config, default `~/.local/state/pa-lab/control.sqlite`, created `0600`)
is the source of truth; every change is one `BEGIN IMMEDIATE` transaction, so racing threads,
processes or a restarted daemon cannot both win, and a kill mid-write leaves nothing.

**Leases.** `acquire_console(console, request_id, owner, ttl_s=1200)` takes a `free` console and
returns an opaque `lease_token` (only its hash is in the lease row) and `expires_at`.
`renew_console` / `release_console` need the token. `agent_idle` expires after its TTL;
`agent_running` (entered by Slice 5's run start, `ControlPlane.begin_run`) never expires and ends
when the app reports the run's stop (back to `agent_idle` with a fresh TTL) or a new app instance.
A UI Start/Stop in `Lab/state.json` (`last_transition.source == 'ui'`) revokes any agent lease and
installs `user`, with no timeout, and so does the user's hold file `Lab/hold` (`pa_lab/hold.py`;
the app writes it when the user drives the console, `hold.sh on` by hand). Only the operator ends it;
`release-user` also removes `Lab/hold`, `claim-user` writes it, and `hold.sh off` runs `release-user`:

```sh
python3 -m pa_lab lease --config ~/.config/pa-lab/server.json show|claim-user|release-user
```

A token whose lease ended is refused with the reason: `lease_expired`, `console_held_by_user`,
otherwise `lease_invalid`. `lease_changed` events carry `old`, `new` and a `reason` (`acquired`,
`released`, `expired`, `run_started`, `run_ended`, `app_restarted`, `ui_takeover`,
`operator_claim`, `operator_release`).

**Requests.** `(client_id, tool, request_id)` is unique; `client_id` is the MCP session's client
name. The record keeps the normalized arguments (defaults filled in) and the result or refusal.
An exact retry gets the stored record, refusals included, even after a restart; the same key with
other arguments is `request_id_reused`. `request_status(console, request_id)` reads the caller's
own record on that console (the newest, if one id was used for several tools); the lease token never appears in a status read or an event.

**Tickets** are request records with status `pending`: `open_ticket` / `resolve_ticket` are the
store side of Slice 5's long operations. They survive restarts and resolve once.

**Events.** `next_events(console, after_seq, timeout_s<=60, limit<=500)` returns the events after
the cursor and `next_after_seq`; it waits for one up to `timeout_s`. `after_seq` is also the
client's confirmed position: pass the last cursor you processed, so a lost response loses nothing.
`seq` comes from `AUTOINCREMENT` and is never reused.

**Reconciliation.** Every read of a live app (`console://B/state`) and the daemon's start apply
`observe`: each transition once (UI takeover, `stopped` with its source), a run's end, an app
restart. Expiries due during downtime apply on the next read, once.

**Retention.** `compact()` runs at start and every 500 events: it keeps the newest 10 000 events,
every event after an active cursor (one used in the last 7 days), every event of a pending ticket
and every pending record; finished records go after 7 days. A cursor behind the compacted range
is refused (`invalid_arguments`, field `after_seq`, `evidence.compacted_through`), never served
with a silent gap.

## Slice 5a: `run_program` and `stop_program`

`python3 -m pa_lab serve --config ... --actions` adds the two tools to the HTTP server (and
`ACTION_INSTRUCTIONS` to `initialize`). Without `--actions` they are neither listed nor callable;
`--read-only` and `serve-stdio` (the `.mcp.json` entry) never have them, and `--read-only --actions`
is a usage error.

```text
run_program(console, request_id, lease_token, panel, options={}, preflight_token)   # options must be {} in 5a
stop_program(console, request_id, lease_token)
```

Each call is one request record (`ControlPlane.begin_action`, one transaction): an exact retry gets
the stored record, refusals included, and never sends a second line; the same `request_id` with
other arguments is `request_id_reused` (not stored). The checks run in this order; a failed one is
stored as a `refused` record with a `request_failed` event, and nothing reaches `Lab/inbox`.

| Check | Refusal |
|---|---|
| the request's token holds the lease (idle or running) | `lease_invalid`, `lease_expired`, `console_held_by_user` (a UI Start/Stop or the operator) |
| no earlier run/stop on the console is still pending | `program_not_stopped`, `evidence.pending_tool` |
| `Lab/state.json` is live | `app_unavailable`, `evidence.reason` = the availability reason |
| run: `options` is `{}` (`preflight_token` is required by the contract, not checked in 5a) | `invalid_arguments` (`options`) |
| run: `panel` is the selected program | `invalid_arguments`, field `panel`, `evidence.selected`; `panel_not_program` on the settings page |
| run: `console://B/state`'s `blocked` (program state, controller, video) | `program_running`, `program_not_stopped` (loading), `controller_not_ready`, `video_stale`, `video_bad_signal` |
| the channel's owner lock is not claimed (`channel.owner_claimed`) | `console_held_by_agent`, `evidence.reason: channel_owned` |
| stop: a program is running | not a refusal: `completed` with `sent: false` and the program state |

A passing request is stored `pending` with the app instance it was checked against (table
`actions`, migration 2), and the daemon writes `request <request_id>` / `start 10` (or `stop`)
into `Lab/inbox`, the commands `labctl.sh start|stop` sends; the client's `request_id` is the
channel's request id. It then reads the journal and `state.json` (up to 13 s for a start, 10 s for
a stop) and resolves the record. The app renames the finished journal into place first and
rewrites `state.json` on its next 500 ms timer (later while another request runs), so a journal is
judged against `state.json` only once its `written_at` is after the daemon first read the journal.
The first time the daemon sees the request taken (in `done/` or answered) and its journal finished,
it stores the time (`actions.taken_at` / `answered_at`, migration 3): `disk_gc.sh` prunes `done/`
and `outbox/`, and a request once seen taken must not later read as never sent. Each attempt's inbox name has a random part, so a journal left by
an earlier request with the same id (after compaction dropped its record) never answers it.

While a `run_program` is pending the lease cannot end: `release_console` is refused
`program_running` and the TTL does not expire it, since the run may still start. Once the request
settles, an overdue lease expires at the next read. The outcomes:

| The app shows | Record |
|---|---|
| `last_transition` = this request's start | `completed`, `correlated_by: request_id`; the lease enters `agent_running` in the same transaction, unless `observe` already applied a newer transition |
| `last_transition` = this request's stop, `program.state` `stopped` | `completed`, `stopped_by: lab` |
| after the start's `ok`, a stop by `program` / `error` dated after the request | `completed` with `ended: {source, at}`, `correlated_by: time`; the lease stays idle |
| after the start's `ok`, a stop by a lab script (`lab`, another request id) dated after the request | the same, `ended.source: lab` |
| (stop) a stop by `program` / `error` / a lab script dated after the request | `completed`, `stopped_by` that source, `correlated_by: time` (the terminal event completes it) |
| a UI transition dated after the request | `failed`, `console_held_by_user` (the user lease is already installed). For `run_program`, `evidence.started: true` when the start's `ok` line was read (the run did start and moved the game; `evidence.run_id` is its id), else `false`; the status stays `failed` so a caller stops and waits for the user |
| another `instance.id` without this request's id | `failed`, `app_unavailable`, `instance_replaced` |
| the line failed | `failed`: `owned by` → `console_held_by_agent` (the token is not copied); `Program is RUNNING.` → `program_running`; `controller not connected` → `controller_not_ready`; else `app_unavailable` with the journal |
| the inbox write failed | `failed` at once, `app_unavailable`, `evidence.reason: inbox_write_failed` (nothing was sent) |
| no answer, request still in the inbox | withdrawn; `failed`, `timeout` (nothing was sent) |
| no answer, request never seen in `done/` and no journal | `failed`, `timeout`, `evidence.reason: never_sent` (the daemon stopped between the record and the write; settled at the next start too) |
| a complete journal without an error or the start's `ok`, that nothing above matches, after the wait and once `state.json` was written after the journal | `failed`, `timeout`, `evidence.reason: outcome_unknown` with the journal and transition |
| (start) the start's `ok` line, but `state.json` shows nothing above | stays `pending` (the program started; the lease cannot end); never failed for want of a match |
| no answer, request taken | stays `pending`; settled from `state.json` on the next `request_status`, `next_events`, `console://B/state` read or action call, also after a daemon restart; `failed` / `app_unavailable` if the app exits |

**What Slice 1's data does not give.** `last_transition` keeps only the newest start or stop, and
`run_id` is null until run records exist (#116). So a run that ends before the daemon reads
`state.json` is matched by time (a `program`/`error` stop dated after the request, on the same
instance, after the start's `ok` line), not by id, and its `started_at` is unknown. A `run_id` on
the stop transition, or the app keeping the start beside the stop, would make that match exact.

**Changed by Slice 5b.** `run_program` checks its `preflight_token` (required in the v1 request
contract; 5a took it unchecked) and takes `options`; the 5a refusal for a non-empty `options` is
gone. See below.

## Slice 5b: `preflight_program`, options on `run_program`, `cfr_batch`

Served with the 5a tools by `serve --actions`; still not wired to anything, and `.mcp.json` stays
read-only. Each call is one request record on the same `begin_action` / inbox / settle path as 5a
(an exact retry gets the stored record; a pending earlier action on the console refuses the next).

```text
preflight_program(console, request_id, lease_token, panel, options)
run_program(console, request_id, lease_token, panel, options, preflight_token)
cfr_batch(console, request_id, lease_token, lines)
```

**`preflight_program`** (`pa_lab/preflight.py`, `actions.check_preflight`). First, without a lease
and outside the transaction, `describe_program` (an exact retry skips it). Then, in order:

| Check | Refusal (stored `refused`, nothing sent) |
|---|---|
| lease, no pending action, app live, `panel` selected, `blocked`, channel free | as for `run_program` (5a) |
| the app restarted between `describe_program` and here | `app_unavailable`, `instance_replaced` |
| the program declares a precheck list | `prechecks_unsupported` (absence never passes) |
| every option key is one the program saves; value of the entry's type, within `min`/`max`; an enum/select slug that is listed and enabled; a group is an object of its own entries (plus `Enabled` when toggleable); no `type: other` | `invalid_arguments`, `field` = `options.<key path>` |

Then the daemon writes `request <id>` / `preflight <canonical JSON>` and waits up to 30 s (the
checks may open a menu or go Home and back) for the one reply line, `preflight <JSON>` (format in
`pa_lab/preflight.py`). The record resolves:

| The reply | Record |
|---|---|
| `restored: false` (whatever the verdict) | `failed`, `app_unavailable`, `evidence.reason: options_not_restored`; no token |
| `id` is not `panel` | `failed`, `app_unavailable`, `panel_changed` |
| `verdict: unsupported` | `failed`, `prechecks_unsupported` |
| `verdict: invalid_options` (the app's `check_validity` or its own key check) | `failed`, `invalid_arguments`, `field: options.<key>` |
| `verdict: failed` | `failed`, `precheck_failed`, `check` = the check's name, its message and evidence |
| `verdict: passed` | `completed`: `checks`, `candidate_hash`, `options_hash` (the app's, or null), `preflight_token`, `issued_at`, `expires_at` |
| the line failed (today's app: no `preflight` command) | `failed`, `app_unavailable` with the journal; `owned by` → `console_held_by_agent` |
| a UI Start/Stop or another app instance before the reply | `failed`, `console_held_by_user` / `app_unavailable` `instance_replaced` |
| taken, no reply after 30 s, `state.json` written after that | `failed`, `timeout`, `evidence.reason: outcome_unknown`; the console is free again |

**The token.** A pass stores (table `preflights`, migration 3) the token's hash with its binding:
console, app instance, panel, the candidate's hash (`candidate_hash`), the app's hash of every
option the program holds (`options_hash`: the reply's, else `program.options_hash` in `state.json`,
#601; null from an older app), the last start/stop transition,
`console://B/save` (status, reason, since) with the newest `cfr_batch`'s time, and the hash of the
request's lease token. The token is a salted hash of that binding. `run_program` reads the same
things and, after the 5a checks, refuses with `preflight_stale` when the token is unknown
(`evidence.reason: unknown_token`), older than 120 s (`expired`), or any bound field differs
(`changed`, `evidence.changed` lists them, e.g. `["transition"]`). A run's own start changes the
transition, so a token starts at most one run, and a user editing any option in the panel, one the
candidate does not name included, changes `options_hash` (`changed: ["options_hash"]`).

**Options on `run_program`.** The request is `request <id>`, one `set KEY JSON` line per option
(groups as `GROUP.KEY`, in key order), then `start 10`; the app stops at the first failing line, so
a refused `set` starts nothing (`app_unavailable` with the journal). The panel keeps the options
after the run, as when a user sets them and clicks Start.

**`cfr_batch`** (`pa_lab/cfr.py`, the allowlist from #547). 1-200 lines; only `press`, `dpad`,
`stick`, `wait`, `taps`, `walk`, `shot`, `shots`, `detect`, `wait_for`, `expect`, `game_open`, in
canonical form, with bounded numbers and a worst case of at most 60 s for the batch; no channel
command, `close_game`, `history`, `CAPTURE`, image path, comment or blank line
(`invalid_arguments`, `field: arguments.lines[i]`). The other checks: lease, a program panel
(`panel_not_program`), `blocked` (program stopped, controller ready, video live), channel free.
Before the request is written, the console's save mark (table `save_marks`) is set, so
`console://B/save` is `stale` / `cfr_batch` from then on (a press may have happened even if the
daemon dies next) and every earlier preflight token is stale. The daemon waits for the worst case
+ 5 s; the record is `completed` with `outcome: done`, or `line_failed` with `failed_line`
(1-based in the batch) and `error`, and `output` (what the lines printed, e.g. shot paths). An app
that never took the request: withdrawn, `failed`, `timeout`; the mark stays. An app that took it
and is still alive (`state.json` written) past the wait with no journal: `failed`, `timeout`,
`outcome_unknown` (some presses may have run; the mark stays), so a hung batch frees the console.

**The app's side (fork #601).** The `preflight` command runs on the LabChannel worker: it stages
the candidate on the UI thread (through `run_on_ui`, as `set` does), runs the program's prechecks
on the worker without starting the program, and restores the prior options on the UI thread. If
the options no longer equal the staged candidate when it restores (the user edited the panel
during the checks), it leaves the user's edit in place and replies `restored: false`, which never
yields a token. The reply's `options_hash` is read after the restore: the options the program
holds, which `state.json`'s `program.options_hash` must still show at the run. Until #601 is in
the lab build, a preflight on the real app is `failed` / `app_unavailable` (unknown command), and
until a program overrides its prechecks every preflight is `prechecks_unsupported`, so
`run_program` cannot start anything there.

**Known gaps.** `state.json` is rewritten every 500 ms, so an option edit in the last half second
before a run is not seen by the token. The `set` echoes in the start's
journal are not compared with what was sent (the preflight already refused clamped or disabled
values). The channel's owner token is still not claimed (a claimed channel is refused), and the
console-B soak (every mutation has exactly one record) is still to run.

## User pages

`GUIDE.md` is the user-facing overview; `guide/*.md` has one page per feature, marked DRAFT
until it is built and switched on. A PR that builds or switches on part of a feature updates
its page in the same PR: the status line at the top, the tool tables, and the DRAFT banner when
the feature is done.
