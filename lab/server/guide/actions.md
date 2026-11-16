> **DRAFT.** This page describes a feature that is only partly built. It is updated as each piece lands.
> Status (2026-11-15): Slice 5a (`run_program`, `stop_program` under a lease) is built in [#592](https://github.com/dayfine/Arduino-Source/pull/592), not merged and not switched on. Slice 5b (preflight, options, `cfr_batch`, a console soak) is not built.

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
  is not running; the selected panel is a different program; the controller is not ready.

## Tools

| Tool | What it does | Status |
|---|---|---|
| `acquire_console` / `renew_console` / `release_console` | take, extend and give back the lease | built (control plane) |
| `run_program(panel)` | start the selected program once | built in #592, not switched on |
| `stop_program()` | stop it once; stopping when nothing runs is a no-op, not an error | built in #592, not switched on |
| `preflight_program(panel, options)` | load candidate options, run the program's [start checks](start-checks.md), then put your options back; `run_program` will need its token | planned (5b) |
| options on `run_program` | run with options other than the panel's current ones | planned (5b) |
| `cfr_batch(lines)` | a short allowlisted controller batch, for debugging only; marks the save as unknown | planned (5b) |

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

**Start the selected program** (5a: the panel's current options):
`run_program(console="B", request_id="9b1d7e44-...", lease_token=..., panel="PokemonFRLG:AutoStory")`.
The stored record once the app has started it:

```json
{"request_id": "9b1d7e44-...", "tool": "run_program", "status": "completed",
 "created_at": "2026-11-14T06:20:00.000Z", "completed_at": "2026-11-14T06:20:02.150Z",
 "result": {"run_id": null, "started_at": "2026-11-14T06:20:02.100Z",
            "instance_id": "d8ea6a9d-..."},
 "refusal": null}
```

Sending the same `request_id` again returns this same record and starts nothing. Refusal codes
you can get: `console_held_by_user`, `console_held_by_agent`, `program_running`,
`app_not_running`, `panel_not_program`, `channel_owned`, `invalid_arguments`.

**Stop it.** `stop_program(console="B", request_id=..., lease_token=...)` answers with who
stopped it and when, e.g. `{"sent": true, "stopped_at": "...", "stopped_by": "lab"}`. If nothing
was running: `{"sent": false, "state": "stopped", ...}`, which is not an error.

**Preflight (planned, 5b).** `preflight_program(console="B", panel="PokemonFRLG:RngStatic", options={...})`
returns a token for `run_program`, or the first failed start check:

```json
{"tool": "preflight_program",
 "refusal": {"code": "precheck_failed", "check": "free_party_slots",
             "message": "Free party slots: needs 1 empty slot, found 0.",
             "evidence": {"needed": 1, "found": 0}}}
```

**Controller batch (planned, 5b, debugging only).**
`cfr_batch(console="B", lines=["shot before", "press B 160 640", "shot after"])` runs the lines
with the program stopped and marks the save `stale`.

## Switching it on

The session connection (`.mcp.json`) stays read-only until you decide otherwise. The plan:
5b adds preflight; a soak on console B shows every action has exactly one record; then actions
are enabled one program family at a time.
