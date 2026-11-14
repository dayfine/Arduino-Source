# pa-lab console daemon (`lab/server`)

The local server designed in `docs/design/Agent-Integration.md` (fork #131, branch
`design/agent-integration`). Slice 0: the version-1 contracts, golden examples and a parser.
Slice 2: the attach-only read server ([below](#slice-2-the-attach-only-read-server)). It has no
lease store, no write path and no press: those are Slices 3 and 5.

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
| `pa_lab/lab_reader.py` | Strict reads of `Lab/state.json` and `Lab/save.json`; `console://B/state` and `/save` |
| `pa_lab/tools.py`, `pa_lab/shot.py` | `ensure_game` (verification only) and `shot` (newer than the call) |
| `pa_lab/mcp_server.py` | MCP over Streamable HTTP on `127.0.0.1`: bearer, Host/Origin, sessions, size caps |
| `tests/fake_lab.py` | A fake `Lab/` folder and a fake app answering `shot` requests, in temp dirs |
| `tests/test_{lab_reader,ensure_game,shot,config,mcp_server}.py` | Slice 2's fake-lab tests |

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

**Lease, reported only.** With no lease store yet, the lease comes from the app's last
transition: a UI Start/Stop means `user` (no timeout), otherwise `free`. A user lease is not
remembered across an app restart until Slice 3's store.

**`ensure_game(console, expected, allowed_screens?, max_age_ms=5000)`** reads the app's
`observation.game` / `.screen` and returns them with their ages and `verified` with a `reason`
(`game_unknown`, `game_mismatch`, `screen_unknown`, `screen_not_allowed`, `observation_stale`).
`unknown` never verifies; until the app has detectors, every call says `game_unknown`.

**`shot(console, max_age_ms=2000, timeout_ms=5000)`** returns the PNG (MCP image content), its
path and `frame_seq_at_least`. The app has no snapshot command yet (#481), so the daemon reads
`(instance.id, frame_seq)` at call start, waits until `state.json` shows a strictly greater
`frame_seq` from the same instance, and only then writes the fixed request `request <uuid>` /
`shot frame` into `Lab/inbox` (a read-only line: no owner token). The saved PNG is that frame or
a later one. It is opened only at the path the daemon chose, and only when complete (signature
and `IEND`). Refusals: `timeout` (no newer frame, or the app did not answer; the request is then
withdrawn from the inbox), `video_stale`, `video_bad_signal`, `panel_not_program` and
`controller_not_ready` (the channel's `shot` needs both today), `app_unavailable` (journal
error, incomplete PNG, `instance_replaced`). The exact frame token of the PNG and a snapshot
without a ready controller wait for the app's snapshot command (#131 Slice 1).
