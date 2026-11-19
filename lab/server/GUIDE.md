# pa-lab: a guide for the person running the lab

pa-lab is a small program on your Mac that sits between Claude and lab console B (the lab
Switch, the SerialPrograms lab app on profile `lab-B`, and its log). Claude asks pa-lab what the
console is doing; later it will ask pa-lab to start and stop programs. pa-lab answers with facts,
or refuses and says why.

This guide is organised by what you do. Each section says in one line whether it works today.
How it is built, the plan and the PRs are in the developer doc, [README.md](README.md).

Checked against the code on `agents` and the lab build on console B on 2026-11-18.

## Contents

- [Set it up](#set-it-up)
- [Check console B](#check-console-b)
- [Hold the console](#hold-the-console)
- [Have an agent run a program](#have-an-agent-run-a-program)
- [Know which save is on the console](#know-which-save-is-on-the-console)
- [Troubleshooting](#troubleshooting)
- [Glossary](#glossary)

## Set it up

pa-lab is already set up for Claude Code sessions opened in `~/Projects/Arduino-Source`; the
local file `~/Projects/Arduino-Source/.mcp.json` (not in git) starts it.

1. Open Claude Code in `~/Projects/Arduino-Source`.
2. The first time, approve the project's MCP server `pa-lab`.
3. Check with `/mcp`: `pa-lab` is connected, with one tool, `console_status`.
4. Ask: *"what's console B doing?"*

It runs the code in `~/Projects/Arduino-Source-agents/lab/server` with the Python in
`~/.venvs/lab-claude`. Pull that checkout to get fixes, then restart the Claude session. If it
does not show up, run the smoke test, which starts the server the same way and prints one answer:

```sh
~/.venvs/lab-claude/bin/python ~/Projects/Arduino-Source-agents/lab/server/smoke_readonly.py
```

This connection only reads. It cannot press a button, start or stop anything.

## Check console B

**Works today.**

Ask Claude *"what's console B doing?"*, *"did 03.11 pass?"* or *"who holds B?"*. You get the
app's build, the selected program and its state, controller, video, who holds the console, the
last checkpoint lines, the last button press and the newest failure frame. Nothing to do on your
side. Real output and how to read each field: [guide/status.md](guide/status.md).

## Hold the console

**Works today.**

Click Start or Stop in the lab app, or press a button through the app window, and console B is
yours: the app, the lab scripts and keep-awake keep off it until you give it back. `hold.sh on`
takes it without touching the game; `hold.sh off` gives it back. It never expires. Details:
[guide/hold.md](guide/hold.md).

## Have an agent run a program

**Not yet.** The code and its unit tests are merged but have never been used end to end; the
server mode that serves it is not configured; the console-B soak has not been run; and no program
reports its start checks yet, so nothing can be started this way today. Today agents use the lab
scripts (`labctl.sh`).

How it will work: you ask *"run FRLG 13.01 to 13.02 on B"*. Before Start, the server checks what it
can see from outside the game, without pressing anything (app up, right program and options, you
don't hold the console, controller ready, video live, the right save). Then it clicks Start. The
program's first step is its own start checks, which may press buttons (for example opening the
party to count free slots); a failed check ends the run with a named result such as
`start_check_failed: free_party_slots`. Click Stop at any time to take over. Details:
[guide/actions.md](guide/actions.md).

## Know which save is on the console

**Not yet.** Designed, no code: the server always says the save is `unknown`. Today, check the
save yourself before a checkpoint run.

How it will work: when a program saves, or the game autosaves where the plan expects, the server
records where; anything that could save without telling it (you playing, a button batch, an app
restart) marks the record stale, and a run from the wrong save is refused before Start. Details:
[guide/saves.md](guide/saves.md).

## Troubleshooting

| You see | What it means | What to do |
|---|---|---|
| `app` is `null`; `blocked` has `app_unavailable` | The lab app is closed or crashed. The status shows the last state it wrote. | Start the lab app the usual way (`labrun.sh`, never the bare binary). |
| `availability` `unavailable`, reason `stale_heartbeat` | The app is up but has not written its state for 10 s: busy or hung. | Ask again in a few seconds; if it stays, look at the app window. |
| `video_stale` or `video_bad_signal` | No camera frames, or a black or wrong picture. | Check the capture card cable and the app's video source; do not run `ffmpeg` on the card. |
| `console_held_by_user` and `hold` names you | You clicked Start or Stop, pressed in the app, or ran `hold.sh on`. It does not expire. | When you are done: `hold.sh off`. |
| `hold` with owner `unknown`, reason `unreadable` | `Lab/hold` exists but cannot be read; it counts as held, on purpose. | `hold.sh off`, which removes the file. |
| `program_running`, or a program stuck in `STOPPING` | A program is running, or slow to stop. | Wait, or click Stop. |
| `console://B/save` says `unknown` | Normal today: nothing records saves yet. | Check the save yourself before a checkpoint run. |
| `/mcp` does not list `pa-lab` | The session was opened elsewhere, or not approved. | Open Claude Code in `~/Projects/Arduino-Source`, approve it, run the smoke test above. |

## Glossary

| Word | Meaning |
|---|---|
| **Console B** | The lab Switch, its capture card and the lab app on profile `lab-B`. pa-lab knows no other console; console A is never touched. |
| **Hold** | Your claim on console B, the file `Lab/hold`. Put on by your Start, Stop or button press in the app, or `hold.sh on`; only `hold.sh off` removes it. |
| **Lease** | Who may act on console B now: `free`, `user` (you, from the hold), or an agent. An agent's lease ends the moment you take the console. |
| **Blocked** | The list of reasons, one sentence each, why an agent may not act right now. |
| **Preflight** | The server's checks right before it clicks Start. It presses nothing and checks only what it can see from outside the game. Not yet in use. |
| **Start check** | A check a program runs as its first step after Start, in the game: free party slots, no black border, the right game screen. A failure ends the run with a named result. No program reports these yet. |
| **Save record** (save fact) | What a reload would give: the game and the place. `known`, `stale` (may have changed; says why), `unknown` or `no_save`. Designed. |
| **Refusal** | An answer that says no, with a code (`console_held_by_user`, ...) and a sentence. |
