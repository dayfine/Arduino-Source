# pa-lab

**Let Claude see lab console B, and later run programs on it, without ever getting in your way.**

pa-lab is a small server on your Mac between Claude and console B (the lab Switch, its capture
card and the lab app on profile `lab-B`). Claude asks it questions; it answers with facts, or
refuses and says why. It knows no other console.

- [Available tools](#available-tools)
- [How to set up](#how-to-set-up)
- [FAQ](#faq)

## Available tools

Each entry says whether you can use it today: **Available**, **Built, needs the HTTP mode (not
set up yet)**, or **Not available yet**.

### Read the console

#### `console_status`

What console B is doing: app and build, program and state, controller, video, who holds it, last
checkpoint lines, last press, newest failure frame. **Available.** Reads files only.

#### `console://B/state`

The app's live state, with `blocked`: one sentence per reason an agent may not act now. Empty
means free. **Available.**

#### `shot`

A fresh console screenshot, taken after the call. Refused when the video is stale or black.
**Built, needs the HTTP mode (not set up yet).**

#### `game_open`

Whether a game is in front, or why not: HOME, user select, black screen, no video.
**Built, needs the HTTP mode (not set up yet).**

#### `ensure_game`

Checks that the expected game is in front; never navigates. Today it never verifies: the app
cannot name the game yet. **Built, needs the HTTP mode (not set up yet).**

#### `describe_program`

The selected program's options: keys, types, defaults, allowed values, and its start checks.
Call before `run_program`. **Built, needs the HTTP mode (not set up yet).**

### Your hold

#### Start, Stop or a button press in the lab app

Takes console B for you automatically. Scripts, agents and keep-awake keep off until you run
`hold.sh off`. It never expires. **Available.**

#### `hold.sh on` / `hold.sh off` / `hold.sh status`

Take the console without touching the game, give it back, or see who holds it.
Run `~/BDSPLab/hold.sh`. **Available.**

#### `acquire_console` / `renew_console` / `release_console`

An agent's lease on console B, needed for anything that presses or starts. Refused while you hold
the console. **Built, needs the HTTP mode (not set up yet).**

#### `next_events` / `request_status`

Follow a pending request, or wait for the next event (run started, run ended, you took over).
**Built, needs the HTTP mode (not set up yet).**

### Run programs

#### `run_program`

Checks the console and the options, then starts the selected program with them, in one call.
Presses nothing when it refuses. Needs the lease. The program then runs its own start checks. A
failed check ends the run with a named result (`start_check_failed`). The BDSP AutoStory pilot
build reports those checks. Use HTTP mode with `--actions`.

#### Follow a run to its result

With the updated pa-lab server and a lab build that publishes results, read the latest cursor
from `next_events` before Start. Keep the `request_id` you send to `run_program`, then poll
`next_events`, advancing `after_seq` to each response's `next_after_seq`. Look for a terminal
event with that original `request_id`, even when a separate request stops the run.

| Event | What happened |
|---|---|
| `program_finished`, verdict `passed` | The program completed. |
| `program_finished`, verdict `stopped`, reason `cancelled` | The run was cancelled. |
| `program_failed`, verdict `failed` | The run failed. Read the checkpoint and exception message when present. |
| `program_failed`, verdict `start_check_failed` | The program started and failed one of its own checks. Read the check name and message. |

A completed `run_program` request confirms Start. A `stopped` event reports the transition.
Look for the terminal event to learn how the run ended. A refusal before Start is returned by
`run_program` itself.

For more context, use `console_status` to read the latest log lines and see whether a failure
frame exists. `shot` returns a fresh image of the current screen. An earlier failure frame or
video clip may be unavailable. Structured results passed offline tests with the BDSP AutoStory
pilot build and are awaiting live Console B verification.

#### `stop_program`

Clicks Stop once. Asking again with the same request never stops twice. **Built, needs the HTTP
mode with actions (not set up yet).**

#### `cfr_batch`

Debugging only: up to 200 button and screenshot lines, at most 60 s, with no program running.
Marks the save stale. **Built, needs the HTTP mode with actions (not set up yet).**

### Saves

#### `console://B/save`

Which save is on the console. Today it always says `unknown`: nothing records saves yet. Check the
save yourself before a checkpoint run. **Available.**

### Coming

- **Start checks in programs**: a program's first step checks free party slots, black border and
  game screen, and ends the run with a named result such as `start_check_failed`.
- **Save records**: `console://B/save` says `known`, `stale` (with why) or `no_save`.
- **Save check before a run**: a run from the wrong save is refused before Start (`save_mismatch`).
- **Run records**: one record per run: options, start save, how it ended, checkpoint lines,
  failure frame.

## How to set up

### Claude Code (read-only, Available)

Put this in `.mcp.json` at the root of `~/Projects/Arduino-Source` (local, not in git), open
Claude Code there, approve `pa-lab` once, and check `/mcp` lists `console_status`.

```json
{
  "mcpServers": {
    "pa-lab": {
      "type": "stdio",
      "command": "<HOME>/.venvs/lab-claude/bin/python",
      "args": ["-m", "pa_lab", "serve-stdio",
               "--profile-dir", "<HOME>/Library/Application Support/SerialPrograms/Profiles/lab-B",
               "--keepawake-log", "<HOME>/BDSPLab/keepawake.log"],
      "env": {"PYTHONPATH": "<HOME>/Projects/Arduino-Source-agents/lab/server"}
    }
  }
}
```

If it does not connect, run the smoke test; it starts the server the same way and prints one
answer:

```sh
~/.venvs/lab-claude/bin/python ~/Projects/Arduino-Source-agents/lab/server/smoke_readonly.py
```

### HTTP mode (Built, not set up yet)

Serves `shot`, `game_open`, `ensure_game`, `describe_program` and the lease tools; add `--actions`
for the run tools. Make a token, write the config, start the server, add it to Claude Code.

```sh
cd ~/Projects/Arduino-Source-agents/lab/server
python3 -m pa_lab init-token ~/.config/pa-lab/token
python3 -m pa_lab serve --config ~/.config/pa-lab/server.json            # add --actions to run programs
claude mcp add --transport http pa-lab-http http://127.0.0.1:8765/mcp \
  --header "Authorization: Bearer <TOKEN>"
```

```json
{"schema": 1, "host": "127.0.0.1", "port": 8765, "token_file": "~/.config/pa-lab/token",
 "consoles": {"B": {"lab_dir": "<HOME>/Library/Application Support/SerialPrograms/Profiles/lab-B/Lab",
                    "profile": "lab-B"}}}
```

## FAQ

### What can it do today?

Read only: `console_status`, `console://B/state`, `console://B/save`, and your hold. Agents still
start and stop runs with the lab scripts (`labctl.sh`).

### Can an agent ever touch console A?

No. pa-lab accepts only console `B`; its config must list exactly `B`, and it never reads another
profile.

### What happens when I click Start in the app?

Console B becomes yours. Every press, start and stop from scripts, agents and keep-awake is
refused, and an agent's lease ends at once. Reading still works. Your own run is never refused.

### How do I take the console back?

- **Just use it**: click Start or Stop, or press a button in the app window.
- **Without touching the game**: `~/BDSPLab/hold.sh on`.
- **When you are done**: `~/BDSPLab/hold.sh off`. Nothing else ends your hold.

### Why was my agent's run refused?

The refusal names the reason in one sentence. The common ones:

- **`console_held_by_user`**: you hold the console. Run `hold.sh off` when done.
- **`program_running`**: a program runs or is still stopping. Wait, or click Stop.
- **`app_unavailable`**: the lab app is closed, hung (no state for 10 s), or the video is stale.
  Start it with `labrun.sh`, never the bare binary.
- **`prechecks_unsupported`**: the program reports no start checks yet. Expected today.
- **`panel_not_program`** / **`invalid_arguments`**: wrong program selected, or an option it does
  not take. Check `describe_program`.

### `console_status` says `app` is `null`. What now?

The lab app is closed or crashed; the status shows the last state it wrote. Start it the usual
way (`labrun.sh`).

### `/mcp` does not list `pa-lab`.

Open Claude Code in `~/Projects/Arduino-Source`, approve the server, and run the smoke test above.

---

For developers: contracts, design and plan are in [README.md](README.md).
