# Agent integration: one lab CLI, MCP later

Status: proposal (2026-11-01, revised the same day: CLI first, MCP an optional last
step). It builds on two things:
- the lab channel: fork #107, `SerialPrograms/Source/CommonFramework/Windows/Lab.md`;
- run records: fork #116, `docs/design/Lab-RunRecords.md` on `prototype/program-session-agent`.

Nothing here is implemented except what those two already have.

## Requirement

Coding agents (Claude Code, Codex) already develop and test programs here. They do it
through shell scripts around the lab channel: `labctl.sh`, `cfrun.sh` and `lablock.sh` in
`devtools/lab/`. That works, but every agent has to rediscover the same things:
- the file protocol;
- which program identifiers and option keys exist;
- how to tell a battle screen from the overworld;
- how to save evidence;
- the safety rules, such as never pressing keys while a program runs, never touching the
  user's console, and never starting a new game over the only save.

Agents end up filtering logs by date with `grep`, copying screenshots into notes by hand,
and hitting the same traps again (a missed `owner` line, a stale build, a batch with a
typo).

The goal is one documented, safe interface for running, observing and developing any
program on a real console. It returns JSON, and every agent host uses it the same way.

Users of this interface:
- **An agent developing a program.** The loop is edit → build → test offline → run on the
  console → read the evidence → fix.
- **An agent running or monitoring a program for the user.** Start a farm, watch it, and
  report when it stops.
- **The user.** They run the same commands, and see the evidence the agent saw: run
  records, screenshots, route pictures.

## Scope

In scope:
- **One lab CLI, `devtools/lab/lab.py`.** Python 3.9+, standard library only. JSON on
  stdout, a non-zero exit code on any refusal or failure.
  - It holds every guard.
  - `labctl.sh` and `cfrun.sh` become thin callers of it, kept for existing notes and skills.
  - The agent skills (`console-iteration`, `bdsp-autostory-test`, ...) document `lab.py`
    commands instead of raw request files.
