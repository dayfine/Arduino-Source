---
name: console-iteration
description: The dev loop for getting a console automation segment (BDSP/FRLG AutoStory checkpoint, nav leg, talk, door) from idea to 3/3 - define, code, observe, hypothesize, verify by hand, fix once, rerun - with the Switch as an external machine, one train on the console, hard stops, and rules for learning, escalating and filing follow-ups. Use whenever a console run fails, before writing the next fix, and whenever starting a new checkpoint.
---

# Console iteration: define, measure, change one thing

A program run costs about 3 minutes (build, deploy, reload, run) and shows
one failure. A hand batch on the Command File Runner costs 30 seconds and
shows the whole picture. A human playing the stretch once costs 1 minute and
shows the answer. So the loop is: **define from a hand pass, run, look,
name the layer, verify the guess by hand, change one thing, rerun.** Never
run to "see what happens", and never rerun to "confirm" something a hand
batch can show.

Map and route work follows the `map-route-verify` skill, console rules
`bdsp-autostory-test`. This skill is the loop around them.

## 1. Define a segment, then code it

A segment (checkpoint) is **one map, or one story event**, never both:
Route 203 to Jubilife's first tile; Jubilife to the end of Rowan's scene;
Route 204 to the Ravaged Path. A map boundary is a free anchor (the location
banner), a story event is a free save point. Splitting 01.06 this way took
Route 203 from 5 failed builds to a pass, because a Jubilife problem no
longer replayed Route 203.

- **Start at a save on a fixed tile** (a doorstep, a map's first tile after
  a banner, the spot after a scene). Never a loose spot like a cave mouth.
- **End on something the screen proves**: a banner, a dialog, a Center floor,
  a healed party. "Walked long enough" is not an end.
- **At most 5 steps.** More means it is two segments.
- **Hand pass first.** Walk the segment once on the CFR with a shot after
  every leg, or ask the user to play it while you record (`rec.sh` shots
  every second plus the app log; 18 presses and 30 s found Rowan's scene
  that an hour of my probing had not). Write down, per leg: the move, how
  far it really went, what stopped it.
- **Code from the recording**, not from the map or the grid. Every number
  gets a comment naming the measurement (date, run or batch). A number you
  did not measure (18 taps "from the grid") is a guess and must say so.
- **Pick the move per leg** (table below): clip to a wall, exact D-pad
  taps for counted offsets, a stick hold only into a target wider than its
  error, running only on legs that end at a wall.

## 2. Observe

After a run, before anything else:

1. **Read the log tags**: `[NN.NN step i/n]`, `[Route] Leg`, `[Walk] ...
   after Nms`, `Fighting`, `attempt N failed`. Build the timeline: what the
   code believed at each step.
2. **Open the failure screenshot and say in one sentence where the player
   really is**, from landmarks you can name: a sign, a banner, a building,
   the fountain. I called Route 202 "Route 204" from the trees alone and
   lost a cycle. If you cannot name the map from the shot, take another by
   hand with a landmark in view before concluding anything.
3. **Compare belief and reality.** The gap is the bug: "the code thought
   it was still in Jubilife; the shot shows Route 202."
4. **Check the story state**: a pass needs a flag that moved (someone
   answered, a scene played, a badge, a banner), checked by something the
   next step depends on.
5. **Check the save state**: did the run save, and where? A run's
   checkpoint save overwrites the console's save; know what it holds now.

## 3. Hypothesize: name the layer, one hypothesis, one prediction

Layers, in order of how often they were the real cause today:

| Layer | Looks like | Today's example |
|---|---|---|
| **lab process** | the run stopped, restarted or reloaded without a program reason | r232a stopped mid-battle by a leftover probe loop; my helper's "stop to see what's selected" started AutoStory and overwrote the save |
| **story** | walked the right path, nothing happened | Rowan's scene: I assumed "up the north road" without knowing the trigger |
| **map** | a wall where the grid has floor, or the reverse | Route 203's sand: 1 row in the trace, 3 on the console |
| **route** | the plan is right on paper but fragile in play | a 1-wide corridor leg on the stick; a column search that walks south until blocked |
| **walk/physics** | moves land short, long, or a row off | stick drift on stairs; time-to-tiles wrong for D-pad walks |
| **perception** | the detector read the screen wrong | "blocked, 0 tiles" while walking in a fixed-camera area; a cutscene with no dialog box |

