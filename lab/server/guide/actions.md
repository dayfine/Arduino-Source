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

## Switching it on

The session connection (`.mcp.json`) stays read-only until you decide otherwise. The plan:
5b adds preflight; a soak on console B shows every action has exactly one record; then actions
are enabled one program family at a time.
