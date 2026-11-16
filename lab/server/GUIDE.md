# pa-lab: what it is and what it gives you

pa-lab is a small local server that lets a Claude session see lab console B (the Switch, the
SerialPrograms lab app on profile `lab-B`, and its log) through MCP, the protocol Claude Code
uses for tools. Instead of an agent running shell scripts and grepping logs to work out what
the console is doing, it asks pa-lab and gets one structured answer.

Today, as connected to your sessions, it only reads: nothing there presses a button, starts or
stops a program, or writes to the app. Console A is never part of it.

Developer details (contracts, slices, tests) are in [README.md](README.md); the design is
`docs/design/Agent-Integration.md` on branch `design/agent-integration`.

## What you can use today

pa-lab is connected in Claude Code sessions opened in `~/Projects/Arduino-Source`, through
`.mcp.json`. A new session asks once to approve it (`/mcp` shows it as `pa-lab`).

**One tool, `console_status`.** Ask Claude for "console B status" (or it calls the tool itself on
check-ins). It returns:

| Field | What it tells you |
|---|---|
| `app` | Whether the lab app is running, its pid, and which build (deployment and commit) |
| `lab_state` | The app's own report: selected program, `RUNNING` / `STOPPING` / `STOPPED`, controller, last error |
| `availability`, `blocked` | Whether an agent may act, and if not, why in plain words, e.g. "The user clicked Start in the app at 00:37; console B is theirs until they release it." |
| `lease` | Who holds console B: `user` (you clicked Start or Stop in the app), an agent, or free |
| `video` | Whether camera frames are arriving |
| `program_state`, `last_run` | The last program state change and the last checkpoint result lines |
| `last_press`, `keepawake` | When a button was last pressed, and what keep-awake last did |
| `newest_failure_frame`, `newest_screenshot` | Paths to the latest failure frame and screenshot |
| `log_tail` | The last log lines (how many is a parameter) |

**Two resources**, for clients that read resources instead of calling tools:
`console://B/state` (the same console state) and `console://B/save` (what is known about the
save on the console; today usually `unknown`, see Slice 6 below).

## What it changes for you

- **Your clicks are respected automatically.** When you press Start or Stop in the lab app,
  pa-lab marks console B as held by you, and agents see that in every status read and stop
  until you release it.
- **Cheaper, more reliable check-ins.** The 10-minute lane ticks and overnight check-ins read
  one answer instead of several scripts and log greps.
- **Fewer wrong guesses.** It reports the build actually running, a program stuck in
  `STOPPING`, or an app that exited, as facts with timestamps.

## What is built but not switched on

These run only in the HTTP mode of the server, not in the read-only session connection:
`describe_program` (a program's options and its start checks), `game_open` (is the program's
game in front, or HOME / user select / black screen), `shot` (a fresh screenshot) and
`ensure_game`. The control plane is also built: console leases, request records that make a
retried request safe, and an event log. None of it presses buttons yet.

**Starting and stopping a program (Slice 5a).** `run_program` and `stop_program` exist, but only
when the server is started with `serve --actions`, which nothing uses yet; your session connection
stays read-only. An agent holding the console's lease can start the program selected in the app
(with its current options) and stop it. It is refused whenever you hold the console (you clicked
Start or Stop), when another program is running, when the app is not running, or when the
selected program is not the one asked for. If you click Start or Stop while an agent's start or
stop is under way, yours wins and the agent's request is marked failed. Every start and stop is
one record, and a retried request never starts the program twice.

## What comes next

| Slice | What you would get |
|---|---|
| 4 (rest) | FRLG AutoStory's start checks listed and run before any press (fork #488) |
| 5 (rest) | The server runs the program's start checks before a start and refuses on a failed one (`preflight_program`), passes options, and runs debug button batches (`cfr_batch`); then a soak on console B |
| 6 | Save facts: the server knows which save is on the console and refuses a run that would start from the wrong one; run records become readable |
| 7 | The server launches and supervises the lab app itself, so a build always starts with the right profile |
| 8 | Optional: a local worker for routine operations |

## Limits

- One console, B. Windows is not supported.
- When the lab app is not running, `console_status` says so (`app_unavailable`) and shows the
  last state the app wrote.
- Read-only means read-only: to act on the console, agents still use the lab scripts, under the
  same rules as before.