Write one hypothesis with a prediction that only it makes: "if the sand is
3 rows, a tap south from the girl's row will move; if 2, it will not."
Two hypotheses at once means you have not looked hard enough.

## 4. Verify by hand before fixing

- **One CFR batch per question**, 5 to 15 commands, a `shot` after every
  move that matters. Show the user the batch text before sending when
  pairing; they catch wrong assumptions in the command list.
- **Symmetric batches**: a probe that goes 3 up must come 3 down. My
  "3 up, 4 down" probe drifted the player a row per column and wrecked the
  measurement.
- **PSNR between consecutive shots** tells moved (about 15 to 20) from
  stood still (above 30) without looking; look anyway at the ends.
- **Measure thresholds on both the true and the false screen** and keep a
  margin.
- **Never verify by rerunning the program.** A rerun is 3 minutes for one
  bit; a batch is 30 seconds for ten.
- **If the human can show it faster, ask them to play it once** while you
  record. Then measure from the recording.

## 5. Fix: one change, the simplest that fits the margin

- **Change the one thing the measurement points at.** Not two.
- **Prefer removing a mechanism over adding one.** The column search
  (7 columns, time-to-tiles, step back) became "stick 14 tiles west, walk
  north until the scene". When a mechanism keeps needing patches, it is
  fighting the game; replace it.
- **Pick the move by its margin**, not by habit:

| Move | When | Error |
|---|---|---|
| **Clip to a wall** (stick or D-pad hold until stopped) | whenever a wall, fence, sign or counter ends the leg; pins one axis for free | ±0 on that axis |
| **D-pad taps** (160 ms, settle 640 to 700 ms) | counted offsets to a door, a person, a corridor row; stairs and 1-wide corridors | ±0 (whole tiles); every tap steps, the first one toward a new direction too (r248a) |
| **Stick hold** (160 ms/tile across, 240 up/down) | open legs into a target wider than the error: 14 tiles into a 4-wide street | about ±0.15 tile per tile; drifts a row on stairs and corridors |
| **Run** (B + D-pad) | legs that end at a wall; speed assumed 2x, not measured | as D-pad |
| **Walk until event** | a scene or talk ends the leg; a cutscene may open with no dialog box, so wait for the first line | needs its own end check |

- **A new idea is due when the old one contradicts a measurement twice.**
  Two "blocked, 0 tiles" on a leg the hand pass walked means the stop
  detector is wrong there (fixed camera), not the grid.
- **Comment the why**: the run that failed, the batch that measured, the
  number.

## 6. Iterate with the Switch as an external machine

The console is a shared, stateful, slow peripheral. Treat every run as a
transaction:

1. **Reload the save** before every run (`seg01.sh ... reload`), so the
   start is the checkpoint's start and not wherever the last run died.
2. **Build, deploy, run** under one name (`bdsp-rNNN`, run `rNNNa`), one
   change per build, logged in the run notes line.
3. **Read the result** (section 2), then decide (3), measure (4), fix (5).
4. **Keep the console at a known save between cycles.** If a run dies
   elsewhere, reload before the next, or walk back and save deliberately,
   and say which save the console holds.
5. **One train on the console at a time** (section 7). Starting a second
   thing is how today's runs were stopped and a save was overwritten.
6. **Budget**: a cycle is about 3 minutes of console time plus the fix.
   Three cycles without a pass is the hard stop (section 9).

## 7. Keep it safe

**Processes**
- `~/BDSPLab/lablock.sh` holds console B for one run. `seg01.sh` waits its
  turn; `cfrun.sh` refuses if held; `labctl.sh` and `stage_run.sh` refuse a
  caller without the lock. Use them; never call the app's start/stop by
  another path while a run holds the lock.
