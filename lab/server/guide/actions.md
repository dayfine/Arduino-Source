> **DRAFT.** This page describes a feature that is only partly built. It is updated as each piece lands.
> Status (2026-11-15): Slice 5a (`run_program`, `stop_program` under a lease) is built in [#592](https://github.com/dayfine/Arduino-Source/pull/592); Slice 5b's server side (`preflight_program`, options on `run_program`, `cfr_batch`) is built on branch `lab/server-slice-5b`, tested against a fake app only. Neither is merged or switched on. The app cannot yet run a program's start checks on request, so on the real console every preflight fails and no run can start; a console soak comes after that.

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
| `preflight_program(panel, options)` | check the options, run the program's [start checks](start-checks.md) without starting it, put your options back; returns a token for `run_program` | server built (5b), needs an app command that does not exist yet |
| `run_program(panel, options, preflight_token)` | load the options and start the selected program once; only with a current token for the same options | built (5a + 5b), not switched on |
| `stop_program()` | stop it once; stopping when nothing runs is a no-op, not an error | built in #592, not switched on |
| `cfr_batch(lines)` | a short allowlisted controller batch, for debugging only; marks the save as stale | built (5b), not switched on |

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
 "options_hash": "22386fd0...", "preflight_token": "pf1.be3fc628...",
 "issued_at": "2026-11-14T06:00:00.150Z", "expires_at": "2026-11-14T06:02:00.150Z"}
```

When a check fails, the record is failed and names it; your options are back as they were:

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
changed since the check, the answer says what:

```json
{"code": "preflight_stale", "message": "Since the preflight, save changed; run preflight_program again.",
 "evidence": {"reason": "changed", "changed": ["save"]}}
```

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

The session connection (`.mcp.json`) stays read-only until you decide otherwise. What is left
before actions can be switched on: the app command that runs a program's start checks without
starting it (with your options put back afterwards); a soak on console B that shows every action
has exactly one record; then actions are enabled one program family at a time, starting with
programs that list their start checks.
