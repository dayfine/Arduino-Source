# pa-lab: the lab server, a guide for the person running the lab

pa-lab is a small program on your Mac that sits between Claude and lab console B (the lab
Switch, the SerialPrograms lab app on profile `lab-B`, and its log). Claude asks pa-lab what the
console is doing, and later asks it to start or stop a program; pa-lab answers with facts, or
refuses and says why.

This page is for you, the person who runs the lab. It says what works today, what is built but
not switched on, and what is still a design. Developer detail (contracts, file formats, tests)
is in [README.md](README.md).

Checked against the code on `agents` and the lab build running on console B on 2026-11-18
(`slot-B-plza/8cc1615d8940`).

## Contents

- [The short version](#the-short-version)
- [Connect and approve it](#connect-and-approve-it)
- [Feature map](#feature-map)
- [What is guaranteed](#what-is-guaranteed)
- [Features, one by one](#features-one-by-one)
- [A day in the lab](#a-day-in-the-lab)
- [Troubleshooting](#troubleshooting)
- [Glossary](#glossary)

## The short version

- **Today, in your Claude sessions, pa-lab only reads.** One tool, `console_status`, tells
  Claude whether the app is up, what program is selected and running, who holds the console,
  whether video is live, and the last run lines. Nothing in that connection presses a button.
- **Your hold already works.** When you click Start or Stop in the app, or press buttons
  through the app window, the console is yours. The app, the lab scripts and the server all keep
  off it until you release it with `hold.sh off`.
- **Starting and stopping programs through pa-lab is built but not switched on.** It runs only
  in a separate server mode nobody runs yet, and no program lists its start checks yet, so even
  there nothing can be started.
- **Knowing which save is on the console is designed, not built.**

## Connect and approve it

pa-lab is already set up for Claude Code sessions opened in `~/Projects/Arduino-Source`. The
file `~/Projects/Arduino-Source/.mcp.json` (local, not in git) starts it.

1. Open Claude Code in `~/Projects/Arduino-Source`.
2. The first time, Claude Code asks whether to use the project's MCP server `pa-lab`. Approve it.
3. Check with `/mcp`: `pa-lab` should be listed as connected, with one tool, `console_status`.
4. Ask: *"what's console B doing?"*

It runs the code in `~/Projects/Arduino-Source-agents/lab/server` (your checkout of the `agents`
branch) with the Python in `~/.venvs/lab-claude`. Pull that checkout to get server fixes; restart
the Claude session to pick them up.

If it does not show up: run the smoke test, which starts the server the same way and prints one
status answer:

```sh
~/.venvs/lab-claude/bin/python ~/Projects/Arduino-Source-agents/lab/server/smoke_readonly.py
```

## Feature map

States: **connected** = built and in your Claude sessions today. **built, not connected** = the
code is merged and tested, but no session uses it. **designed** = written up, no code.
**idea** = mentioned, not designed.

| Feature | What it is for you | State | Page |
|---|---|---|---|
| Console status | One answer: app, program, lease, video, last run, last press, failure frame | **connected** (`.mcp.json`, read-only) | [status.md](guide/status.md) |
| User hold | Your Start/Stop or button press makes the console yours until `hold.sh off` | **connected**: the app (lab build has #608), lab scripts and the status read all honor it | [hold.md](guide/hold.md) |
| Save resource (`console://B/save`) | What is known about the save | **connected**, but always `unknown`: nothing writes save facts yet | [saves.md](guide/saves.md) |
| Screenshot (`shot`) | A fresh frame, with its frame number and time | built, not connected (HTTP server mode only) | [status.md](guide/status.md#not-connected-yet-shot-game_open-ensure_game) |
| `game_open` | Is the game in front, or HOME / user select / black | built, not connected | [status.md](guide/status.md#not-connected-yet-shot-game_open-ensure_game) |
| `ensure_game` | Is it *this* game on *this* screen | built, not connected; always answers `game_unknown` (the app has no game detector yet) | [status.md](guide/status.md#not-connected-yet-shot-game_open-ensure_game) |
| `describe_program` | A program's options and start checks, without running it | built, not connected; app side in the lab build | [start-checks.md](guide/start-checks.md) |
| Start checks (lists) | Named things a program needs before it presses anything | the mechanism is built; **no program declares a list yet** (FRLG AutoStory's was a design note, #488, closed) | [start-checks.md](guide/start-checks.md) |
| Console lease | An agent takes console B for a while; you always win | built, not connected (HTTP server mode) | [actions.md](guide/actions.md) |
| Request records and event log | Every action once, a retry never repeats it | built, not connected | [actions.md](guide/actions.md) |
| `preflight_program` | Run a program's start checks with the agent's options, put yours back | built, not connected (`serve --actions`); app side in the lab build (#601, #616); refuses every program today (`prechecks_unsupported`) | [actions.md](guide/actions.md) |
| `run_program` / `stop_program` | An agent starts or stops the selected program | built, not connected; a start needs a passed preflight, so none can start today | [actions.md](guide/actions.md) |
| `cfr_batch` | A short allowlisted button batch for debugging | built, not connected | [actions.md](guide/actions.md) |
| Slice 5 soak | Proof on the real console that every action has exactly one record | tool built (`soak-check`); **the soak has not been run** | [actions.md](guide/actions.md#switching-it-on) |
| Save facts and `save_at` | The server knows which save is on the console and refuses a run from the wrong one; `no_save`, save-screen reads | **designed** (Slice 6, #621) | [saves.md](guide/saves.md) |
| Run records | One readable record per run | **designed** (Slice 6e); needs the app's run ids | [saves.md](guide/saves.md#run-records) |
| Server runs the lab app | Launches the right build with the lab profile; crash handling | planned (Slice 7), not designed here | [app-lifecycle.md](guide/app-lifecycle.md) |
| Local worker | Builds and test runs queue instead of colliding | planned (Slice 8), optional, not designed here | [worker.md](guide/worker.md) |
| Headless app | The app without a window | idea; designed elsewhere, not part of pa-lab | none |

## What is guaranteed

These hold today, in plain words.

- **Console A is never touched.** pa-lab knows one console, B. Its configuration must list
  exactly `B`, and a console name is looked up in that list, never turned into a path.
- **Your session connection cannot act.** The `.mcp.json` connection is read-only by
  construction: it lists one reading tool, and the tools that write to the app are neither listed
  nor callable. It reads files; it does not even ask the app for a screenshot.
- **You win.** A click on Start or Stop in the app, or a button press through the app window,
  puts the hold on. While the hold is on, the app refuses every button press, start, stop or
  option change from the lab channel, whoever sends it; `labctl.sh`, `labrun.sh` and `cfr.sh`
  refuse too, and keep-awake stops pressing. Only you take it off.
- **When the server can act (not yet switched on), it adds more:**
  - an agent must hold the lease to act, and your hold ends any agent lease at once;
  - if you click while an agent's start is on its way, yours wins and the agent's request is
    marked failed;
  - each request is recorded once; sending it again returns the first answer and never starts
    or stops twice;
  - a start needs the program's start checks to have passed within two minutes with the same
    options, and nothing changed in between;
  - after the checks, your option values in the panel are put back; if you edited the panel
    during the checks, your edit stays and no start is allowed.
- **It says why.** Every refusal has a code and a sentence, for example
  `console_held_by_user`: "The user clicked Stop in the app at 00:00; console B is theirs until
  they release it."

## Features, one by one

Each has its own page with examples. Short versions:

### Console status (connected)

- **Ask:** "what's console B doing?", "is the lab app up?", "did 03.11 pass?", "who holds B?"
- **You see:** the app's pid and build, program and state, controller, video, lease, hold, the
  last checkpoint lines, last press, keep-awake, newest failure frame. Real example in
  [status.md](guide/status.md).
- **Refuses:** nothing; it is a read. When the app is closed it says so (`app_unavailable`).
- **You do:** nothing.

### Your hold (connected)

- **Ask:** "who holds B?" Claude reads `hold` and `lease` in the status.
- **You see:** `"lease": {"state": "user", ...}` and a `console_held_by_user` line in `blocked`.
- **Refuses:** while held, every press, start, stop and option change from the lab, whoever
  sends it. Reading still works.
- **You do:** click Start or Stop or press in the app to take it (automatic); `hold.sh on` to
  take it without touching the game; `hold.sh off` to give it back. See [hold.md](guide/hold.md).

### Start checks (mechanism built, no lists yet)

- **Ask (later):** "what does FRLG Static RNG need before it starts?"
- **You see:** each check's name and what it needs, e.g. "At least 1 empty party slot."
- **Refuses:** a start whose first failing check is named, before any reset.
- **You do:** fix what the check names (free a party slot, fix the black border), or turn off a
  check marked skippable for your own Start. See [start-checks.md](guide/start-checks.md).

### Actions: start and stop through the server (built, not connected)

- **Ask (later):** "take console B and run FRLG 13.01 to 13.02", "stop the run".
- **You see:** the start time and request id, or a refusal with a code.
- **Refuses:** when you hold the console, another program runs, the app is down, the panel is
  another program, the controller or video is not ready, a start check failed, or anything
  changed since the check.
- **You do:** nothing to allow it; click Stop at any time to take over. Switching it on is your
  decision. See [actions.md](guide/actions.md).

### Save facts (designed)

- **Ask (later):** "which save is on console B?"
- **You see:** `known` at a checkpoint start, `stale` with the reason (for example you played by
  hand), or `no_save`.
- **Refuses:** a run whose start checkpoint is not the known save.
- **You do:** after playing by hand, save at the checkpoint start and let a program confirm it.
  See [saves.md](guide/saves.md).

### Later: the app's lifecycle, a local worker

See [app-lifecycle.md](guide/app-lifecycle.md) and [worker.md](guide/worker.md). Both are plans.

## A day in the lab

One day, told as it will work once every slice is in. Each step says what works **today**.

**1. The agent checks in.** Claude calls `console_status`: app up, `PokemonLZA:AutoStory`
`STOPPED`, controller ready, video live, lease `free`, `blocked: []`, last line
`[Checkpoint] 03.11: passed on attempt 1`.
*Today: works exactly like this* (real output in [status.md](guide/status.md)).

**2. It reads what the program needs.** `describe_program` lists the options and start checks.
*Today: built, not connected. Every program answers `"prechecks": "unsupported"`.*

**3. It takes the console and preflights.** `acquire_console`, then `preflight_program` with
`START_SEGMENT` and the checkpoints it wants. The app checks `game_open`, `save_at(03.12)` and the
rest, without starting, and puts your panel options back. Pass: a token valid two minutes.
*Today: built, not connected; refused `prechecks_unsupported`. `save_at` is designed (Slice 6).*

**4. It runs the checkpoint.** `run_program` with the token. The app sets the options and clicks
Start; the lease becomes `agent_running`. The checkpoint saves at its start; the server records
the save as `known` at 03.12.
*Today: the agent starts runs with the lab scripts (`labctl.sh`), as before. The start is built,
not connected; the save record is designed.*

**5. You take over.** You see the run walk into a wall and click Stop. The app writes the hold;
the agent's lease ends; any request it had in flight is marked failed (`console_held_by_user`).
You walk the player back by hand.
*Today: the hold works: the app writes it, and the scripts and keep-awake stop. The lease and
request records are built, not connected.*

**6. The save goes stale.** Because you played, the server marks the save `stale` (reason
`user_lease`). Moving the player does not change the save, but nobody can tell whether you saved.
*Today: designed. `console://B/save` says `unknown`.*

**7. The next run is refused.** You run `hold.sh off`. The agent preflights 03.12 again and gets
`precheck_failed`, check `save_at`: "The save on console B is stale: user_lease since 23:50. Save
at 03.12's start, or let a program confirm it."
*Today: designed. Today the agent must ask you, or check the screen itself, before it reloads.*

**8. It recovers.** One of: you save at 03.12's start and a program reads the save screen
(`save_screen`) and records it; or the agent reruns the checkpoint before, which saves; or (PLZA)
the program loads the backup and maps it to a checkpoint (`backup_load`). The save is `known`
again, the preflight passes, the run starts.
*Today: designed. Recovery is by hand, following the lab notes.*

## Troubleshooting

| You see | What it means | What to do |
|---|---|---|
| `app` is `null`; `blocked` has `app_unavailable`: "The app that wrote Lab/state.json (pid ...) is not running." | The lab app is closed or crashed. The status shows the last state it wrote. | Start the lab app the usual way (`labrun.sh`, never the bare binary). |
| `availability` `unavailable`, reason `stale_heartbeat` | The app is up but has not written its state for 10 s: busy (a long `walk`) or hung. | Wait a few seconds and ask again; if it stays, look at the app window. |
| `video_stale` ("The video is ...") or `video_bad_signal` | No camera frames, or a black or wrong picture. | Check the capture card cable and the app's video source; do not run `ffmpeg` on the card. Then ask again. |
| `console_held_by_user` and `hold` names you | You clicked Start or Stop or pressed in the app, or ran `hold.sh on`. It does not expire. | When you are done: `hold.sh off`. |
| `hold` with owner `unknown`, reason `unreadable` | `Lab/hold` exists but cannot be read. It counts as held, on purpose. | `hold.sh off`, which removes the file. |
| `program_running` or a program stuck in `STOPPING` | A program is running, or slow to stop. | Wait, or click Stop. |
| `console://B/save` says `unknown` | Normal today: nothing records saves yet (Slice 6). | Check the save yourself before a checkpoint run. |
| (later) `precheck_failed`, check `save_at`, `stale` | The save may have changed since it was recorded. | Save at the checkpoint start and let a program confirm it, or run the checkpoint before. |
| (later) `preflight_stale` | Something changed between the check and the start: a start or stop, a save, a batch, an app restart, an option edit. | The agent runs the preflight again. |
| `/mcp` does not list `pa-lab` | The session was opened elsewhere, or not approved. | Open Claude Code in `~/Projects/Arduino-Source`, approve the server, run the smoke test above. |

## Glossary

| Word | Meaning |
|---|---|
| **Console B** | The lab Switch, its capture card and the lab app on profile `lab-B`. pa-lab knows no other console. |
| **Hold** | Your claim on console B, the file `Lab/hold`. Put on by your Start, Stop or button press in the app, or `hold.sh on`. Never expires; only `hold.sh off` removes it. |
| **Lease** | Who may act on console B now: `free`, `user` (you, from the hold), `agent_idle` (an agent holds it, with a time limit) or `agent_running` (an agent's run is on). An agent's lease ends the moment you take the console. |
| **Blocked** | The list of reasons, in plain words, why an agent may not act right now. |
| **Start check** (precheck) | One named thing a program needs before it starts, with a "needs" sentence: the game is open, a party slot is free, the save is the right one. A check may open a menu or go to HOME and back, but never resets, saves or changes an option. |
| **Preflight** | Running a program's start checks with the options an agent wants, without starting it, then putting your options back. A pass gives a token that allows one start within two minutes. |
| **Save fact** | A record of what a reload would give: the game and the place (a checkpoint's start, or a named place). `known`, `stale` (it may have changed; says why), `unknown` or `no_save`. Designed. |
| **Request record** | The one stored answer to one agent request. Sending the same request again returns it and does nothing else. |
| **Refusal** | An answer that says no, with a code (`console_held_by_user`, `precheck_failed`, ...) and a sentence. |
| **Read-only connection** | The `.mcp.json` link in your sessions. It reads files and nothing else. |
| **`--actions` mode** | The HTTP server mode that serves leases and actions. Built, not used yet. |
