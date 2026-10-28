# AutoStory: what broke on a real console, and the rule each break taught

Each entry is a failure seen on a Switch while running BDSP AutoStory (Developer-Butters'
U#1717 plus our intro work, October 2026), the pattern behind it, and the rule we now follow.
Runs are the lab builds r14 to r79 on console B. The patterns repeat, so the rules matter
more than the individual fixes. Sections 6 to 8 cover r40 to r79, which took 00.04 from the
catch to Dawn in Jubilife unattended (r79).

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

## 5. Our own process

- Look at the screen, then the code, before rerunning. Every run since r28 that failed in a new
  way was explained by one screenshot.
- Put new code in its final module first; no copy-then-move PRs of our own code.
- Keep line endings as they are (CRLF files stay CRLF); compare `git diff --stat` with
  `--ignore-cr-at-eol`.
- When a program is stuck or failing, stop it and close the game at once; don't let retries run.

## 6. Butters' 00.04 and 00.05, checked against the game

What his recorded version does, what went wrong with it on console B, and what replaced it. Our
changes are on the fork branch
[`bdsp/new-game-intro-v2`](https://github.com/dayfine/Arduino-Source/commits/bdsp/new-game-intro-v2),
on top of his segment 00 ([`bdsp/1717-seg00`](https://github.com/dayfine/Arduino-Source/compare/503a28fc6...bdsp/1717-seg00)).

| His version | What happened | Now | Our commits |
|---|---|---|---|
| Every leg is a stick hold of a recorded length (00.05 alone is ~150 lines of them). | Drift, as in section 1. In r79 the 00.05 legs reached the Trainers' School, then "west 200 ms, A" faced an empty tile: Barry stands up and to the right, at the blackboard. | 00.04 walks map plans whose legs end at walls (design doc, sections 3 to 5). 00.05 is being stepped through (section 8 below). | [`1598fc2c7`](https://github.com/dayfine/Arduino-Source/commit/1598fc2c74e7cccf6356251bf35e21fdbcac5006), [`0a5311175`](https://github.com/dayfine/Arduino-Source/commit/0a5311175ec72feb0fcb326a38d7e2dc7b1ec673) |
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

## 7. Our own mistakes in this round

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
  console, with the story NPCs on it.
- **Not saying what we are doing.** The overlay and the log now print each checkpoint's steps,
  the current one and the next, and every status report starts with the current problem.
  ([`3cd874125`](https://github.com/dayfine/Arduino-Source/commit/3cd8741250dc03f1c166e15cfc9563f5ac36add9))

## 8. Step through new ground before automating it

After hours of whole-run attempts at the lady, five minutes of stepping found her: the Lab
Console takes 5 to 10 commands at a time (walk until stopped, a few presses, a screenshot),
then waits while we look. Where the map or the scene is unknown (a new town, a building,
an NPC to talk to), step through it first, write down each stop and dialogue, and only then
write the automated version from what was seen. 00.05 was done this way on 2026-10-28: Barry and the Town Map, the president, all three
coupon clowns, the Pokétch and the Center PC, about 60 batches, ending in a save at the PC
(segment 0 complete).