- **Never stop a program to find out what is running.** Read the app's
  `LabControl/state` file. Stopping AutoStory to probe, then starting
  "whatever is selected", started a second AutoStory run that saved over
  the checkpoint save.
- **Kill loops by their shell, not their child.** A `for` loop over CFR
  batches outlived the batch I killed and sent the next one into the
  running test. Find the parent (`pgrep -fl "for k in"`) and kill it first.
- **Never edit a shell script while a run is reading it**: write a new
  file and `mv` it in.
- **Background commands**: one waiter per thing; kill stale waiters before
  starting new ones; never chain sleeps.
- Console and app rules from `bdsp-autostory-test`: never quit an app
  running a program you did not start; never press keys while a program is
  RUNNING; console A is not ours.

**Save state**
- A checkpoint run **saves at its start**. Starting checkpoint N moves the
  console's save to N's start. Start a checkpoint only from its own save,
  and never start one to "take a screenshot".
- Hand batches never save unless the batch is named a save (`sv*`) and the
  user asked.
- Before any deliberate save, screenshot the save prompt (location, play
  time) and keep it with the run notes.
- Never start a new game without the user's OK.

## 8. Keep track of learning

- **Run notes line per run**: build, change, hypothesis, result, layer.
- **Measurements go where the number lives**: the data file header for
  map facts (rows, stairs, wandering NPCs), the code comment for a tap
  count or hold, `physics.txt` for movement error.
- **Mechanism findings go to the planner repo** as a PR comment on the PR
  they change (stick drift sideways, event leg ends, fixed-camera stops,
  single-tile starts), so the watcher applies them.
- **Process findings go to this skill** and a memory note with the why.
- **Lessons file** (`AutoStory-Lessons.md`) only when asked.

## 9. When to escalate to the human

- **Hard stops**: the same step fails twice in a row; three builds for one
  checkpoint without a pass; 45 minutes on one checkpoint. Stop, send one
  line (problem, what was measured, plan) and the route drawing.
- **Story knowledge is missing**: where a scene triggers, which NPC to
  talk to, whether a flag is set. Ask, or ask the user to play it once.
  Do not search the map with code.
- **A save would be overwritten** or the console's save is not where the
  next run expects.
- **The screen cannot be read with certainty** (which map, which menu).
- **Anything outward-facing**: upstream comments, PR descriptions on repos
  the user does not own, replies to maintainers.
- **The user is driving.** When they say they are doing it, send nothing
  to the console until they hand it back.

## 10. When to file a follow-up instead of fixing now

Anything found that is not on the critical path to the current pass:
fixed-camera stop detection, running speed measurement, time-to-tiles for
D-pad walks, the segment 00 plan cap, a helper's missing fail-fast. File it
(task list, PR comment, issue) with the run that showed it, and keep going.
Fixing it inline is how one checkpoint eats an afternoon.

## 11. When to question existing decisions and code

- **A mechanism needed its third patch** (`west_until_transition`'s row
  probes, the column search): the plan is wrong, not the constants.
- **The code assumes a fact nobody measured** ("18 taps from the grid",
  "the scene is up the north road"). Measure it or ask.
- **The human would do it differently.** If the user's hand route is three
  moves and the code's is thirty, the code is wrong.
- **A cost or rule in the planner contradicts a measurement** (open legs
  always cost 8; a 14-tile stick hold into a 4-wide street is safe).
- **Inherited steps**: check the story needs each one (the Oreburgh heal
  trip did not).

## 12. CFR recipes (copy, adapt, send)

The Command File Runner takes a text file, one command per line (the full
table: `SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/README.md`).
Send a batch with `printf '...' | sh cfrun.sh NAME`: it waits for the lab lock,
stops keep-awake, prints the `.out`, and shots land in
`.../lab-B/CommandFiles/outbox/NAME_label.png`. Name batches by their question
(`probe-stairs1`), keep them to 5 to 15 lines, end every batch with `shot`.