- **Small app-side additions that make the CLI typed,** each its own PR: an option schema
  command, detector descriptions, structured run events (#116's schema), and per-program
  agent notes.
- **Optionally, last:** a thin MCP server over the CLI (see "MCP, later").

Not in scope:
- **Remote or networked control.** The CLI runs on the machine with the apps and talks
  through the lab folders.
- **A general "do anything" tool.** There is no raw shell passthrough, and no file writes
  outside the lab folders and the audit log.
- **GitHub or upstream actions.** Agents keep their own `gh` rules.
- **Changing how programs work.** Programs never depend on the CLI. With no `Lab/` folder,
  the app behaves exactly as today.
- **Video streaming.** Screenshots and the existing `history` clips only.

## High-level design

```
 agent host (Claude Code, Codex, a shell)       optional, later: MCP client
        │  runs                                         │
        ▼                                               ▼
 devtools/lab/lab.py  ◄──────────────────────── pa-lab MCP server (thin: one tool = one
   ├─ console | program | runs | logs | dev            lab.py call, no logic of its own)
   ├─ guards: consoles.json, lock + owner, confirms,
   │          own-process quit, build freshness, audit log
   └─ JSON out
        │  files: Lab/inbox → Lab/outbox, Lab/state, Lab/runs/<id>/
        ▼
 SerialPrograms app (one per console, --profile NAME)
   └─ lab channel (#107) + run recorder (#116)
```

**Why a CLI over files:**
- The lab channel already gives one request file per command batch, journals and a state
  file, with app-side checks (refusing presses while running, the owner token).
- The CLI adds typing, discovery and policy without growing the app.
- Any agent with a shell can use it, and so can the user, by hand. A crash on either side
  leaves plain files behind to inspect.

### Commands

```
lab.py console list|state|lock|release|shot|detect|wait-for|batch   --console B
lab.py program list|select|options|set|start|stop|wait              --console B
lab.py runs    list|show|events|report|pictures                     [RUN]
lab.py logs    tail|search                                          --run RUN | --since T
lab.py dev     build|test|deploy|launch|quit                        --slot lab-frlg
```

Examples:

```sh
$ lab.py console state --console B
{"console": "B", "state": "STOPPED", "program": "NintendoSwitch:CommandFileRunner",
 "owner": "", "build": "frlg-m28", "binary_mtime": "2026-11-01T11:39"}

$ lab.py console batch --console B -- "dpad UP 2600 0" "wait 2500" "press B" \
      "wait_for frlg_battle_menu 3500" "shot end"
{"ok": true, "lines": [{"n": 1, "ok": true, "ms": [0, 2602]}, ...,
 {"n": 4, "ok": true, "out": "timeout"}], "images": [".../outbox/req_end.png"]}

$ lab.py program set --console B START_CHECKPOINT='"8"' NEW_GAME_FROM_TITLE=true
{"ok": false, "refused": "NEW_GAME_FROM_TITLE overwrites the save on console B; pass --confirm 'overwrite save'"}

$ lab.py runs report latest --program PokemonFRLG:AutoStory > notes/run.md
```

### Example user journey: develop a fix and verify it

The user asks: "FRLG AutoStory fails at the Viridian Forest gate; fix it."

1. **Look at the evidence.** The agent runs `lab.py runs list --program
   PokemonFRLG:AutoStory --status failed`, then `lab.py runs show RUN`. It gets the
   checkpoint 00.08, the step `forest_gate`, the error and the failure frame. `lab.py runs
   pictures RUN` draws the walk (#125), and shows the player a tile off after a battle.
2. **Change code and test offline.** It edits code, then runs `lab.py dev build --slot
   lab-frlg` and `lab.py dev test --filter PokemonFRLG`. Both wrap the existing ninja and
   unit-test commands.
3. **Deploy.** `lab.py dev deploy --slot lab-frlg --console B` copies the build and records
   its mtime. Then `lab.py dev launch --console B`.
4. **Run.** It runs `lab.py console lock --console B`, `lab.py program select
   PokemonFRLG:AutoStory` and `lab.py program set START_CHECKPOINT=...`. The options are
   checked against the program's schema. Then `lab.py program start`. A start is refused if
   the deployed binary is older than the last build of that slot.
5. **Watch.** `lab.py program wait --until stopped --timeout 1800` prints new run events as
   they arrive (`checkpoint 00.08 start`, `battle`, `leg`), not raw log lines.
6. **Report.** `lab.py runs report latest` writes Markdown with the outcome, build, options,
   checkpoint table and failure frames. The agent gives it to the user, then runs `lab.py
   console release`.

### Example user journey: monitor a farm for the user

The user asks: "Start the BBQ farmer on console B and tell me if it stops."

The agent runs:
1. `lab.py console lock`, then `lab.py program select PokemonSV:BBQSoloFarmer`;
2. `lab.py program start`;
3. `lab.py program wait --until stopped`.

When the program stops, it sends the user `lab.py runs show latest` and the last frame.
Presses are refused while the farmer runs, by the CLI and again by the app.

## Guards (all in lab.py)

Rules live in `consoles.json` in the user's profile folder; an example sits in the repo.
The CLI reads it and has no command that changes it.

```json
{
  "A": {"profile": null, "access": "none", "why": "the user's console"},
  "B": {"profile": "lab-B", "access": "full",
        "confirm": {"NEW_GAME_FROM_TITLE": "overwrite save", "NEW_GAME_INTRO": "overwrite save",
                    "close_game": "close the game"}}
}
```

Enforcement:
- **Allowlist.** Consoles with `access: none` are not listed and every command on them is
  refused. `read` allows `state`, `shot`, `detect` and `wait-for` only.
- **Lock and owner.** Every write takes the console lock (the `lablock.sh` semantics, moved
  into the CLI) and puts the session's owner token on each request. Another session's
  lock is reported with its holder, never broken.
- **Never press while running.** Checked from `Lab/state` before every batch; the app
  enforces it again.
- **Confirmations.** Listed options and commands need `--confirm '<phrase>'` naming what is
  lost.
- **Own processes only.** `dev quit` refuses an app the CLI didn't launch in this session
  (pid file), or an app running a program it didn't start.
- **Build freshness.** `program start` refuses a deployed binary older than the slot's last
  `dev build`, unless `--allow-stale`.
- **Audit.** Every command, its arguments and its result summary are appended to
  `Lab/audit.jsonl` in the profile.

## Major new components

1. **`devtools/lab/lab.py`: the CLI.** Five subcommand modules, a request writer (write
   beside `inbox`, then move into it), a journal parser, the guards and the audit log.
   - About 700 lines.
   - Unit tests against a fake lab folder: a fixture "app" loop that consumes requests and
     writes journals and state.
   - `runs` is #116's proposed `runs.py` (list, show, report), folded in as the `runs`
     subcommand rather than a second script.
2. **Option schema (app side).** A lab command `schema` writes the selected program's
   options as JSON: key, type, enum labels, default, and an optional "danger" note the
   program sets on an option.
   - It reuses the config option tree that already writes the settings file. About 150
     lines plus tests.
3. **Detector descriptions (app side).** Each detector in the lab's list (#107) gets a
   one-line description; `help detectors` prints them.
4. **Run events: one format, defined once, in #116.** `events.jsonl` objects always carry
   `time` and `kind`; the rest is optional (`checkpoint`, `step`, `attempt`, `text`,
   `pose`, `frame`, `output`).
   - Kinds: `error`, `probe`, `checkpoint`, `step`, `attempt`, `route_plan`, `leg`,
     `battle`, `screenshot`. `pose` is the string `"<map> (x, y) facing D[, certainty]"`.
   - This design adds no kinds. Programs send events through `run_recorder().event()`, and
     story runners and walkers through the `SegmentHooks.event` callback that #116 proposes.
   - The CLI, #125's route renderer, and any later MCP wrapper read only this format.
5. **Per-program agent notes.** `<Program>.agent.md` next to a program, covering:
   - which detectors to use and the safe states to stop in;
   - known traps, such as "the battle menu shows only after the appearance text is
     dismissed", measured on console B on 2026-11-01;
   - the program's dangerous options.

   `lab.py program notes ID` prints the file, and the skills point to it.

## Developing new programs with it

The same loop works for a program that doesn't exist yet:

1. **Explore by hand.** Use `lab.py console batch` with presses, `shot` and `detect` to
   learn the screens. Saved frames become detector test data.
2. **Write detectors and their screenshot tests.** Run them offline with `lab.py dev test`.
3. **Write the program with an injectable game-steps interface,** as FRLG and BDSP
   AutoStory do with `StoryStepExecutor`, so a simulation test can run it against a fake
   game.
4. **Run it on the console** with `lab.py program`, and read the run record with `lab.py
   runs`.

A program gets better agent support by:
- emitting run events;
- marking dangerous options in its schema;
- shipping an `.agent.md`.

## MCP, later

A shell agent can always call `lab.py`, or the raw request files, directly, so an MCP
server adds no enforcement. The guards only hold because they live in the one CLI every
path goes through, and in the app.

What MCP would add is **discovery**, meaning tools and resources a client lists without
reading docs, and access for **clients without a shell**. So it comes last, and only when
one of those is needed: a non-shell client we want to support, or upstream asking for it.

When it comes, it's a thin server:
- each tool runs one `lab.py` command and returns its JSON;
- resources map to `lab.py program list|options`, the detector descriptions and `runs show`;
- no policy, state or protocol code of its own.

## Milestones and work plan

| PR | Content | Size | Tests |
|---|---|---|---|
| 1 | This design doc | doc | review |
| 2 | `lab.py`: console and program subcommands, the guards, `consoles.json`, the audit log. `labctl.sh` and `cfrun.sh` call it. | ~500 + tests | fake-lab unit tests; one scripted session on console B |
| 3 | `lab.py runs` and `logs` on #116's run records (folds in #116's proposed `runs.py`) | ~250 | unit tests on fixture run folders |
| 4 | App: `schema` lab command, danger notes on options, detector descriptions | ~200 | C++ unit tests on two programs' option trees |
| 5 | `lab.py dev`: build, test, deploy, launch and quit, with the freshness check | ~250 | unit tests with fake commands |
| 6 | Skills and `.agent.md` notes for FRLG AutoStory, BDSP AutoStory and the BBQ farmer | docs | a skill-driven console run |
| 7 (optional) | Thin MCP server over `lab.py` | ~150 | each tool's output matches the CLI's |

The run events themselves (the hooks in story runners and walkers, and #125 reading
`events.jsonl`) are #116's milestones; PRs 3 and 6 depend on them.

Order: PR 2 is useful on its own, because today's labctl and batch flows become typed and
guarded. PRs 3–5 make it observable and discoverable. PR 6 makes agents faster on the
programs we work on most. PR 7 waits for a reason.

Testing story:
- **Every CLI command is unit-tested** against a fake lab folder.
- **The app-side commands** have C++ unit tests in the existing runner.
- **One scripted console-B session per milestone** checks the end-to-end path, and leaves a
  run record as evidence.

## Open questions

- **`labctl.sh` and `cfrun.sh`:** keep them as thin wrappers (existing notes and skills call
  them), or retire them after the skills move to `lab.py`? The proposal is to keep them for
  one release, then retire them.
- **Where `consoles.json` lives:** the proposal is the profile folder, so each machine keeps
  its own rules, with an example in the repo.
- **Upstream or fork-only?** The lab channel and the CLI are developer tooling. Upstream may
  want them behind developer mode, or not at all.
- **Whether and when to add the MCP wrapper:** only on a concrete need, as above.
