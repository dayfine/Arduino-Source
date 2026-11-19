> Status (2026-11-18): `console_status` and the `console://B/state` and `/save` resources are
> built and connected in your Claude sessions (`.mcp.json`, read-only). `shot`, `game_open`,
> `ensure_game` and `describe_program` are built but served only by the HTTP server mode, which no
> session uses yet; that part is **DRAFT**.

# Console status: what console B is doing

[Back to the guide](../GUIDE.md)

One tool answers "what is console B doing?" with facts and their times, instead of an agent
running several scripts and grepping the log. It reads files in the lab profile folder and the
keep-awake log; it sends nothing to the app.

## What you can ask

- "What's console B doing?"
- "Is the lab app up, and which build is it?"
- "Did the last checkpoint pass?"
- "Who holds console B?"
- "Where is the newest failure frame?"

Claude calls `console_status(console="B", log_lines=N)`.

## What you see

Real output, 2026-11-18, `log_lines=3`, trimmed (the log lines shortened):

```json
{
  "console": "B",
  "app": {"pid": 86256, "deployment": "slot-B-plza/8cc1615d8940-e2923202"},
  "lab_state": {"state": "STOPPED", "program": "PokemonLZA:AutoStory", "controller": "ready", "error": ""},
  "console_state": {
    "availability": {"state": "available", "reason": null},
    "app": {
      "panel": {"id": "PokemonLZA:AutoStory", "kind": "program"},
      "program": {"state": "stopped",
                  "last_transition": {"action": "stop", "source": "program", "at": "2026-11-19T02:27:17.674Z"}},
      "controller": {"state": "ready"},
      "video": {"state": "live", "frame_seq": 108668},
      "observation": {"game": {"value": "unknown"}, "screen": {"value": "unknown"}}
    },
    "lease": {"state": "free", "owner": null},
    "blocked": []
  },
  "hold": null,
  "program_state": "2026-11-18 18:27:17.674544 - [Program]: Program State: STOPPED",
  "last_run": ["... [Checkpoint] 03.11: passed on attempt 1 (one-shot)"],
  "last_press": "18:56:33",
  "keepawake": "18:48:04 idle 1052s: pressed B through Lab inbox (STOPPED)",
  "newest_failure_frame": ".../Screenshots/AutoStory/failures/20261117-125454060815-champion.jpg",
  "newest_screenshot": ".../Screenshots/screenshot-20261118-140526388785.png",
  "log_tail": ["...", "...", "..."]
}
```

How to read it:

| Field | Tells you |
|---|---|
| `app` | The lab app's pid and the build deployed (slot, commit) |
| `lab_state` | The app's own short report: state, program, controller, last error |
| `console_state.availability` | `available`, or `unavailable` with a reason (app closed, state not written for 10 s, unreadable state) |
| `console_state.app.program.last_transition` | The last start or stop, who did it (`ui` = you, `program`, `error`, `lab`) and when |
| `video` | `live` with a frame counter, or stale |
| `lease`, `hold` | Who holds the console: `free`, or `user` with when and why ([hold.md](hold.md)) |
| `blocked` | Why an agent may not act now, one sentence each; empty means nothing blocks |
| `last_run`, `program_state` | The last checkpoint result lines and state change from the log |
| `last_press`, `keepawake` | The last button press, and what keep-awake last did |
| `newest_failure_frame`, `newest_screenshot` | Paths to open when something failed |

`observation.game` and `.screen` say `unknown`: the app has no detector that names the game yet.

When you hold the console, `blocked` says so (real output, 2026-11-15):

```json
"lease": {"state": "user", "owner": "user", "acquired_at": "2026-11-16T00:00:51.929Z"},
"blocked": [
  {"code": "console_held_by_user", "message": "The user clicked Stop in the app at 00:00; console B is theirs until they release it."},
  {"code": "program_running", "message": "PokemonSV:AutoStory is stopping."}
]
```

With the app closed, `app` is `null` and `blocked` says
`{"code": "app_unavailable", "message": "The app that wrote Lab/state.json (pid 96761) is not running."}`.

## What it refuses

Nothing: it only reads. Only console `B` is accepted.

## What you do

Nothing. Approve `pa-lab` once per project in Claude Code ([GUIDE.md](../GUIDE.md#connect-and-approve-it)).

## Resources

For clients that read resources: `console://B/state` (the `console_state` above) and
`console://B/save` (what is known about the save; today `unknown`, see [saves.md](saves.md)).

## Not connected yet: shot, game_open, ensure_game

**DRAFT.** These are built and tested, but served only by the HTTP server (`python3 -m pa_lab
serve --config ...`), which is not configured on this Mac. They write a request into the app's
`Lab/inbox`, which is why the session connection leaves them out.

| Tool | Ask | Answer |
|---|---|---|
| `shot` | "show me console B" | the PNG, its path, `frame_seq` and `captured_at`; refused `video_stale` when the frame is old, `timeout` when the app does not answer |
| `game_open` | "is the game open?" | `{"open": true}` or `{"open": false, "reason": "home"}` (also `user_select`, `black`, `no_video`) |
| `ensure_game` | "is PLZA on the overworld?" | `verified` and a reason; today always `game_unknown`, since the app has no game detector |
| `describe_program` | "what options does this program take?" | see [start-checks.md](start-checks.md) |

Example `game_open` answer (shape from the code, trimmed; not run on the console): `{"console": "B", "open": false, "reason": "home", "instance_id": "..."}`.
