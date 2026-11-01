# Agent integration: an MCP server for SerialPrograms

Status: proposal (2026-11-01). It builds on the lab channel (fork #107,
`SerialPrograms/Source/CommonFramework/Windows/Lab.md`) and on run records (fork #116).
Nothing here is implemented except what #107 already has.

## Requirement

Coding agents (Claude Code, Codex) already develop and test programs here. They do it
through shell scripts around the lab channel: `labctl.sh`, `cfrun.sh` and `lablock.sh`
(see `devtools/lab/`). That works, but every agent has to rediscover the same things:
- the file protocol;
- which program identifiers and option keys exist;
- how to tell a battle screen from the overworld;
- how to save evidence;
- the safety rules, such as never pressing keys while a program runs, never touching the
  user's console, and never starting a new game over the only save.

Agents end up filtering logs by date with `grep`, copying screenshots into notes by hand,
and hitting the same traps again (a missed `owner` line, a stale build, a batch with a
typo). The goal is one typed, documented and safe interface that any agent host can use,
to run, observe and develop any program on a real console, without reading the app's
source first.

Users of this interface:
- **An agent developing a program.** The loop is edit → build → test offline → run on the
  console → read the evidence → fix.
- **An agent running or monitoring a program for the user.** Start a farm, watch it, and
  report when it stops.
- **The user.** They see the same evidence the agent saw: run records, screenshots, route
  pictures.

## Scope

In scope:
- **An MCP server.** A separate process, speaking the Model Context Protocol over stdio, that
  wraps the lab channel of one or more running SerialPrograms apps.
- **Read access** to:
  - program metadata: the program list, option schemas, detectors;
  - run records (#116) and their screenshots;
  - logs, by run rather than by date.
- **Safety rules enforced in the server,** not left to prompt text: the console allowlist,
  locks, owner tokens, no presses while running, and confirmation for save-destroying
  options.
- **Thin development helpers:** build and test status, deploying a build to a lab slot,
  launching an app with a profile.
- **Small app-side additions** that make the interface typed: an option schema dump, a
  detector list with descriptions, and structured run events. Each is its own PR.

Not in scope:
- **Remote or networked control.** The server runs on the same machine as the apps and
  talks through files. A socket transport is possible later, but files keep the app
  unchanged and auditable.
- **A general "do anything" tool.** There is no raw shell, and no arbitrary file write
  outside the lab folders.
- **Upstream publishing.** No PR or GitHub actions; agents keep their own `gh` rules.
- **Changing how programs work.** Programs never depend on the server. With no lab folder,
  the app behaves exactly as today.
- **Video streaming.** Screenshots and the existing `history` clips only.

## High-level design

```
 agent host (Claude Code, Codex, ...)
        │  MCP over stdio
        ▼
 pa-lab MCP server  (devtools/mcp/, Python, standard library + the mcp package)
   ├─ consoles.json     which app profiles and consoles it may touch
   ├─ lock + owner      lablock.sh semantics, owner token per session
   ├─ tools / resources (below)
   └─ evidence index    run records, outbox pictures, logs by run
        │  files: Lab/inbox → Lab/outbox, Lab/state, Lab/runs/<id>/
        ▼
 SerialPrograms app (one per console, --profile NAME)
   └─ lab channel (#107) + run recorder (#116)
```

**Why a separate process over files:**
- The lab channel already gives one request file per command batch, journals and a state
  file, with app-side checks (refusing presses while running, the owner token).
- The server adds typing, discovery and policy without growing the app.
- A crash on either side leaves plain files behind to inspect.

### Example user journey: develop a fix and verify it

The user asks: "FRLG AutoStory fails at the Viridian Forest gate; fix it."

1. **Look at the evidence.** The agent calls `runs.list(program="PokemonFRLG:AutoStory",
   status="failed")`, then `runs.read(id)`. It gets the events (checkpoint 00.08, step
   `forest_gate`, the error) and the failure screenshot, and the route picture renders
   (#125). It sees that the player was a tile off after a battle.
2. **Change code and test offline.** It edits code, then calls `dev.build(target="lab-frlg")`
   and `dev.test(filter="PokemonFRLG")`. Both run the existing build and test commands and
   return structured results.
3. **Deploy.** It calls `dev.deploy(build="lab-frlg", slot="B")`, which copies the build and
   checks that the binary's mtime is newer than the source. Then
   `app.launch(console="B", profile="lab-B")`.
4. **Run.** It calls `console.lock("B")`, then `program.select("PokemonFRLG:AutoStory")` and
   `program.set_options({...})`. The server checks the option schema. It refuses
   `NEW_GAME_FROM_TITLE=true` without `confirm="overwrite save"`, unless the console's
   policy allows it. Then `program.start()`.
5. **Watch.** It calls `program.wait(until="stopped", timeout_s=1800, notify_every_s=300)`.
   Progress comes back as run events (`checkpoint 00.08 start`, `battle`, `leg`), not as
   raw log lines.
6. **Report.** It calls `runs.read(latest)` and gives the user the result with the record's
   pictures. Then it calls `console.release("B")`.

### Example user journey: monitor a farm for the user

The user asks: "Start the BBQ farmer on console B and tell me if it stops."

The agent calls:
1. `console.lock("B")`, then `program.select("PokemonSV:BBQSoloFarmer")`;
2. `program.set_options(preset="user-default")`, then `program.start()`;
3. `program.wait(until="stopped")`.

When the program stops, the agent returns `runs.read(latest).summary` and the last
screenshot. The agent never presses a button while the farmer runs: the server refuses it,
and so does the app.

### Example commands (MCP tool calls)

```jsonc
{"tool": "console.state", "args": {"console": "B"}}
→ {"state": "STOPPED", "program": "NintendoSwitch:CommandFileRunner", "owner": "", "app": "frlg-m28", "binary_mtime": "..."}

{"tool": "console.batch", "args": {"console": "B", "lines": ["dpad UP 2600 0", "wait 2500", "press B", "wait_for frlg_battle_menu 3500", "shot end"]}}
→ {"ok": true, "lines": [{"n": 1, "ok": true, "ms": [0, 2602]}, ..., {"n": 4, "out": "timeout"}], "images": ["outbox/req_end.png"]}

{"tool": "program.options", "args": {"program": "PokemonFRLG:AutoStory"}}
→ {"START_CHECKPOINT": {"type": "enum", "values": ["00.01 ...", "00.09 Pewter Gym ..."]}, "NEW_GAME_FROM_TITLE": {"type": "bool", "danger": "overwrites the save"}, ...}
```

## Tools and resources

Tools (verbs) are grouped by what they touch. Each returns structured JSON, and images
as MCP image content.

| Group | Tools | Notes |
|---|---|---|
| console | `list`, `state`, `lock`, `release`, `shot`, `detect`, `wait_for`, `batch` | `batch` takes command lines from the lab channel's list (`help`), validated before sending. Presses are refused while a program runs, as the app does. |
| program | `list`, `select`, `options`, `set_options`, `start`, `stop`, `wait` | `options` returns the schema. `set_options` checks values against it and the console's policy. `wait` blocks until a state is reached or a timeout passes, and returns new run events. |
| runs | `list`, `read`, `events`, `pictures` | Reads `Lab/runs/<id>/` (#116). `pictures` calls the #125 renderer on the run's route events. |
| logs | `tail`, `search` | By run id or time window. Never returns the whole log. |
| dev | `build`, `test`, `deploy`, `launch`, `quit` | Wrap the existing commands (`ninja -j4`, the unit test runner, `bdsp_deploy.sh`, `stage_launch.sh`). One build at a time. `quit` only closes an app this session launched. |

Resources (nouns the host can list and read):
- `pa://programs`: every program identifier, with title and description.
- `pa://programs/<id>/options`: the option schema.
- `pa://detectors`: every detector name, its game, and what it means.
- `pa://consoles`: the allowlist and each console's policy.
- `pa://runs/<id>`: one run record.
- `pa://docs/lab`: Lab.md.

## Safety model

Rules live in `devtools/mcp/consoles.json`, which the user owns. Agents can read it but
not change it through the server.

```json
{
  "A": {"profile": null, "access": "none", "why": "the user's console"},
  "B": {"profile": "lab-B", "access": "full",
        "confirm": ["NEW_GAME_FROM_TITLE", "NEW_GAME_INTRO", "close_game"]}
}
```

Enforcement:
- **Allowlist.** Consoles with `access: none` are invisible. `read` allows screenshots and
  state only.
- **Lock and owner.** Every write takes the console lock (`lablock.sh` semantics) and puts
  the session's owner token on each request. Another session's lock is reported with its
  holder, never broken.
- **Never press while running.** The server checks the state file before each batch, and
  the app enforces it as well.
- **Confirmations.** Listed options and commands need an explicit `confirm` argument
  naming what is lost, e.g. "overwrite save". A host can surface that to the user.
- **Own processes only.** `dev.quit` refuses an app the session did not launch, and an app
  that is running a program the session did not start.
- **Stale builds.** `program.start` refuses when the deployed binary is older than the
  last `dev.build` of that slot, unless `allow_stale=true`.
- **Audit.** Every tool call and its result summary is appended to
  `Lab/mcp-audit.jsonl` in the profile.

## Major new components

1. **`devtools/mcp/pa_lab_server.py`: the MCP server.** Python 3.9+, the standard library
   plus the `mcp` package.
   - It owns the tool table, the policy and the request writer: write beside `inbox`, then
     move into it, as `labctl.sh` does. It also parses journals into JSON.
   - About 600 lines, with unit tests against a fake lab folder: a directory plus a tiny
     simulated app loop that answers requests from fixtures.
2. **Option schema export (app side, small).** A lab command `schema` that writes the
   selected program's options as JSON: key, type, enum values, default, and a "danger"
   note the program sets.
   - It reuses the existing config option tree, which already serialises to JSON for the
     settings file. Only types and enum labels are new.
   - About 150 lines plus tests.
3. **Detector registry descriptions (app side, small).** The lab's detector list (#107)
   gets a one-line description per detector, and `help detectors` prints them.
4. **Run events (#116).** Structured events (`checkpoint`, `step`, `route_plan`, `leg`,
   `battle`, `failure`, `screenshot`), written with `run_recorder().event()` into
   `events.jsonl`.
   - Programs opt in. AutoStory first, since its story runner already logs these as text.
   - The server reads these events; it never parses free-text logs.
5. **Program skills and docs.** Per-game notes are exposed as resources, not code:
   - which detectors to use;
   - safe states to stop in;
   - known traps, such as "the battle menu appears only after the appearance text is
     dismissed" (measured 2026-11-01).
   - They live next to each program as `<Program>.agent.md`, and the server lists them.

## Developing new programs with it

The same loop works for a program that doesn't exist yet:

1. **Explore by hand.** Use `console.batch` with presses, `shot` and `detect` to learn the
   screens. Save the pictures as detector test data.
2. **Write detectors and their screenshot tests.** Run them offline with `dev.test`.
3. **Write the program with an injectable game-steps interface**, as FRLG and BDSP
   AutoStory do with `StoryStepExecutor`. A simulation test can then run it against a fake
   game without a console.
4. **Run on the console** through `program.*`, and read the run record.

The server adds nothing program-specific. A program gets better agent support by:
- emitting run events;
- declaring dangerous options;
- shipping an `.agent.md`.

## Milestones and work plan

| PR | Content | Size | Tests |
|---|---|---|---|
| 1 | This design doc | doc | review |
| 2 | `devtools/mcp`: the server with the console, program and logs tools over the existing lab channel, and the policy file | ~600 + tests | fake-lab unit tests; one manual session on console B |
| 3 | App: `schema` lab command and detector descriptions | ~200 | unit tests on two programs' option trees |
| 4 | Run events in the run recorder (#116 slice) and the `runs` tools | ~300 | recorder unit tests; an AutoStory run produces events |
| 5 | AutoStory emits checkpoint, step, leg and failure events; the `runs.pictures` tool calls #125 | ~250 | the simulation tests assert the events; one console run |
| 6 | The `dev` tools (build, test, deploy, launch, quit) with the stale-build check | ~250 | unit tests with fake commands |
| 7 | `.agent.md` for FRLG AutoStory, BDSP AutoStory and the BBQ farmer | docs | — |

Order: PR 2 is useful on its own, because today's batch and labctl flows become typed. PRs
3 and 4 make it discoverable and observable. PRs 5–7 make agents faster on the programs we
work on most.

Testing story:
- **Every tool is unit-tested** against a fake lab folder: a fixture app that consumes
  requests and writes journals.
- **The app-side commands** have C++ unit tests in the existing runner.
- **One scripted console session per milestone** checks the end-to-end path on console B,
  recorded as a run record.

## Open questions

- **Language:** Python (fast to write; agents already have it) or a C++ tool built with the
  app (one toolchain, no `mcp` dependency)? The proposal is Python, because the server never
  links app code.
- **Where `consoles.json` lives:** in the repo as an example, with the real one in the
  user's profile folder? The proposal is the profile folder, so each machine keeps its own
  rules.
- **Upstream or fork-only?** The lab channel and the server are developer tooling. Upstream
  may want them behind a developer-mode flag.
- **A socket transport,** so an agent can watch several consoles without polling: wait until
  the file transport limits something real.
