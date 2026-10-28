# AutoStory: what broke on a real console, and the rule each break taught

Each entry is a failure seen on a Switch while running BDSP AutoStory (Developer-Butters'
U#1717 plus our intro work, October 2026), the pattern behind it, and the rule we now follow.
Runs are the lab builds r14 to r39 on console B. The patterns repeat, so the rules matter
more than the individual fixes.

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

## 6. Next: a map instead of recorded legs

BDSP keeps DP's overworld layouts: Bulbapedia's DP map of Route 202 (37 x 26 tiles of 16 px)
matches our console B screenshots (the 202 sign by the L-shaped path, the eastern tree wall, the
Potion giver at the edge of the grass by the trees). The DP maps can be turned into a walkability
grid per route: walls, grass, ledges, doors, NPCs and trainer sightlines.
Planned legs then end at known walls; the route banner (OCR) and the walls we hit tell us where
we are. This replaces the recorded stick movements, and FRLG can use the same tool.
