# ProgramSession agent interface — throwaway proof of concept

Question: can an agent list existing sessions and request start/stop by explicit
session ID, receiving the real rejection reason and distinguishing acceptance
from completion, without owning a special automation program?

This prototype adds four JSON methods and a lab-channel adapter. It reuses
`ProgramTracker`; it does not introduce a second session registry. The same
dispatcher can be driven in a hardware-free terminal demo. Delete or absorb
this prototype once the interface decision is made.

For the proposed next step, see [the run results and failure evidence design](DESIGN.md).
It adds per-run outcomes to Lab's shared backend; it is not implemented here.

## Try the terminal demo

From the repo root, with a C++23 compiler:

```sh
sh devtools/prototypes/session-agent/run-demo.sh
```

This compiles only the small native demo, into a temporary directory. It does
not build SerialPrograms, connect hardware, or control a running app. Session 1
can start; session 2 has a simulated configuration error. Enter one request per line:

```json
{"method":"sessions.list"}
{"method":"sessions.start","session_id":2}
{"method":"sessions.start","session_id":1}
{"method":"sessions.status","session_id":1}
{"method":"sessions.stop","session_id":1}
{"method":"sessions.status","session_id":1}
```

The rejected start reports `ok:false`. The accepted stop leaves session 1 in
`STOPPING`; the demo-only command `finish` simulates its eventual completion.
`unload` removes session 1, so requests using that ID fail. `quit` exits;
`--plain` disables terminal clearing for a piped walkthrough. Full simulated
state and the last reply are displayed after each action.

## Live app adapter

A build of this branch accepts `session_agent <JSON>` through the existing
lab channel. Select and configure a suitable program in the app first. Within
the lab's existing console lock and owner session:

```sh
printf '%s\n' 'session_agent {"method":"sessions.list"}' |
    sh devtools/lab/labctl.sh send sessions-list
```

The journal contains a JSON result, followed by its normal `ok`/`end` lines.
Take the `session_id` from that result, then use it explicitly:

```sh
printf '%s\n' 'session_agent {"method":"sessions.status","session_id":1}' |
    sh devtools/lab/labctl.sh send session-status
```

Use `sessions.start`/`sessions.stop` with the same shape only for the program
under test: these invoke its actual lifecycle and may produce game/save effects.
When the channel is claimed, all prototype requests require `LAB_OWNER`,
including observations. A failed request emits both the JSON error and the
channel's `error` line, so the batch stops and `labctl.sh` exits unsuccessfully.

## Reply contract

- `sessions.list`: current live session snapshots, each with `session_id`,
  `program` (descriptor identifier), `state`, `stats`, and `console_ids`.
- `sessions.status`: one snapshot or `session_not_found`.
- `sessions.start` / `sessions.stop`: `operation_rejected` with the session's
  actual error, or `{"ok":true,"result":{"session_id":1,"accepted":true}}`.
- Poll status to observe the outcome; `accepted` is not completion. A program
  may also finish between polls. There is no per-run history in this prototype.
- IDs belong to this app process. Rediscover after restart. IDs 0, UINT64_MAX,
  negative numbers, fractions and booleans are rejected; no implicit selection.

## What this proves, and what it does not

`SessionAgentPrototype.h` contains the transport-independent dispatcher;
`SessionAgentPrototype.cpp` binds it to the live tracker. `PrototypeDemo.cpp`
supplies simulated bindings and is excluded from the app build.

One prerequisite fixes an existing information loss: `TrackableProgram`'s
`async_start`/`async_stop` now return their error strings, and the tracker
propagates them. `ProgramSession` already supplies those strings. Existing
integration callers can now see a rejected operation too.

This draft is not an MCP server or JSON-RPC implementation. It does not add
program creation/selection, option editing, direct controller inputs, run IDs,
event subscriptions, an operation queue, or a new lifetime/ownership scheme.
The live adapter uses the existing tracker's locking and destruction contract;
concurrent session destruction still needs an explicit audit before production.
Multiple clients are coordinated only by the existing lab-channel mechanism.

The terminal walkthrough demonstrates that the four-method interface can expose
rejection, accepted-but-still-stopping, and stale IDs. The remaining experiment
is to exercise the live adapter with a deliberately invalid configuration and
a suitable short program, then decide whether to add per-run results and events
before attaching MCP. No live-console validation is claimed for this draft.

## Concrete target: investigating the Sandgem failure

In BDSP AutoStory run r109, the player reached Sandgem correctly, but OCR read
the location banner as `andgemltown`, so the location check failed. See the
[AutoStory lessons, section 13](https://github.com/dayfine/Arduino-Source/blob/bdsp/floaroma-mvp/docs/design/AutoStory-Lessons.md#13-floaroma-mvp-from-a-new-game-and-the-lab-around-it-2026-10-29-r107-to-r109).

Lab can already start the program and report that it stopped. The agent must
then find the corresponding error and screenshot, associate them with this
attempt, and distinguish the original failure from later retries. The current
four-method prototype does not remove that work.

The useful next slice would support this investigation:

1. Start the configured checkpoint and receive a unique run ID.
2. Wait for that run's outcome, rather than polling whether the session is stopped.
3. Retrieve its error and failure evidence together.
4. Inspect the frame and log: the location is visibly correct, but its OCR
   comparison rejected it.
5. Investigate the matcher with saved-image cases, including a different town
   that must still be rejected, then rerun the checkpoint under a new run ID.

An illustrative future result, **not implemented by this prototype**, is:

```json
{
  "run_id": "run-109",
  "outcome": "failed",
  "checkpoint": "00.04",
  "error": "Location verification failed",
  "failure_screenshot": "...",
  "log_excerpt": "..."
}
```

The error text above is illustrative too. The interface should preserve the
actual program error and capture the failure evidence before a retry changes
the screen. The matcher fix remains in AutoStory; the interface makes its
investigation dependable and automatable. Run identity, outcome and error
capture belong near `ProgramSession`. Checkpoint details require connecting
the program's progress reporting to that run.

Other recorded failures show the boundary:

| Failure | What an agent interface could contribute | What the program still must do |
|---|---|---|
| r84: selecting Water Gun at 0 PP for four minutes | Progress reports and recent captures let the agent recognize repetition and stop the identified run. | Read PP, skip exhausted moves and bound retries. A generic session cannot infer PP without observation or program reporting. |
| r105/r106: reported a pass after talking to the rock beside Roark | Expose a story-effect verification failure, once the program checks it. | Verify that Roark actually left. Lifecycle completion alone cannot establish story success. |
| r109a: the Start click missed after the window resized | Lab's existing explicit start/stop commands already address this. | No new benefit should be attributed to this prototype for removing GUI clicks. |

The intended unlock is an agent that can run a bounded automation experiment,
identify its first failure, inspect the associated evidence, probe the cause,
change the code and repeat. Structured start/stop alone is groundwork, not
that outcome. A concrete next proof should demonstrate: **a checkpoint fails;
one request returns that run's outcome, error and failure screenshot**. MCP
can expose the same operations afterward; changing transport alone adds little.
