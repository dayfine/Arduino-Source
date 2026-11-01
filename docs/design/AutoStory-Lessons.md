# AutoStory: what broke on a real console, and the rule each break taught

Each entry is a failure seen on a Switch while running BDSP AutoStory (Developer-Butters'
U#1717 plus our intro work, October 2026), the pattern behind it, and the rule we now follow.
Runs are the lab builds r14 to r109 on console B. The patterns repeat, so the rules matter
more than the individual fixes.

The lessons fall into two parts. **Part 1 is for Butters**: what his recorded segments in
#1717 (PokemonAutomation/Arduino-Source#1717) need to survive another console, another
starter or another day, with the general rules behind each fix. **Part 2 is for us**: our
own code, our lab and our process. The rules in Part 1 apply to our code too.


# Part 1. For Butters: his segments on another console

## 1. Timed movement drifts; stop on what the screen shows

| Run | What happened |
|---|---|
| r27 | "Route 201 align east 2300ms" walked into the trees; the next dialogue never came. |
| r31 | Two timed grass walks ended a tile short; Tristan never saw us. |
| r33, r39 | Five timed legs to the Potion giver ended one grass row off; A faced nobody. |
| r35 | Walking 5 s into Jubilife ran past the spot where Dawn's scene starts. |

Error from each timed leg adds up, and the same leg lands in different places from run to run.

**Rule:** a duration is only a cap. A walk ends on a screen event: a wall (the view stops
scrolling), a dialogue, a black transition, a battle, or a landmark. Between two anchors, use
as few timed legs as possible. Walking to a wall is the cheapest anchor: hold long enough to
reach it from anywhere the previous legs could have left us, and the wall puts us in the same
place every time. (`walk_until`, `walk_to_talk` in `PokemonBDSP_AutoStory_Walk`.)

## 2. Checks tied to one capture, one starter or one time of day

| Run | What happened |
|---|---|
| r27 | The Potion bag search matched pixels against the author's capture. Console B is warmer (blank list 235 blue vs 253) and softer, so the Potion in row 1 failed the match. |
| r28 | The Tristan fight required "Ember" in move slot 3: Chimchar only. We had Piplup. |
| r28 | Grass-exit landmarks scored 0.41 and 0.60 after the console clock had drifted 3.5 h from the time the references were taken at. |
| r34, r36 | "Blocked" was read from screen change; in tall grass the sway keeps the screen changing. |

**Rule:** if the screenshot plainly shows the thing a check looks for, the check is too
narrow. Fix the check, don't gather more runs. Prefer signals that survive other capture
cards and other playthroughs: OCR of names (items, moves, route banners), layout facts,
colour-normalised comparison. Never assume the starter: pick moves from what the Fight menu
shows. When an image check depends on lighting, set the clock right before it, including on
retries.

## 3. A retry is not a fix

Butters' checkpoint retry reloads the save and runs the same steps again. A deterministic
failure (wrong starter, wrong row, a narrow check) fails all 20 attempts. r27 spent three
attempts on the Potion check, and every r28 to r39 failure retried until we stopped it.

**Rule:** stop at the first failure, go HOME and close the game, find the cause. Save the
screen before the retry resets it (`Screenshots/AutoStory/failures/`), and save the save prompt
at each checkpoint (`Screenshots/AutoStory/saves/`: play time, badges, party).

## 4. Know what a stop means before reacting to it

| Run | What happened |
|---|---|
| r30 | Walking toward Tristan read as "blocked": he had spotted us and frozen the player while he walked over. |
| r32 | Natalie spotted us, but her walk kept the screen changing, so the walk ran out its time instead. |

**Rule:** for a trainer or a scene, the walk can end in any way short of a transition; the
dialogue decides. Walk, then wait for the dialogue with its own timeout.

## 5. Butters' 00.04 and 00.05, checked against the game

What his recorded version does, what went wrong with it on console B, and what replaced it. Our
changes are on the fork branch
[`bdsp/new-game-intro-v2`](https://github.com/dayfine/Arduino-Source/commits/bdsp/new-game-intro-v2),
on top of his segment 00 ([`bdsp/1717-seg00`](https://github.com/dayfine/Arduino-Source/compare/503a28fc6...bdsp/1717-seg00)).

| His version | What happened | Now | Our commits |
|---|---|---|---|
| Every leg is a stick hold of a recorded length (00.05 alone is ~150 lines of them). | Drift, as in section 1. In r79 the 00.05 legs reached the Trainers' School, then "west 200 ms, A" faced an empty tile: Barry stands up and to the right, at the blackboard. | 00.04 walks map plans whose legs end at walls (design doc, sections 3 to 5). 00.05 is being stepped through (section 10 below). | [`1598fc2c7`](https://github.com/dayfine/Arduino-Source/commit/1598fc2c74e7cccf6356251bf35e21fdbcac5006), [`0a5311175`](https://github.com/dayfine/Arduino-Source/commit/0a5311175ec72feb0fcb326a38d7e2dc7b1ec673) |
| Tristan is fought with move slot 3 (Ember). | Chimchar only; we had Piplup (r28). | The Fight menu is read by OCR and the strongest move picked. | [`c16ee0262`](https://github.com/dayfine/Arduino-Source/commit/c16ee0262772a5519e489fb9ad360d263d07852c), [`97df88c43`](https://github.com/dayfine/Arduino-Source/commit/97df88c439845600a02d0114478bb66d4b158ad5) |
| No training, no healing before the Route 202 trainers. | A Lv5 Piplup with Pound fainted to Logan's Shinx (r72). | Three wild battles first (Piplup learns Water Gun), then Potions. | [`08a9fb7e0`](https://github.com/dayfine/Arduino-Source/commit/08a9fb7e01b6be5ee042708b89e32ca2149dca69), [`a95359311`](https://github.com/dayfine/Arduino-Source/commit/a953593111dba3d56b10cf995eb7e9c86455018d) |
| The Potion is found in the Bag by a pixel match. | Console B's colours differ; the match failed (r27). | OCR of the item name. | [`50c9d955d`](https://github.com/dayfine/Arduino-Source/commit/50c9d955d44d767d58d45834dc7e8d3a1b0e6c65) |
| The Route 201 Poké Mart lady is reached by timed legs. | Missed her by a tile (r33, r39); so did our first planned route, which had her above the row (r77). | Walk west along her grass's top row until she stops us: she stands at (43,3), and we stop at (44,3) facing her. One A per line, 1.5 s apart. | [`99715d5d6`](https://github.com/dayfine/Arduino-Source/commit/99715d5d6d3e2c373980b68123363a2b64ae1022), [`f823453e9`](https://github.com/dayfine/Arduino-Source/commit/f823453e9cea8966d20752c4da5f48f54e634d76) |
| Barry in the Trainers' School: "west 200 ms, A", then mash B. | Faced nobody (r79). | From the school door: one tile west, north until Barry stops us, then A per line. He gives the Town Map. | (not yet coded) |
| Leaving 00.04: walk north 5 s into Jubilife. | Ran past Dawn's spot (r35); with our route, walked into the wall beside the stairs (r78). | Walk north; when the wall stops us and no dialogue follows, step a tile west and try again. Next: put the stairs in the map. | [`3cef78b77`](https://github.com/dayfine/Arduino-Source/commit/3cef78b771877675c83356133d8917eb0de681c9) |
| The Pokétch president (Phase 23): timed legs, then wait for his dialogue. | He stands in the road and talks when we face him; pressing A to advance re-opened his talk each time it ended, so his first line came back again and again. | Talk once with A, then advance with B, which never starts a talk. | (stepped through; not yet coded) |
| Three coupon clowns: recorded legs, then press toward each and A until a dialogue opens. | The recorded legs ended a row off each clown; walking past a clown on the row above or below never talks. | Each clown stands still: below the Poké Mart's planters (from the south, facing north; Coupon 1), at the TV station door (from the east, facing west; Coupon 3), beside the Pokétch Company sign (from the south, facing north; Coupon 2). The default "Yes" answers both quiz questions; close "obtained" with A, the rest with B. | (stepped through; not yet coded) |
| Pokétch from the president: a landmark match, then align and talk. | — (not reached) | He is back at his first spot. Stand level with him on his east side, face west, A, then B. "Please press the R Button" means the Pokétch is ours. | (stepped through; not yet coded) |
| Into the Jubilife Pokémon Center, to the PC. | — (not reached) | The door opening is half a tile east of where the frame stops a walk north. Inside: north to the counter, about 4 tiles east, then the tile below the PC, facing north. | (stepped through; not yet coded) |
| Checkpoint retry reloads and repeats the same steps. | Deterministic failures retried up to 20 times. | Lab runs stop at the first failure (section 3). | lab scripts |

His route did one thing right that we dropped for a while: the Poké Mart lady on Route 201
gives **10 Potions**. With them, healing needs no trip to the Pokémon Center: heal to full
after training, and after each trainer read the HP bar in the Bag and use a Potion under half.
The run now gets the Potions first and saves on Route 202, (30,19), so later attempts skip her.

## 6. Butters' segment 01, checked against what we found

| His version | What we found | Rule |
|---|---|---|
| Oreburgh city, mine and gym are timed stick holds and d-pad tap counts. | The same drift as section 5; d-pad taps are one tile each only when the player already faces that way. | Walls and the map, not counts. |
| Roark in the mine: A once, then up to 30 s for his first box, B to advance. | Right to wait long and to advance with B. But A does nothing until Barry has been talked to at the gym door, which his route gets from the tour scene's mashed B. | Make the Barry talk an explicit step. |
| Gym: up 3, left 4, up 5, right 8, up 11, left 5, up 4, then A at Roark. | That path goes round both gym trainers; straight up meets both. | Worth keeping as a plan for the gym grid; the trainers' experience helps an under-levelled team, though. |
| "Barry 1 battle logic can be used here since the moves don't matter this early." | Moves matter against Roark: Pound does almost nothing to rock types, and the first two tries were lost. | Pick Water Gun by name; heal or switch before Cranidos. |

## 7. Butters' route on console B, 2026-10-29 (r107 to r109)

Two failures in today's runs came from #1717's own code rather than ours:

| Run | What happened | Rule / fix | Commit |
|---|---|---|---|
| r108 | 00.01 "Failed to enter Date Change screen". | The clock pin walks the System Settings menu by position; with "Synchronize Clock via Internet" on, the date row is disabled and the walk lands on Time Zone. Turned the sync off on console B; dropping the pin is queued. | — |
| r108b | 00.04: A beside Mom started nothing. Two d-pad taps north from the door should have put us level with her; one did not move. | Same as section 1, now in the house: counted taps lose tiles. The door fixes the column, so face east and press A one row at a time going north until she answers. | `985de47c1` |

# Part 2. For us: our code, our lab, our process

## 8. Our own process

- Look at the screen, then the code, before rerunning. Every run since r28 that failed in a new
  way was explained by one screenshot.
- Put new code in its final module first; no copy-then-move PRs of our own code.
- Keep line endings as they are (CRLF files stay CRLF); compare `git diff --stat` with
  `--ignore-cr-at-eol`.
- When a program is stuck or failing, stop it and close the game at once; don't let retries run.

## 9. Our own mistakes in this round

- **An optional step kept for two hours.** We spent ~15 runs making the Route 201 detour
  work before asking whether the story needs it. Rule: before automating an inherited
  step, write down why the story needs it; after three different failures at one stage,
  question the stage, not the symptom.
- **Claims not checked against the screen.** We concluded "the save already has Potions"
  from log lines of runs that had, in fact, reached the lady; r76's Bag screenshot showed an
  empty Medicine pocket. Rule: an inventory or position claim needs a screenshot of it.
- **Map errors, each found from one screenshot.** Route 202's east tree wall is at column 31
  in rows 15 to 17, not 33 (r75 lost its place by three rows); the lady is in the grass row,
  not above it; Jubilife's south edge is a wall with stairs, not open ground. Rule: when a
  wall leg stops somewhere unexpected, compare its screenshot with the grid before changing
  any timing, and add the measured stop as a wall-hit test. ([`a95359311`](https://github.com/dayfine/Arduino-Source/commit/a953593111dba3d56b10cf995eb7e9c86455018d), [`f823453e9`](https://github.com/dayfine/Arduino-Source/commit/f823453e9cea8966d20752c4da5f48f54e634d76), [`3cef78b77`](https://github.com/dayfine/Arduino-Source/commit/3cef78b771877675c83356133d8917eb0de681c9))
- **Movement is not tile-locked.** A short tap moves about 0.77 of a tile. A left/right
  wiggle for encounters rocked across one tile edge and found no battle in three minutes
  (r74). Walk wall to wall instead: the catch grass's rows sit between tree walls. ([`68279ca9b`](https://github.com/dayfine/Arduino-Source/commit/68279ca9ba631a5012207ad664892a2bdf88e760))
- **One fixed OCR cut for every button.** Move names under the cursor are dark text on a
  light blue button, and status moves sit lower (no "Effective" line); a fixed cut and box
  read "" for Water Gun and Growl (r73). Threshold halfway between each button's background
  and its darkest pixel, and keep the tallest text band. ([`97df88c43`](https://github.com/dayfine/Arduino-Source/commit/97df88c439845600a02d0114478bb66d4b158ad5))
- **Reading positions from roofs and heads.** In the tilted 3D view, a roof shifts against the
  ground as the camera follows us, and a sprite's head rises into the row above its feet. Fixed
  clowns looked like they wandered, and we pressed A diagonally next to the president, which
  talks to nobody. Rule: judge rows and columns by feet and ground features (planters, curbs,
  plaza edges), and stand level with an NPC before talking.
- **Dead reckoning in a town without a map.** In Jubilife, open-street "walk until stopped" legs
  ran far past what was meant, and buildings hid the player; we lost our place twice. Bulbapedia's
  DP map of Jubilife matches the BDSP layout (school, Center, Mart, TV station, Pokétch Company,
  fountain park all where DP has them), so towns get a tile grid too, checked against the
  console, with the story NPCs on it. *Later (r82):* the grid's walls in the open plaza are not
  where the game has them, so it is not good enough to plan on there; see section 11.
- **Not saying what we are doing.** The overlay and the log now print each checkpoint's steps,
  the current one and the next, and every status report starts with the current problem.
  ([`3cd874125`](https://github.com/dayfine/Arduino-Source/commit/3cd8741250dc03f1c166e15cfc9563f5ac36add9))

## 10. Step through new ground before automating it

After hours of whole-run attempts at the lady, five minutes of stepping found her: the Lab
Console takes 5 to 10 commands at a time (walk until stopped, a few presses, a screenshot),
then waits while we look. Where the map or the scene is unknown (a new town, a building,
an NPC to talk to), step through it first, write down each stop and dialogue, and only then
write the automated version from what was seen. 00.05 was done this way on 2026-10-28: Barry and the Town Map, the president, all three
coupon clowns, the Pokétch and the Center PC, about 60 batches, ending in a save at the PC
(segment 0 complete).

## 11. Segment 01 to Oreburgh City (2026-10-28, r81 to r90)

01.01 (Jubilife Center to Barry) and 01.02 (Route 203, Oreburgh Gate, Oreburgh City) were
stepped through by hand once, written from that ([`8dda984d5`](https://github.com/dayfine/Arduino-Source/commit/8dda984d5ca88a1d59b605a3312711162777bba7)), and then run on console B. Ten
runs, each stopped at its first failure, and one fix per failure:

| Run | What happened | Rule / fix | Commit |
|---|---|---|---|
| r81 | West from the PC hit the end of the counter; the walk south then stopped at the stools. | The hand run had gone south first without our noticing. Write down every move of a hand run, including the ones that "did nothing". | [`da01cc799`](https://github.com/dayfine/Arduino-Source/commit/da01cc799ba92eb5723493d943414c881a4a36dc) |
| r82 | A grid leg to a wall the Jubilife map shows walked on ~19 tiles and ended at the Trainers' School. | Jubilife's plaza grid is wrong in its open areas. There, use the holds measured on the console, and end each leg with a search (walk east; at a wall, try the next row). | [`c4883f3cb`](https://github.com/dayfine/Arduino-Source/commit/c4883f3cb2e7c1ea18f1a34a1dd0f82b89d1b2f4) |
| r83 | Barry stopped us, but "only talked" was decided 0.6 s into the pause before his battle. | A trainer's talk and battle have a quiet gap with no text box: wait 4 s of overworld before calling it a talk, and for a story battle never call it one. | [`85ef91790`](https://github.com/dayfine/Arduino-Source/commit/85ef91790d74c0b6b2a9caa88c9890fa07886dfe) |
| r84 | Stuck for 4 min choosing Water Gun at 0 PP; the game refused it each turn. The user spotted the PP risk at the same time. | Read each move's PP by OCR (text = whatever differs from the button's own color, since PP text turns yellow to red); skip 0-PP moves; fail a battle still at its menu after 25 turns. | [`a5f91acde`](https://github.com/dayfine/Arduino-Source/commit/a5f91acdeff362da2e502e33b56e4c5aaf5ddd6a), [`9721801e3`](https://github.com/dayfine/Arduino-Source/commit/9721801e3f258cd49bae506434253df55528648e) |
| r85 | Barry started at 66 % with Water Gun 12/25: the nurse had not healed. Her Yes/No is on her third line, and the B after two A presses answered No. The in-battle Potion (15 % → 93 %) worked. | Count an NPC's lines before choosing how many A presses; an extra A on a later line is harmless, a B on a question is not. The run now also saves after Barry (01.01 split at him). | [`b2236ba72`](https://github.com/dayfine/Arduino-Source/commit/b2236ba72f48e09f237a2ea07b82da802c2efba9) |
| r85 | After the Hiker, a "Hidden Moves" tutorial card held the screen; it is no dialogue box, so the walk read it as a wall. | Tutorial cards close with B; press B after a scene that teaches something. | [`a9863ad8b`](https://github.com/dayfine/Arduino-Source/commit/a9863ad8b73b7941d2b0070025b2415b9913f3a3) |
| r86 | The nurse healed, but `finish_dialogue` ended during the healing jingle; her "Thank you for waiting" then stopped the walk out. | A scene with an animation in the middle needs a second wait for its last lines. | [`00c8fcbf7`](https://github.com/dayfine/Arduino-Source/commit/00c8fcbf7c1f88680ebf54a98e42001918a122a2) |
| r87 | Route 203: only one trainer stopped us, so "after two fights the next wall is the cliff" fired at the stairs between stretches. | Who stops us depends on our row, so trainer counts say nothing about where we are. | [`5345260db`](https://github.com/dayfine/Arduino-Source/commit/5345260dbb113e90cb0174a594561b3d3281bef7) |
| r88 | Reached the north-east corner by the second sign, but the row search counted blocked steps north as moves and never tried the cave's row. | The user's rule: at a wall, go north to the top wall (that pins the row), then try rows south of it one at a time. The cave is two rows below the corner. | [`22998e853`](https://github.com/dayfine/Arduino-Source/commit/22998e853caa8b8529165a33b0c5135511a869a5) |
| r89 | Got into the Gate on the first row, but the cave's fade did not read as a transition; the Hiker's talk was then taken for a trainer's, and his card stayed up. | On Route 203 only trainers start a talk, so a talk with no battle means the Hiker inside the Gate. | [`3cb2e2f78`](https://github.com/dayfine/Arduino-Source/commit/3cb2e2f78631648b132824997e9f8c757cefd657) |
| r90 | **01.02 passed**: trainer, Hiker, Curtis, Diana, the man at Oreburgh's entrance. | | |

Also from these runs:

- **01.02 ends too early.** The man at Oreburgh's entrance walks us to the Pokémon Center,
  where Barry shows up ("Huh? There's someone there."). `finish_dialogue` returned while the
  scene was walking, between lines. The next checkpoint must finish that scene first. The user
  saved by hand there after r90.
- **The lab script's Stop click can miss.** r87's load step left the Lab Console running, so
  AutoStory was refused as "console busy". `run_from_save.sh` now retries Stop until the
  console reads free.
- **Overlay text was cut off** at the log box's width; AutoStory now wraps its overlay lines.
  ([`344a050ce`](https://github.com/dayfine/Arduino-Source/commit/344a050cecc08a61a0f5b528c2c301493c2095e9))
- **What held up:** the in-battle HP read and Potion, the PP read (29, 40, 12 → 11 in Barry's
  battle), the walk out of the Center to the girls below the door, and Barry from the measured
  holds plus row search.

## 12. Oreburgh Mine, Roark's gym and on to Floaroma (2026-10-28/29, r91 to r106, then by hand)

01.03 (city → mine → Roark) was written from the parsed mine map (`devtools/maps/levels.py`)
and run r91 to r106. After that the user asked to fight the gym as we were, gaining
experience by losing, and to reach Floaroma by hand; the console now holds a save in the
Floaroma Pokémon Center (Badge 1, play time 4:23, Piplup Lv 15, Starly Lv 13).

| Run | What happened | Rule / fix | Commit |
|---|---|---|---|
| r91–r93 | Timed holds put us a column off the mine's stairs, sometimes left, sometimes right. | Anchor on walls the map shows (the conveyor pillars, the fence below the Mart); never count on a hold's length for a column. | `1a77905e4` |
| r95, r96 | The walk south stopped early: the hard-hat worker by the fence wanders onto the path. | A stop that a wall cannot explain may be a wandering NPC: wait and walk again before trusting it. | `1a77905e4` |
| r96 | One column off, the walk ends on the sign above the mine's stairs. | The user: being stuck is fine if we can rescue ourselves. Step one tile east off the sign's column; the stairs are open on both sides. | `b3928e6fc` |
| r97 | Blocked legs left us a row high, probing Roark's own row with his rock in between. | End a search on a wall that pins the row (the bottom wall) before probing. | `f84c3ce3c`, `2f8be46f9` |
| r98 | Right below Roark; A started his camera zoom but the first line came after 2 s. | Story NPCs zoom the camera before the first line: wait up to 8 s. | `f59923b67` |
| r99, r103 | The route back walked through the corridor's top into the hall ("unexpected transition"); later the hall and the corridor turned out to be one walk. | End the planned route on a tile a wall pins (the corner at (14,11)) and let one walk do the transitions. | `a414e333a`, `f59a16e51` |
| r101, r104 | Below Roark, A did nothing, by hand too. | **Roark only answers after Barry has sent us** ("go down to the mine and ask him"), so talk to Barry at the gym door first. Confirmed by hand: after that talk, Roark answered the first A. | `a65a02b84` |
| r105, r106 | "Passed", but the nudge before A had stepped onto the rock beside Roark and the rock's one line ("It's a rugged rock…") was taken for his. | A dialogue opening is not proof: check the story effect. Movement is by whole tiles (30, 40 and 60 ms holds all move one tile; 20 ms only turns), so there are no half-tile nudges. 01.03 now steps onto Roark's tile after the talk and fails if he is still there. | `a65a02b84` |

By hand, after 01.03:

- **The gym.** Walking straight up the middle meets both gym trainers (one at the entrance,
  one on the second tier). Roark (Geodude 12, Onix 12, Cranidos 14) beat Piplup Lv 10 and 14
  twice; the third try at Lv 14 with a fixed move choice won and Piplup reached 15. A whiteout
  costs some money and keeps experience, so losing and retrying is a cheap way to level.
- **Battle loop on the Lab Console** (`~/BDSPLab/fight_lc.sh`, with a colour check for the
  four battle buttons in `~/BDSPLab/battlemenu`): Battle, A, then the move; the move list
  needs ~2 s to open, or the Down presses are lost and Pound is used. A double battle needs a
  third A to pick the target. Move-learn prompts need "Keep old moves" (Down, A); B alone
  loops on them.
- **Screenshots lag.** After long walks a `shot` can show a frame several seconds old, which
  made us think the player was stuck. Wait ~3 s before a `shot` that decides the next move.
- **Route 204** (Jubilife → Floaroma): Jubilife's north road starts ~10 tiles west of where
  Route 203 enters; Rowan, Dawn and two Galactic grunts stop us there for a tag battle with
  Dawn. The Ravaged Path needs Rock Smash on its rocks (A, Yes, Yes); its north exit is the
  lit opening a tile west of the corridor's east end, *south* of the entrance chamber, not
  across the water. North of it: up the stairs, then a row down and east along the sand,
  north through the grass to the flower beds, round the twins (a double battle), and north
  between the beds into Floaroma. The Center is a few tiles east of where we arrive.

## 13. Floaroma MVP from a new game, and the lab around it (2026-10-29, r107 to r109)

The MVP branch (`bdsp/floaroma-mvp`, fork #68) runs segments 00 and 01 from a new game on
the shared navigation core. Half of today's failures were in the lab, not in the game.

| Run | What happened | Rule / fix | Commit |
|---|---|---|---|
| r107 | Every program refused to start: "requires video feedback". The camera opened without error but delivered no frames. | Launch lab apps through LaunchServices (`open -n`), never by running the binary from a shell: the app then inherits the shell's (the Claude app's) camera permission, which has none. `stage_launch.sh` now checks for video before anything else. | lab scripts |
| r107 | The MVP build stopped at run time on a missing template ("Unable to open file"). | The worktree's `Packages` linked to upstream's resource set (686 files); BDSP AutoStory needs `~/BDSPLab/packages` (1,467). The build copies `Resources` only when it is missing, so delete it after relinking. | r108 deployment |
| r109 | Mom answered after 1 step; then entering Sandgem the check read the banner as "andgemltown" and failed. | A check tied to an exact OCR string is too narrow (section 2): the place was right. Match banner names loosely, with a test that a different town still fails. | pending |
| r109a | The run never started: the Start click landed where the button was before the window grew to 1554×1488. `stage_run.sh` would then have clicked Start/Stop "to stop the wrong program" and started an unwatched run. | Never toggle Start/Stop blindly: stop only when the log says RUNNING. Better, don't click at all: the lab app gets a control folder (`LabControl/start`, `stop`, `state`). Clicks also take focus from the user's work. | `lab/control-folder` |

What this round adds to the design:

- **Confirm where we are, not just where we planned to be.** The planner plans in tiles and
  the walker counts them, but nothing reads the position back; every drift so far (sections
  1, 5, 11, 12 and the Mom row in section 7) was found only when the next action failed. After each
  leg, confirm with something the game shows (a wall reached, a landmark, a dialogue
  answered, the facing) and replan from the confirmed tile when it differs.
- **Overworld screens belong in the screen machine.** The new-game intro runs on
  `ScreenMachine`; walking, dialogue and battle interrupts should be states of the same loop,
  so a dropped input shows up as an unexpected screen instead of a later timeout.

## 14. Oreburgh 01.03 and 01.04 on console B, and what movement really is (2026-10-31, r178 to r215)

Run 1 (English, Lucas, Piplup) went from 01.03 through beating Roark. It took about 40 builds,
most of them spent guessing at timed walks before measuring.

**Movement physics, measured** (screenshots from a wall anchor; background shift at 640 px width):

| Input | Distance per tap | Spread |
|---|---|---|
| D-pad tap, 160 ms | 40 to 42 px (one tile) | about ±1 px |
| Stick hold, 160 ms | 36 to 48 px | about ±6 px (±0.15 tile) |
| Stick, 2 and 4 holds | 80 to 84 px, 144 to 152 px | the errors add up |
| D-pad, 22 taps from the Oreburgh Center's doorstep | the same tile, under the gym's door, in 3 runs | 0 px |

- **The D-pad moves in whole tiles; the stick doesn't.** Counted stick legs drift about half a
  tile over 18 steps, which missed the gym's door again and again (r204 to r214). The D-pad
  landed on the same tile every run.
- **A 100 ms stick tap toward a new way only turns; a 160 ms D-pad tap turns and steps whichever way the player faced** (r248a, 2026-10-31: all seven first taps moved). It moves nobody, so a door north of us takes a tap to
  face it and then a hold into it.
- **Leaving a Pokemon Center lands on the same doorstep every time** (3 exits from the counter
  save, pixel-identical). The tile-a-time drift we blamed on the exit was the stick.
- **Holding past an event overshoots.** Walks are held in 400 ms chunks and only look between
  chunks: holding through a door's fade walked a tile past the doorstep (r205), and out of the
  gym down to the rocks (r212). Stop on the event (the dark fade, the floor gone), a tile at a
  time.

**How to plan a robust route** (the user's model): each leg is **clip to a wall, then an
offset**.
- **The clip** is a walk until stopped, with the stick: it pins one axis exactly, whatever the
  start was, even with people moving nearby (the time limit counts as reached).
- **The offset** is a count of D-pad taps from that pinned point: exact, as long as nothing
  stands in the way.
- Don't use stick holds for counted legs, and use stick nudges only for a last line-up with a
  person (Roark) or a lit door.
- Start checkpoints at fixed points: a Pokemon Center counter (heal there), a doorstep, a save
  right after a battle that is hard to repeat (01.04 now ends just after Roark).

**Seeing where we are:**
- **Door fades** are often missed by the black-screen watcher: it stops watching when the step
  ends, and the fade comes about 2 s later. Poll mean brightness for a few seconds instead
  (fade 7, outdoors about 100).
- **Inside a Center:** at least 45% warm floor (Oreburgh's sand reads up to 34%).
- **In a cave or gym:** mean brightness under 65 to 85 (Oreburgh Mine 45 to 64, the street 99 to 116).
- **A one-tile scroll can't be told from a scene change** by the view change (106 to 119 vs
  121): don't use the view change for doors.
- **Rocks and signs answer A too.** Check the talk is the one wanted: Roark's zooms the
  camera (view change 86 vs under 10).
- **Battles where there are no wild Pokemon** (gyms, towns, buildings) are trainers': fight,
  never run (r206). Roark's battle menu reads as STANDARD, not TRAINER (r210). The map should
  say where wild battles can happen (PokemonNavigation #11).
- **Lines after a battle** (Roark's badge) keep the menu shut: finish them before a Potion (r211).

**Process:**
- Blind run, patch, rerun cost about 4 hours on 01.03 and 01.04. One hand measurement (4-tile
  chunks with shots, or 10 D-pad taps) settled what several runs didn't. This is now the
  `console-iteration` skill: measure by hand before fixing; stop after 2 same-step failures,
  3 builds or 45 minutes.
- A "pass" proved by timing alone was false twice: Roark's scene never finished (r186 "3/3"),
  and a walk long enough counted as the mine. Prove a pass with something only the right
  outcome shows.
- Keep the game open on failure (the lab option `STOP_AT_FIRST_FAILURE`), so the failure can be
  seen.
- A lab script that starts "whatever program is selected" saved over the only save (01.03's
  start, 03:05). `cf.sh` now refuses unless the Command File Runner is selected.

**TODO:** audit every earlier route (00.04 to 01.05) for counted stick legs and redo them as
"clip to a wall, then D-pad taps", measured on the console.

## 15. Jubilife to Route 204, and where maps come from (2026-10-31, r216 to r246)

Run 1 went on from Roark through the Oreburgh Gate, Route 203, Jubilife's north street,
Route 204 South, the Ravaged Path (Rock Smash) and out onto Route 204 North. 01.10 (the
walk into Floaroma) is the last checkpoint and took eight builds; what they taught is below.

**Maps:**
- **Bulbapedia's DP map images have 12 px rows for 16 px columns.** Every grid traced from
  them (Jubilife, Routes 201 to 204) has 3/4 of a tile per row, and no single offset fixes
  it: a cliff is painted about 24 px tall (two rows) above its tiles, so the lower level
  under the Ravaged Path sits two rows below a flat 12 px grid. Vertical legs planned on such
  a grid are 25 to 35% short. An image parser with tall grass, shadows and stairs fixed still
  called half the open tiles walls (Route 204 North: 80% of tiles exact, 177 false walls).
- **The Pokemon Platinum decompilation (pret/pokeplatinum) gives the real tiles**: each
  32x32 cell's collision bit and behaviour (tall grass, ledges and their direction, warps,
  doors), plus an events file with every NPC, their facing and wander range. Route 204 North,
  Route 204 South and the Ravaged Path were rebuilt from it and matched the console at every
  anchor probed. BDSP differs in spots: trainers stand a few tiles from Platinum's, one
  column of the grass block's east edge is a wall on the console, the Potion lady on Route 201
  wanders. Terrain from the decomp, people and such edits from the console.
- **Say where each map comes from.** Every map file now opens with its source, its console
  checks and the checkpoints that pass on it (`Data/MAPS.md`); a map change re-runs them.
- **A wandering NPC is not a wall.** Marking the Youngster's two rows under the cliff as
  tiles to avoid took away the only straight row to the stairs. The map stays true; the walker
  waits for him or talks to him.
- **One map is one connected component.** Route 204's halves meet only through the cave:
  split (fork #97, PokemonNavigation #27). A ledge is one way (0x3b, jump south).

**Routes:**
- **The planner's cheapest route is not a human's.** It ran to a wall and tapped back
  (cost 35) instead of nine taps (41), hugged the grass block's corner through tiles nobody
  had walked, and bounced off walls. The human did it in 18 presses: south 1, west along the
  row under the cliff, up the middle of the stairs, east into Aroma Lady Taylor's battle,
  north through the grass, west under the flower beds, north into town. That line, as
  waypoints (`walk_waypoints()`), with costs that let a straight tap leg win, is the route.
- **Verify every tile a route touches** by a human walk or a D-pad probe before coding. The
  corner at column 23 and the fourth row under the cave were planner choices never walked.
- **Record the human.** A shot a second of the lab window plus the app log is enough; the
  keyboard shows in the log only as `controller_state`, so the shots carry the moves.
- **Draw the route on the image only with the cliffs accounted for**; a flat overlay put the
  row under the cliff across the cave mouth and looked right.

**Walker:**
- **Time-counted legs break when something blocks them.** "West 10 to the wall" met the
  Youngster, counted 8 tiles for 4, and every later leg was five columns off (r244a). Open
  legs are whole-tile D-pad taps now; stretches between a line's corners plan as taps.
- **A tap's screen change: a step 21 to 50, a bump into a wall or an NPC brushing past 4 to
  13.** The old threshold of 10 counted bumps as steps up the cliff and "arrived" (r244a);
  tap legs use 16. Measured from the logged per-tap values and the recording.
- **A blocked tap leg waits 2.5 s and taps the rest**, three times, then talks (the Youngster
  answers), marks the blocker and plans again from the exact tile the taps give.
- **Rock Smash's prompt is answered by its Yes/No cursor**, not timed presses (r237a said
  No); "someone in the way" in a cave tries the prompt before a talk.

**Process:**
- **A stale build ran twice.** A background build failed on a header I had broken, a grep
  filtered the error out, the tests ran on the old binary, and the deploy copied the 18:29
  app for the 18:57 fix (r245a was r244a again). Check the binary's time against the source
  edit before any console run; build in the foreground when the result decides the run.
- **The three-build stop exists to force a hand measurement.** 01.10 went to eight builds;
  the measurement, once made (the human's walk), gave the route in 30 s and the parser and
  overlay errors in an hour.
- **An image parser is a check, not a source.** Each of its mistakes is a hand rule per map
  for an approximation of what the decomp states exactly.
