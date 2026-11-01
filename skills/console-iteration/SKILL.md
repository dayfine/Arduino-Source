---
name: console-iteration
description: The dev loop for getting one step of a console automation program (a navigation leg, a talk, a door, a menu sequence, a detector) from idea to a repeatable pass - define, code, observe, hypothesize, verify by hand, fix once, rerun - with the console as an external machine, one train on it at a time, hard stops, and rules for learning, escalating and filing follow-ups. Use whenever a console run fails, before writing the next fix, and whenever starting a new step.
---

# Console iteration: define, measure, change one thing

A program run costs minutes (build, deploy, reload, run) and shows one
failure. A hand batch of presses and detector checks on the live console
costs seconds and shows the whole picture. A human playing the stretch once
costs a minute and shows the answer. So the loop is: **define from a hand
pass, run, look, name the layer, verify the guess by hand, change one thing,
rerun.** Never run to "see what happens", and never rerun to "confirm"
something a hand batch can show.

## How to use

This skill lives in the repo at `skills/console-iteration/SKILL.md`; install
it with `ln -s "$PWD/skills/console-iteration" ~/.claude/skills/console-iteration`
and it loads when a console run fails or a new step starts. Everything it
asks of the console goes through the lab channel (`Lab/` folder of the lab
app's profile) and the three scripts in `devtools/lab/`:

```sh
export LAB_CONSOLE=B                                   # the lock's name
export LAB_PROFILE="$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B"
export LAB_OWNER=lab-B                                 # the channel's owner token
cd devtools/lab
sh labctl.sh state                                     # first: what runs? never stop a program to find out
sh labctl.sh owner claim lab-B                         # once per session
sh labctl.sh select PokemonBDSP:AutoStory              # the program under test
sh labctl.sh set START_SEGMENT '"0"'                   # one option, JSON value
sh labctl.sh start                                     # the run; then read the log and shots
printf '%s\n' "dpad UP" "shot after_up" "detect home" | sh cfrun.sh probe-up   # a hand batch
sh lablock.sh run my-cycle -- sh my_cycle.sh           # build + deploy + run as one train
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

A step is **one thing the screen can prove happened**: one map crossed, one
menu sequence, one talk, one detector reading. Never two things in one. A
map boundary is a free anchor (the location banner), a game event is a free
save point. Splitting an overgrown step in two once turned five failed
builds into a pass, because a problem in its second half no longer replayed
the first. (A story program's checkpoint is one such step; so is a farming
program's "enter the shop and buy".)

- **Start from a known state** (a save on a fixed tile, a known menu, the
  spot after a scene). Never a loose spot like a cave mouth.
- **End on something the screen proves**: a banner, a dialog, a healed
  party, a menu. "Walked long enough" is not an end.
- **At most 5 moves.** More means it is two steps.
- **Hand pass first.** Walk the step once with a batch (`cfrun.sh`) with a
  `shot` after every leg, or ask the user to play it while you record. Write
  down, per leg: the move, how far it really went, what stopped it.
- **Code from the recording**, not from the map or the grid. Every number
  gets a comment naming the measurement (date, run or batch). A number you
  did not measure is a guess and must say so.
- **Pick the move per leg** by its margin (section 5): clip to a wall,
  exact taps for counted offsets, a hold only into a target wider than its
  error.

## 2. Observe

After a run, before anything else:

1. **Read the log tags** (step counters, leg lines, walk results, attempt
   failures). Build the timeline: what the code believed at each step.
2. **Open the failure screenshot and say in one sentence where the player
   really is**, from landmarks you can name: a sign, a banner, a building.
   Naming a map from its trees alone once cost a whole cycle. If you
   cannot name the screen from the shot, take another by hand with a
   landmark in view (`shot`, or `state` for every detector that fires)
   before concluding anything.
3. **Compare belief and reality.** The gap is the bug: "the code thought it
   was still in town; the shot shows the route."
4. **Check the game state**: a pass needs something that moved (someone
   answered, a scene played, a banner, an item count), checked by something
   the next step depends on.
5. **Check the save state**: did the run save, and where? A program that
   saves overwrites the console's save; know what it holds now.
6. **Read what runs from the channel's `state` file**, never by stopping a
   program to find out: stopping one to probe, then starting "whatever is
   selected", once started a second run that saved over the save under test.

## 3. Hypothesize: name the layer, one hypothesis, one prediction

Layers, in the order they have actually been the cause:

| Layer | Looks like |
|---|---|
| **lab process** | the run stopped, restarted or reloaded without a program reason (a leftover loop, a second train on the console) |
| **game event** | did the right thing, nothing happened (the trigger was assumed, not known) |
| **map** | a wall where the grid has floor, or the reverse (a feature with more rows on the console than in the trace) |
| **route** | right on paper, fragile in play (a 1-wide corridor on the stick; a search that walks until blocked) |
| **walk/physics** | moves land short, long, or a row off (drift on stairs; time-to-tiles wrong for the move used) |
| **perception** | the detector read the screen wrong (a stop read as a wall in a fixed-camera area; a cutscene with no dialog box) |

Write one hypothesis with a prediction that only it makes: "if the feature
is 3 rows, a tap south from this row will move; if 2, it will not." Two
hypotheses at once means you have not looked hard enough.

## 4. Verify by hand before fixing

- **One batch per question**, 5 to 15 lines, one command per line, a
  `shot` after every move that matters, a `shot` at the end. Name the batch
  by its question. Send it with `cfrun.sh NAME < batch.txt` and read the
  `.out`. Show the user the batch text before sending when pairing; they
  catch wrong assumptions in the command list.
- **Count in taps (`dpad`), not time**: a tap moves a whole tile; a timed
  stick hold moves a different count from try to try.
- **Find a wall with `walk`**: it holds the stick until the screen stops
  changing and reports how long it moved, so the player ends against the
  wall whatever the start was. Not where the camera is fixed.
- **Check a reading with `detect`** and see everything that fires with
  `state`; both also run on a saved image, so the console need not be on
  that screen again.
- **Symmetric batches**: a probe that goes 3 up must come 3 down. An
  asymmetric probe drifted the player a row per repetition and wrecked the
  measurement.
- **Measure a threshold with `shots`** (N frames at an interval) on both
  the true and the false screen, and keep a margin. Write the numbers and
  the batch name in the code comment.
- **Never verify by rerunning the program.** A rerun is minutes for one
  bit; a batch is seconds for ten.
- **If the human can show it faster, ask them to play it once** while you
  record (no program running, keyboard to the app). Then measure from the
  recording.

## 5. Fix: one change, the simplest that fits the margin

- **Change the one thing the measurement points at.** Not two.
- **Prefer removing a mechanism over adding one.** A multi-column search
  with time-to-tiles and step-back became "hold into the wide street, walk
  until the scene". When a mechanism keeps needing patches, it is fighting
  the game; replace it.
- **Pick the move by its margin**, not by habit:

| Move | When | Error |
|---|---|---|
| **Clip to a wall** (hold until stopped) | whenever a wall, fence, sign or counter ends the leg; pins one axis for free | none on that axis |
| **D-pad taps** | counted offsets to a door, a person, a corridor row; stairs and 1-wide corridors | whole tiles; every tap steps, including the first one toward a new direction |
| **Stick hold** | open legs into a target wider than the error | grows with distance; drifts a row on stairs and corridors |
| **Run** | legs that end at a wall; speed assumed, not measured | as taps |
| **Walk until event** | a scene or talk ends the leg; a cutscene may open with no dialog box, so wait for the first line | needs its own end check |

- **A new idea is due when the old one contradicts a measurement twice.**
  Two "blocked, 0 tiles" on a leg the hand pass walked means the stop
  detector is wrong there, not the grid.
- **Comment the why**: the run that failed, the batch that measured, the
  number.

## 6. Iterate with the console as an external machine

The console is a shared, stateful, slow peripheral. Treat every run as a
transaction:

1. **Reload the known state** (the save, the menu) before every run, so the
   start is the step's start and not wherever the last run died.
2. **Build, deploy, run** under one name, one change per build, logged in
   the run notes line.
3. **Read the result** (section 2), then decide (3), measure (4), fix (5).
4. **Keep the console at a known save between cycles.** If a run dies
   elsewhere, reload before the next, or walk back and save deliberately,
   and say which save the console holds.
5. **One train on the console at a time** (section 7). A second thing
   started on it is how runs get stopped and saves overwritten.
6. **Budget**: a cycle is a few minutes of console time plus the fix.
   Three cycles without a pass is the hard stop (section 9).

## 7. Keep it safe

**Processes**
- **Hold the lock** (`lablock.sh run NAME -- CMD`) for anything that drives
  the console; `labctl.sh` and `cfrun.sh` refuse a caller without it. Never
  start or stop the app's program by another path while a run holds it.
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

**Save state and the HOME screen**
- Know whether the program under test **saves, and when**. One that saves at
  its start (a story program's checkpoint run does) moves the console's save
  the moment it starts: start it only from the state it expects, and never
  start one to "take a screenshot".
- Hand batches never save unless the batch is named a save and the user
  asked.
- Before any deliberate save, screenshot the save prompt (location, play
  time) and keep it with the run notes.
- **Before a reload, check which tile HOME highlights.** HOME puts its
  highlight back on the last game played every time it opens, and
  `close_game` opens it: move the highlight after the close, never before,
  and read the title in a `shot` before pressing A, or the wrong game
  launches into its new-game screens.
- **Never press A blind on an unknown screen**: a new game's first screens
  accept it. `shot` first, then `close_game` (nothing is saved), then the
  launch recipe.
- Never start a new game without the user's OK.

## 8. Keep track of learning

- **Run notes line per run**: build, change, hypothesis, result, layer.
- **Measurements go where the number lives**: the data file header for map
  facts (rows, stairs, wandering NPCs), the code comment for a tap count or
  hold, the physics notes for movement error.
- **Mechanism findings go to the component they change**, as a PR comment
  on its PR (drift, event-leg ends, fixed-camera stops), so the owner
  applies them.
- **Process findings go to this skill** and a memory note with the why.
- **A lessons file** only when asked.

## 9. When to escalate to the human

- **Hard stops**: the same move fails twice in a row; three builds for one
  step without a pass; 45 minutes on one step. Stop, send one line (problem,
  what was measured, plan) and, for a walk, the route drawing.
- **Game knowledge is missing**: where a scene triggers, which NPC to talk
  to, whether a flag is set, what a menu does. Ask, or ask the user to play
  it once. Do not search the game with code.
- **A save would be overwritten** or the console's save is not where the
  next run expects.
- **The screen cannot be read with certainty** (which map, which menu).
- **Anything outward-facing**: upstream comments, PR descriptions on repos
  the user does not own, replies to maintainers.
- **The user is driving.** When they say they are doing it, send nothing to
  the console until they hand it back.

## 10. When to file a follow-up instead of fixing now

Anything found that is not on the critical path to the current pass: a
detector that misreads one area, an unmeasured speed, a helper's missing
fail-fast. File it (task list, PR comment, issue) with the run that showed
it, and keep going. Fixing it inline is how one step eats an afternoon.

## 11. When to question existing decisions and code

- **A mechanism needed its third patch**: the plan is wrong, not the
  constants.
- **The code assumes a fact nobody measured** ("18 taps from the grid",
  "the scene is up the north road"). Measure it or ask.
- **The human would do it differently.** If the user's hand route is three
  moves and the code's is thirty, the code is wrong.
- **A cost or rule in the planner contradicts a measurement.**
- **Inherited moves**: check the game needs each one; a heal trip carried
  over from an older plan did not.
