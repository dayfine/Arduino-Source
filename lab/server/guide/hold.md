# Hold the console

[Back to the guide](../GUIDE.md)

**Works today** in the lab app, the lab scripts, keep-awake and the status read. (Ending an
agent's server lease is not yet in use, since agents do not act through the server yet.)

When you use console B, the lab keeps off it.

## How it works

- **It goes on by itself.** When you click Start or Stop in the lab app, or press a button
  through the app window (keyboard or gamepad), the app writes the file `Lab/hold` with you as
  owner, the reason (`ui_start`, `ui_stop`, `user_input`) and the time.
- **Everything else stops.** While the hold is on, the app's lab channel refuses every command
  that presses, starts, stops or changes the program, from any sender. `labctl.sh`, `labrun.sh`
  and `cfr.sh` refuse to start, and keep-awake stops pressing. Reading still works: status,
  screenshots, `describe_program`.
- **It never expires.** It stays until you take it off. This is on purpose: an agent should not
  guess that you have finished.

## What you can ask

- "Who holds console B?" Claude reads `hold` and `lease` from [console status](status.md).
- "Is B free?" `blocked` is empty when it is.

## What you see

```json
"hold": {"owner": "user", "reason": "ui_stop", "acquired_at": "2026-11-16T00:00:51.929Z"},
"lease": {"state": "user", "owner": "user", "acquired_at": "2026-11-16T00:00:51.929Z"},
"blocked": [{"code": "console_held_by_user",
             "message": "The user clicked Stop in the app at 00:00; console B is theirs until they release it."}]
```

(Sample built from the 2026-11-15 status output; the `hold` line shows the file's fields.)

A script refused by the hold prints e.g. `REFUSED: console held by user since 2026-11-16T00:00:51.000Z (ui_stop): the user is on the console; ~/BDSPLab/hold.sh off releases it`,
and the app's channel answers `console held by user since ... (ui_stop): the user is on the console; ~/BDSPLab/hold.sh off releases it`.

## What you do

```sh
~/BDSPLab/hold.sh on [REASON]   # take the console without touching the game
~/BDSPLab/hold.sh status        # who holds it, or "free"
~/BDSPLab/hold.sh off           # give it back
```

`hold.sh off` removes `Lab/hold` (and, once agents act through the server, ends your lease
there too). A `Lab/hold` the server cannot read counts as held (owner `unknown`, reason
`unreadable`); `hold.sh off` clears it.

## What it refuses

Every press, start, stop, `set`, `select`, `preflight`, `popups` and `keepawake on` through the lab
channel, and the lab scripts. It does not refuse reads, and it does not stop a program you started
yourself.
