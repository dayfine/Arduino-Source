---
name: console-iteration
description: Develop and debug console automation through a focused loop - define an observable outcome, run, inspect evidence, test one hypothesis, change the cause, and validate. Applies to menus, detectors, battles, farming, navigation and timing-sensitive programs. Use when a console run fails or when developing a new behavior; game-specific examples state their assumptions.
---

# Console iteration: define, measure, change one thing

Use this loop for behavior that depends on a live console: menus, visual or
audio inference, battles, farming, navigation and timing-sensitive programs.
The loop is: **define the outcome, run, inspect the evidence, name the layer,
test a hypothesis, change the cause, validate.** Choose the smallest experiment
that can answer the question: a saved capture, unit test, CFR batch, focused
program run or human demonstration.

Examples below come partly from BDSP AutoStory. Tile movement, walls, map
banners and story checkpoints apply only where the game's controls and
observations support them. They are examples of the loop, not requirements
for every console program.

## How to use

This skill lives in the repo at `skills/console-iteration/SKILL.md`; install
it with `ln -s "$PWD/skills/console-iteration" ~/.claude/skills/console-iteration`
and it loads when a console run fails or a new behavior starts. Everything it
asks of the console goes through the lab channel (`Lab/` folder of the lab
app's profile) and the three scripts in `devtools/lab/`:

Example setup on macOS, using BDSP AutoStory as the program under test:

```sh
# Open a shell that holds the console lock for this session.
sh devtools/lab/lablock.sh -c B run console-iteration -- sh
export LAB_CONSOLE=B                                   # the lock's name
export LAB_PROFILE="$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B"
export LAB_OWNER=lab-B                                 # the channel's owner token
cd devtools/lab
sh labctl.sh state                                     # first: what runs? never stop a program to find out
sh labctl.sh owner claim lab-B                         # once per session
sh labctl.sh select PokemonBDSP:AutoStory              # the program under test
sh labctl.sh set START_SEGMENT '"0"'                   # one option, JSON value
sh labctl.sh start                                     # the run; then read the log and shots
# Wait for the run to finish; check `labctl.sh state` reports STOPPED
# before sending a batch that presses buttons.
if sh labctl.sh state | grep -qx 'state=STOPPED'; then
    printf '%s\n' "dpad UP" "shot after_up" "detect home" | sh cfrun.sh probe-up
fi
sh labctl.sh owner release
exit                                                  # release the session lock
```

`Lab.md` (path below) has every command and the file formats; the lab
README has the scripts' parameters and how to run a lab app beside the live
one.

The tools this loop leans on, by path:

- `SerialPrograms/Source/CommonFramework/Windows/Lab.md`: the lab channel,
  the `Lab/` folder through which a script selects a program, sets its
  options, starts and stops it, reads its state, and sends text batches of
  presses, screenshots and detector checks (the CFR commands) to the live
  console; the full command table lives there.
- `devtools/lab/README.md`: the scripts that wrap it (`lablock.sh`,
  `labctl.sh`, `cfrun.sh`) and the one-train rule.

## 1. Define a step, then code it

A step has **one outcome that can be checked**. Examples: the selected menu
option changed, an item count increased, a battle ended, a detector recognized
its target, or the player entered a map. Use screen, audio, controller or log
evidence appropriate to the claim. An input being sent proves only that it
was sent; confirm its effect where the next action depends on it.

- **State the starting conditions and success check.** Include the menu,
  inventory, party, position or timing state that matters to this behavior.
- **Keep the experiment small enough to explain a failure.** Split at
  observable boundaries; choose the number of actions for the task rather
  than imposing a fixed move count.
- **Establish the expected behavior first.** Use a CFR batch, saved capture,
  documented game data or a human demonstration. Record the actions and the
  observations that justify the implementation.
- **Identify the source of important constants.** Cite the measurement or
  trusted data; label estimates and their uncertainty. A map or decompilation
  can supply facts, while the target game still needs to confirm applicability.
- **Choose controls and checks for the required tolerance** (section 5).
  For example, a menu tap must avoid auto-repeat; an RNG sequence may require
  a continuous frame-timed action that interactive stepping cannot reproduce.

For AutoStory, a checkpoint can bound a step. A location banner can confirm
the map, but does not by itself establish the player's tile. A story event
is a useful checkpoint boundary only when its completion and save behavior
are known.

## 2. Observe

After a run, before anything else:

1. **Read the log tags** (step counters, leg lines, walk results, attempt
   failures). Build the timeline: what the code believed at each step.
2. **Inspect the capture and identify the actual state.** Which screen,
   menu, battle phase or location is visible? Use named evidence: prompt
   text, cursor, HP, inventory count or a landmark. If the screenshot is
   ambiguous, get another capture (`shot`), check a specific detector
   (`detect`), or inspect the recording before concluding anything.
3. **Compare belief and reality.** Identify the first disagreement. Menu
   example: the code assumed the confirmation prompt had closed, but it is
   still visible. Navigation example: it assumed town, but the banner shows
   a route.
4. **Check the claimed outcome**, including negative cases. A detector may
   correctly report absence, and waiting may correctly leave the screen
   unchanged. Use the success condition defined for this step.
5. **Check persistent effects when relevant**: saves, inventory consumption,
   clock changes, account settings or other state that affects the next run.
   Record what changed and whether the experiment can be repeated.
6. **Read what runs from the channel's `state` file**, never by stopping a
   program to find out: stopping one to probe, then starting "whatever is
   selected", once started a second run that saved over the save under test.

## 3. Hypothesize: name the layer, one hypothesis, one prediction

Classify the failure by the evidence. These layers are candidates, not a
fixed order of likelihood; the map and route layers apply to navigation.

| Layer | Looks like |
|---|---|
| **lab process** | the run stopped, restarted or reloaded without a program reason (a leftover loop, a second train on the console) |
| **game rules/state** | an action has no expected effect because a prerequisite, resource, battle phase or story trigger differs |
| **input/timing** | a press was dropped, repeated or delivered in the wrong phase; a timed sequence drifts |
| **map** | a wall where the grid has floor, or the reverse (a feature with more rows on the console than in the trace) |
| **route** | right on paper, fragile in play (a 1-wide corridor on the stick; a search that walks until blocked) |
| **walk/physics** | moves land short, long, or a row off (drift on stairs; time-to-tiles wrong for the move used) |
| **perception** | visual or audio inference reports the wrong state (a cursor missed; a sound confused; a cutscene mistaken for overworld) |

Choose one hypothesis to test and state a distinguishing prediction. For a
menu: "if auto-repeat caused the skip, a shorter hold advances one entry."
For a tile map: "if the feature is 3 rows, a tap south from this row will
move; if 2, it will not." Keep alternative explanations recorded, but vary
one factor at a time so the result distinguishes them.

## 4. Verify interactively with CFR before fixing

Use the Command File Runner (CFR) commands through the lab channel to test
the hypothesis on the live console. Send a small batch of presses, walks,
screenshots and detector checks with `cfrun.sh NAME < batch.txt`. Inspect
the journal and screenshots before choosing the fix. This is agent-driven
verification; ask the human to play the sequence when that would be faster.

- **One batch per question**, 5 to 15 lines, one command per line, a
  `shot` after every move that matters, a `shot` at the end. Name the batch
  by its question. Send it with `cfrun.sh NAME < batch.txt` and read the
  `.out`. Show the user the batch text before sending when pairing; they
  catch wrong assumptions in the command list.
- **Use controls whose effect is established for this screen.** A D-pad
  tap may move a menu cursor, turn a player, take a tile step or do nothing.
  Calibrate hold/release times for the game and mode being tested.
- **Navigation example: probe a wall with `walk`** only where player
  movement changes the captured view and an early stop has been ruled out.
  It reports low screen change, which can also mean a fixed camera, dialog
  or NPC obstruction. Confirm the wall before using it as a position anchor.
- **Check a reading with `detect NAME [image]`.** Saved-image checks avoid
  recreating a visual state. The channel's `state` reports the app/program;
  the legacy CFR program's `state [image]` lists detectors that fire.
- **Restore the starting conditions between comparisons.** Reverse a probe
  only if its actions are reversible; menu wrapping, encounters and item
  consumption can prevent a symmetric input sequence from restoring state.
- **Measure a threshold with `shots`** (N frames at an interval) on both
  the true and the false screen, and keep a margin. Write the numbers and
  the batch name in the code comment.
- **Use a focused program run when the hypothesis needs it.** CFR is useful
  for isolated controls and detector checks; asynchronous behavior, battles,
  RNG timing and complete recovery paths may need the real program. State
  the prediction before running, then use the full run for integration validation.
- **If the human can show it faster, ask them to play it once** while you
  record (no program running, keyboard to the app). Then measure from the
  recording.

## 5. Fix the measured cause with the smallest sufficient change

- **Keep one causal change per experiment.** Include the code and checks
  that change requires; defer unrelated tuning so the result remains attributable.
- **Choose a mechanism that matches the observed behavior.** Simplify when
  fewer assumptions achieve the required outcome. Extra recovery or state
  handling can be necessary; judge it by the failure it handles and how it
  will be verified.
- **State the tolerance and how the fix handles variation.** Examples:

| Context | What to establish | Example fix and validation |
|---|---|---|
| Menu input | hold/release range that advances one entry | shorten a repeating press; check several starting entries |
| Visual/audio detector | separation between target and confusing cases | adjust the feature or threshold; replay positive and negative captures |
| Battle or farming loop | expected phase and resource changes | handle a missed prompt; confirm the outcome before continuing |
| Timing-sensitive/RNG sequence | frame or latency tolerance | correct the measured timing source; test with the actual execution path |
| Navigation | position uncertainty, control semantics and target width | use a verified anchor or wider target; check the resulting pose |

Repeated contradictions mean revisiting the hypothesis. Check alternative
causes before deciding which mechanism to replace. Comment the reason for
the fix and the evidence that supports its constants.

### Navigation example: BDSP AutoStory

In one BDSP route, a multi-column search with time-to-tiles and step-back
was replaced by moving into a wider street and walking until a scene began.
That worked because the destination tolerated the measured movement error
and the scene supplied an observable end condition. It is not a general
prescription to use timed holds.

The following options require validation in the target game, movement mode
and camera view. BDSP measurements do not establish FRLG or 3D-game behavior:

| Move | When | Error |
|---|---|---|
| **Hold toward a wall** | a known static obstacle stops the player and the stop can be confirmed | can constrain one axis; NPCs and early stops must be distinguished |
| **D-pad taps** | the mode's turn/step behavior and timings have been measured | count only confirmed steps; a direction change may only turn |
| **Stick hold** | the destination is wider than the measured arrival uncertainty | drift depends on duration, terrain and control mode |
| **Run** | speed and stopping behavior are measured and the endpoint remains reliable | separate calibration from walking; do not assume identical error |
| **Walk until event** | the trigger region and event's observable completion are known | the event may move the player; update the pose after it |

In this navigation example, a failed wall detector is one explanation for
a traversable leg reporting "blocked"; check the start pose and input too
before deciding.

## 6. Iterate with the console as an external machine

The console is a shared, stateful, slow peripheral. Treat every run as a
transaction:

1. **Restore or confirm the required starting conditions** before each
   comparison. Reload a save only when appropriate; other tests may need a
   menu reset, resource replenishment or a specific clock/RNG state.
2. **Run the smallest sufficient check** under one name and record the
   revision, configuration and change. Build/deploy when code changed; a
   saved-capture replay or CFR probe may not need a new build.
3. **Read the result** (section 2), then decide (3), measure (4), fix (5).
4. **Record the state left between cycles.** For save-backed tests, identify
   the save and current screen separately. Do not create a new save merely
   to normalize an experiment; it may destroy the starting fixture.
5. **One train on the console at a time** (section 7). A second thing
   started on it is how runs get stopped and saves overwritten.
6. **Budget for the behavior being tested.** Set the expected run duration
   and stop condition before starting; section 9 bounds unsuccessful iteration.

## 7. Keep it safe

**Processes**
- **Hold the lock** (`lablock.sh run NAME -- CMD`) for anything that drives
  the console. Keep the whole experiment under one lock; the scripts check
  whether another train holds it. Never start or stop the app's program by
  another path while a run holds it.
- **Read the channel's `state` file to learn what runs**; never stop a
  program to find out.
- **Never press while a program is RUNNING**, by hand or by batch: the
  state file says RUNNING, the console is not yours.
- **Kill loops by their shell, not their child.** A loop over batches
  outlived the batch that was killed and sent the next one into the running
  test. Find the parent and kill it first.
- **Never edit a shell script while a run is reading it**: write a new file
  and `mv` it in. The same for a batch: write it, then move it into the
  inbox.
- **Background commands**: one waiter per thing; kill stale waiters before
  starting new ones; never chain sleeps.
- Never quit an app running a program you did not start; a console that is
  not the lab's is not yours.

**Persistent state and recovery**
- Know whether the program under test **saves, and when**. One that saves at
  its start (some AutoStory checkpoints do) moves the console's save
  the moment it starts: start it only from the state it expects, and never
  start one to "take a screenshot".
- Keep deliberate saves and other persistent changes within the user's
  authorized scope. Name save batches explicitly and record the resulting save
  using the game's available evidence (for example, location and play time).
- **Switch example: verify the HOME selection before launching.** After
  `close_game`, inspect a `shot` and confirm the intended title before A;
  do not assume a previously selected tile is still highlighted.
- **Inspect an unknown screen before input.** Use the game's known recovery
  path. Closing or restarting can discard progress; autosave may already have
  changed the save. Do not treat `close_game` as a universal safe reset.
- Start a new game only when it is within the user's authorized scope.

## 8. Keep track of learning

- **Run notes line per run**: build, change, hypothesis, result, layer.
- **Measurements go where the number lives**: detector fixtures for capture
  cases, comments for input timings, data-source notes for game facts. Map
  rows and wandering NPCs belong with navigation data; movement error belongs
  with its game/mode calibration.
- **Mechanism findings go with the component they change**: detector,
  controller, battle policy or navigation helper. Record the evidence and
  intended owner; post it on the component's PR when authorized.
- **Process findings go to this skill** and a memory note with the why.
- **A lessons file** only when asked.

## 9. When to escalate to the human

- **Hard stops**: the same attempted fix fails twice without new evidence;
  three implementation cycles for one outcome without a pass; or 45 minutes
  without resolving the question. Stop and report the problem, measurements
  and next experiment. Include context appropriate to the task: a route
  drawing, menu sequence, detector captures or timing trace.
- **Required game knowledge remains missing** after checking available
  code, documentation, game data and bounded probes. Ask a specific question
  or request a demonstration: where a trigger lies, which battle phase is
  active, or what a menu option does. Avoid an unbounded exploratory run.
- **An unexpected persistent change would be required**, such as overwriting
  the starting save or consuming resources outside the authorized experiment.
- **The evidence cannot distinguish states that require different actions**
  (which map, menu, battle phase or timing window).
- **Outward-facing actions outside the authorized scope**: upstream comments,
  PR descriptions on repos the user does not own, replies to maintainers.
- **The user is driving.** When they say they are doing it, send nothing to
  the console until they hand it back.

## 10. When to file a follow-up instead of fixing now

Defer findings outside the current outcome when they do not invalidate its
result or make continuing unsafe. Examples: a detector case outside the
target screen, an unused movement mode's speed, or an unrelated helper.
Record the evidence in the task list or notes; use PR comments or issues
when authorized by the task. A missing guard needed for this experiment
belongs in the current fix rather than a deferred follow-up.

## 11. When to question existing decisions and code

- **Repeated patches do not explain the observations.** Revisit the model
  before tuning another constant; repeated failure alone does not identify
  which assumption is wrong.
- **A critical assumption has no evidence.** Examples: a menu delay, an HP
  threshold, or a navigation offset. Measure it or find an applicable source.
- **A simpler demonstrated approach achieves the same outcome.** Compare
  its reliability and observability, not just the action count. Automation
  may need extra checks that a human supplies visually.
- **A policy contradicts a measurement**, such as a detector threshold,
  battle decision, timeout or route-planning cost.
- **Inherited actions lack a current purpose.** Check why each remains:
  a heal trip may be unnecessary for one party and essential for another.