**Launch the game to its save, from HOME.** HOME puts its highlight back on
the last game played every time it opens, and `close_game` opens it: move the
highlight after the close, never before (2026-10-31: two launches of Pokemon
Violet into its new-game screens).

    close_game
    dpad RIGHT 100 800
    shot home

Read the label in the shot (`Pokemon Brilliant Diamond`); one more `dpad` per
tile if not. Then, as a second batch:

    press A 100 3000
    press A 100 3000
    wait 35000
    press A 100 3000
    wait 3000
    press A 100 3000
    press A 100 6000
    wait 8000
    shot here

`seg01.sh ... reload` does the close and the launch at once on whatever is
highlighted: use it only when nobody touched the console since our last run.

**Count tiles.** D-pad taps move whole tiles; a 160 ms hold:

    dpad LEFT 160 640
    shot l1
    dpad LEFT 160 640
    shot l2

A tap that moved changes the screen by 21 to 50 (RMSD at 192x108); a bump into
a wall, or an NPC brushing past, 4 to 13. `moves.sh` prints the PSNR between
consecutive shots (about 20 dB moved, 38 and up still). A 160 ms D-pad tap
turns and steps whichever way the player faced (r248a: all seven first taps
moved a tile), so every tap counts; only a 100 ms stick tap turns without a
step.

**Find a wall.** `walk -1 0 6000` holds the stick until two slices in a row
stay still, then `shot`: the player ends against the wall whatever the start
was (the "clip" of clip-then-offset). Not on fixed-camera stretches.

**Talk, read a sign, answer a prompt.** Face the tile with a tap toward it,
`press A 100 1500`, `shot`, `detect bdsp_dialog`. Rocks and signs answer too:
read the shot. `press B 100 800` advances lines and never starts a talk; `A`
takes Yes on a prompt, so read the prompt's cursor in the shot first.

**Where am I.** `state` prints every detector firing on the frame. Read
positions from the feet and ground features, never roofs or heads; take a
shot with a landmark in view (sign, banner, door, stairs) before naming a map.
"Did I enter": `wait_for bdsp_overworld 8000`, `detect black`.

**Record a human.** CFR stopped, keyboard to the lab app; `watch/rec.sh`
screencaptures the lab window every second (cropped to the game area, or
4 MB a frame), plus the app log. The keyboard shows in the log only as
`controller_state`, so the shots carry the moves. Montage them 3 or 4 wide
(`mont.py`) to read a walk in one picture.

**Measure a threshold.** `shots label 10 400` gives ten frames 400 ms apart.
Compute the metric on the true and the false case (a still player with an NPC
walking by; a moving player; a bump) and keep a margin before choosing the
number. Write the numbers and the batch name in the code comment.

**Recover.** On an unknown screen: `shot` first, then `close_game` (HOME, X,
confirm: nothing is saved), then the launch recipe. Never press A blind on an
unknown screen: a new game's first screens accept it.

## Known BDSP physics and perception traps (2026-10-31)

- Every stick tap moves the player, including a 100 ms "turn" (about 0.4
  tile) and a 50 ms nudge (0.2 to 0.45 tile). Do not turn away and back
  between tries.
- Stick holds drift sideways on 1-wide corridors (tree lines) and stairs;
  D-pad taps do not. Use D-pad there.
- Pushing against a round post or a facade slides the player back to the
  same spot; small nudges may not move you there at all.
- Door and cave fades often read as a time limit, not a black screen.
  Detect being inside by the screen, and end walks on the location banner.
- Fixed-camera stretches (parts of Route 203) make walking read as
  "blocked, 0 tiles": screen change cannot confirm a wall stop there.
- A cutscene (Rowan's) opens with no dialog box: the walk reads as stopped;
  wait for the first line.
- Wandering NPCs (the Poketch clown, Jubilife's green-haired man, Route
  203's road woman and the blue-haired man) are moving walls: never anchor
  on them, and wait them out before re-planning.
- Rocks and signs answer A too. Check that the talk is the one you want.
