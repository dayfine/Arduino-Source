> **DRAFT.** Built, not switched on. It is updated as each piece lands.
> Status (2026-11-18): the server side is merged on `agents`: Slice 5a (`run_program`, `stop_program` under a lease, [#592](https://github.com/dayfine/Arduino-Source/pull/592)), Slice 5b (`preflight_program`, options on `run_program`, `cfr_batch`, [#600](https://github.com/dayfine/Arduino-Source/pull/600)), the app channel adapter ([#611](https://github.com/dayfine/Arduino-Source/pull/611)) and the user hold ([#609](https://github.com/dayfine/Arduino-Source/pull/609)). The app's `preflight` command ([#601](https://github.com/dayfine/Arduino-Source/pull/601), [#616](https://github.com/dayfine/Arduino-Source/pull/616)) is in the lab build. These tools are served only by `serve --actions`, which is not configured on this Mac; your sessions stay read-only. No program declares a start-check list yet, so every preflight is refused `prechecks_unsupported` and nothing can be started this way. The console-B soak has not been run.

[Back to the guide](../GUIDE.md)

# Actions: agents start and stop programs through the server

Today agents act on console B through lab scripts. With actions, an agent asks pa-lab instead,
and the server decides whether it may.

## How it works for you

- **You always win.** Clicking Start or Stop in the app gives you the console. Any agent
  request in flight fails, and new ones are refused until you release it.
- **An agent must hold the console first.** It takes a time-limited lease; without one, or
  after it expires, every action is refused.
- **One action, one record.** Each request has an id. Retrying the same id returns the first
  answer and never starts or stops twice. Every action is in the event log.
- **Refusals say why**, for example: you hold the console; another program is running; the app
  is not running; the selected panel is a different program; the controller is not ready; a
  start check failed.
- **Start checks first.** An agent must run the program's start checks with the exact options
  it will use, then start within two minutes. If anything changes in between (you click Start or
  Stop, the app restarts, a save is written, a debug batch runs, other options), it must check
  again. A program that lists no start checks cannot be started this way at all.

## Tools

| Tool | What it does | Status |
|---|---|---|
| `acquire_console` / `renew_console` / `release_console` | take, extend and give back the lease | built (control plane) |
| `preflight_program(panel, options)` | check the options, run the program's [start checks](start-checks.md) without starting it, put your options back; returns a token for `run_program` | built (5b); the app command is in the lab build; refused `prechecks_unsupported` until a program lists its checks |
| `run_program(panel, options, preflight_token)` | load the options and start the selected program once; only with a current token for the same options | built, not switched on |
| `stop_program()` | stop it once; stopping when nothing runs is a no-op, not an error | built, not switched on |
| `cfr_batch(lines)` | a short allowlisted controller batch, for debugging only; marks the save as stale | built, not switched on |

There is deliberately no "switch game" tool: the app cannot yet pick a game on HOME by name.

## Examples

These are the request and answer shapes from the server's contract files
(`contracts/v1/`), with example ids. They are what an agent sends and gets back; you would
ask Claude in plain words ("take console B and run FRLG 13.01 to 13.02").

**Take the console.** `acquire_console(console="B", owner="agent:frlg-lane", ttl_s=1200)`:

```json
{"lease_token": "<opaque>", "expires_at": "2026-11-14T06:40:00.000Z"}
```

If you hold it, the answer is a refusal instead:

```json
{"request_id": "5f0c2a8e-...", "tool": "acquire_console",
 "refusal": {"code": "console_held_by_user",
             "message": "The user holds console B (Stop clicked in the app at 06:10); ask them to release it."}}
```

**Check before starting.** `preflight_program(console="B", request_id=..., lease_token=...,
panel="PokemonFRLG:AutoStory", options={"START_SEGMENT": 3})`. The result (from the server's
tests against a fake app; real console output comes later):

```json
{"panel": "PokemonFRLG:AutoStory", "instance_id": "d8ea6a9d-...",
 "checks": [{"name": "game_open", "status": "passed", "message": ""},
            {"name": "free_party_slots", "status": "passed", "message": ""}],
 "candidate_hash": "22386fd0...", "options_hash": "9f2c41aa...", "preflight_token": "pf1.be3fc628...",
 "issued_at": "2026-11-14T06:00:00.150Z", "expires_at": "2026-11-14T06:02:00.150Z"}
```

When a check fails, the record is failed and names it; your options are back as they were (also
when the checks are cut short because someone changed the app's panel: `app_unavailable` with
`evidence.reason: cancelled`):

```json
{"status": "failed",
 "refusal": {"code": "precheck_failed", "check": "free_party_slots",
             "message": "Your party is full. Leave a party slot free so the program can check the Pokemon it receives.",
             "evidence": {"needed": 1, "party_size": 6}}}
```

**Start it** with the same panel and options and the token:
`run_program(console="B", request_id="9b1d7e44-...", lease_token=..., panel="PokemonFRLG:AutoStory",
options={"START_SEGMENT": 3}, preflight_token="pf1.be3fc628...")`. The server sets the options in
the app, then clicks Start; the panel keeps those options afterwards. The stored record once the
app has started it:

```json
{"request_id": "9b1d7e44-...", "tool": "run_program", "status": "completed",
 "created_at": "2026-11-14T06:20:00.000Z", "completed_at": "2026-11-14T06:20:02.150Z",
 "result": {"run_id": null, "started_at": "2026-11-14T06:20:02.100Z",
            "instance_id": "d8ea6a9d-...", "correlated_by": "request_id"},
 "refusal": null}
```

Sending the same `request_id` again returns this same record and starts nothing. If something
changed since the check (a start or stop, a save, a batch, the app restarting, or anyone editing
any of the program's options in the panel), the answer says what:

```json
{"code": "preflight_stale", "message": "Since the preflight, save changed; run preflight_program again.",
 "evidence": {"reason": "changed", "changed": ["save"]}}
```

If the app refuses one of the `set` lines (it changed since the check), it stops there and has no
undo: the options set before that line stay in the panel. The record is `failed`,
`app_unavailable`, and says which ones (`evidence.applied_options`, `evidence.failed_option`);
check the panel, or run `preflight_program` and `run_program` again, which set them all.

Refusal codes you can get: `console_held_by_user`, `console_held_by_agent`, `program_running`,
`app_unavailable`, `panel_not_program`, `invalid_arguments`, `preflight_stale`; from the check
also `precheck_failed` and `prechecks_unsupported`.

**Stop it.** `stop_program(console="B", request_id=..., lease_token=...)` answers with who
stopped it and when, e.g. `{"sent": true, "stopped_at": "...", "stopped_by": "lab"}`. If nothing
was running: `{"sent": false, "state": "stopped", ...}`, which is not an error.

**Controller batch (debugging only).**
`cfr_batch(console="B", request_id=..., lease_token=..., lines=["shot before", "press B 160 640", "shot after"])`
runs only with the program stopped. Channel commands (`start`, `select`, `set`, ...), `close_game`,
the Capture button and anything over 60 s are refused before anything is pressed. The result
(fake-app test output):

```json
{"outcome": "done", "instance_id": "d8ea6a9d-...",
 "output": [".../Lab/outbox/palab_cfr_batch_21d67be8..._before.png",
            ".../Lab/outbox/palab_cfr_batch_21d67be8..._after.png"]}
```

A line that fails stops the batch: `{"outcome": "line_failed", "failed_line": 2, "error": "..."}`.
After any batch, `console://B/save` says `stale` with reason `cfr_batch`, since the presses may
have changed the game.

## Switching it on

The session connection (`.mcp.json`) stays read-only until you decide otherwise. Done: the
server side and the app's `preflight` command (in the lab build). Left before actions can be
switched on:

1. At least one program declares its start checks (see [start-checks.md](start-checks.md));
   until then every preflight is `prechecks_unsupported`.
2. A server config (`~/.config/pa-lab/server.json` and a token) and a session connected to
   `serve --actions`.
3. The console-B soak ([README.md](../README.md#slice-5-acceptance-the-console-b-soak)): one real
   run driven through the server, then `soak-check` shows every action has exactly one record.
4. Your decision to switch it on, one program family at a time.

## What you do

- Nothing to allow an action. To stop one, click Stop in the app: that wins over any agent.
- When done playing by hand, `hold.sh off` ([hold.md](hold.md)), or agents stay refused.
- Decide when to switch actions on.
