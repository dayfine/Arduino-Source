# AutoStory segment 01 plan: Jubilife to the Coal Badge

**Done 2026-10-29:** 01.01 to 01.03 were built from this plan and run on the console
(r81 to r106; Lessons §11 and §12), and Floaroma was reached by hand. The MVP branch
`bdsp/floaroma-mvp` (fork #68) carries the code. Kept as the plan of record.

Status 2026-10-28: research done, nothing built yet. Companion to
[AutoStory-Design.md](AutoStory-Design.md) (the pieces) and
[AutoStory-Lessons.md](AutoStory-Lessons.md) (what went wrong in segment 00).
Testing is done by an agent with the `bdsp-autostory-test` skill
(`~/.claude/skills/bdsp-autostory-test/SKILL.md`).

## 1. Starting point

- **Save on console B:** end of segment 00, at the Jubilife Pokémon Center PC (play time 1:53,
  "Last saved 01/01/2026 01:00"). This is 01.01's start, so segment 01 can be stepped through
  and tested **without a new game**.
- **Party:** Piplup alone, about Lv 10 to 11 after the 00.04 training and three Route 202
  trainers, knowing Water Gun. Bag: the rest of the 10 Potions, Poké Balls, no money worth
  speaking of beyond the trainer prize money.
- **Help mode:** `HELP_MODE = no_outside_help`. Butters' segment 01 assumes the starter was
  swapped for a **level-100 Pokémon from HOME** at that PC (his 01.01 start text: "Received
  Starter back from Pokemon Home. Max leveled pokemon"). With that, every battle is one
  attack. Without it we must win with a Lv 10 to 15 Piplup, so training, healing and move
  choice matter here, unlike in his version.

## 2. The story, step by step

| # | Where | What happens | Ends when |
|---|---|---|---|
| 1 | Jubilife Center | (optional) heal at the nurse; buy Potions at the Mart next door if money allows | back on the Center's front step |
| 2 | Jubilife east exit to Route 203 | Barry stops us at the route entrance: **rival battle**, Starly Lv 7 then Turtwig Lv 9 (we chose Piplup) | Barry's dialogue ends |
| 3 | Route 203 | trainers in sight along the way: Youngster **Dallas** (Kricketot 7), Youngster **Sebastian** (Machop 7). Off-path: Youngster Michael (Bidoof 5, Zubat 5), Lass Kaitlin (Bidoof, Budew, Starly 4), Lass Madeline (Starly 5, Bidoof 5) | the Oreburgh Gate door (cave entrance, east end) |
| 4 | Oreburgh Gate 1F | a Hiker gives **HM06 Rock Smash** (dialogue only). Camper **Curtis** (Shinx 7), Picnicker **Diana** (Budew 5, Psyduck 5). Wild: Geodude 4 to 8, Zubat 6 | white-screen exit east into Oreburgh City (end of 01.01) |
| 5 | Oreburgh City | a man stops us at the entrance and takes us on a short "tour" (dialogue); Roark is not in the gym yet | mine entrance (south of the city) |
| 6 | Oreburgh Mine 1F, B1F | Worker **Colin** (Geodude 7, Onix 7), Worker **Mason** (Machop 9); Roark is at the far end of B1F, talks, smashes a rock, sends us to the gym. Wild: Geodude 5 to 10, Zubat 7 to 8, Onix 8 to 9. Items: X Defense 1F, Potion and Escape Rope B1F | back outside (walk or Escape Rope), then gym door (end of 01.02) |
| 7 | Oreburgh Gym | tiered floor with stairs (BDSP layout, not DP's); one or two gym trainers that bridges let us skip. **Roark**: Geodude 12 (Stealth Rock, Defense Curl, Rollout), Onix 12 (Stealth Rock, Rock Throw, Bind), Cranidos 14 (Headbutt, Bulldoze, Leer; Mold Breaker) | Coal Badge, standing at Roark (end of 01.03) |

Sources: Serebii's BDSP pages for Route 203, Oreburgh Gate, Oreburgh Mine and the gym list
(Bulbapedia's text pages are behind a Cloudflare check; its image archive is not). Trainer
lists for the gym and the exact Roark location in the mine are still to confirm on the console.

## 3. Battles: what Piplup is up against

Our move picker uses the strongest known attack (Water Gun) every turn. Where that is wrong:

| Fight | Problem | Plan |
|---|---|---|
| Barry's Turtwig Lv 9 | Grass resists Water: Water Gun with STAB ≈ 30 power, Pound 40 | add type effectiveness to the move picker (step 3 below); Potions under half HP |
| Roark's Geodude / Onix | Rock/Ground, Water is ×4 | Water Gun is right |
| Roark's Cranidos Lv 14 | Headbutt hurts; Mold Breaker irrelevant | Water Gun ×2; reach Lv 14 to 15 first; Potion under half |
| Wild battles | run, as in 00.04 | unchanged |

**Levels.** Aim for Piplup Lv 14 before Roark. The trainers on the way (Barry, Dallas,
Sebastian, Curtis, Diana, Colin, Mason) give roughly 3 to 4 levels; if short, train on
Geodude in the mine (Water Gun one-shots them, good experience).

**Losing.** A whiteout returns us to the last Center with half the money. Detect it (black
screen into a Center, or the "can't battle" party screen) and fail the attempt with a clear
message; do not continue blindly.

## 4. Maps and grids

Following segment 00's rule: plan routes offline on tile grids traced from Bulbapedia's DP maps,
and put NPCs/trainers found on the console onto the grid.

| Map | Source image | State |
|---|---|---|
| Route 203 | `Sinnoh_Route_203_DP.png` (976×368, three stitched panels; check that the panel joins sit on the 16 px grid) | downloaded, to trace |
| Oreburgh Gate 1F | filename not found yet (`Oreburgh_Gate_DP.png` etc. 404) | find the file name, or trace from console screenshots |
| Oreburgh City | not found yet | same |
| Oreburgh Mine 1F, B1F | `Oreburgh_Mine_B1F_DP.png` (400×396); 1F not found | B1F downloaded |
| Oreburgh Gym | BDSP layout differs from DP; `Oreburgh_Gym_BDSP.png` is one screenshot | trace from console screenshots |

Archive URL: `https://archives.bulbagarden.net/media/upload/<h0>/<h0h1>/<File>`, h = md5 of the
file name. Downloads go to the session scratchpad, not the repo; only the ASCII grid is
committed.

Butters' segment 01 (fork branch `bdsp/autostory-1717`, `Segment_01.cpp`, 2111 lines) is
mostly timed stick holds (`pbf_move_left_joystick(…, 2694ms)`) plus picture matching inside
the mine (crate and rock patches compared at shifted positions). Useful as a record of the
order of events and dialogue counts, not as code to keep: the same timed-leg style failed in
00.04 and 00.05 (Lessons §6).

## 5. Work order

1. **Trace Route 203** into `Data/PokemonBDSP_AutoStory_Route203.{h,cpp}`, joined to Jubilife's
   east exit in a `MapSet`. Offline plan tests for Center → Barry → Gate.
2. **Step through 01.01 on the console** from the current save with the Lab Console
   (batches of 5 to 10 commands with a screenshot each): Barry's trigger tile, trainers'
   positions and sight, the Gate's Hiker, Curtis and Diana, the white exit. Record each stop.
3. **Type-aware move choice**: read the opponent's species name (OCR of the battle text box),
   look up its types, and score moves by power × STAB × effectiveness. Unit tests on saved
   battle screenshots.
4. **Write 01.01** on the grid with `walk_route`, `talk_to_npc`, `finish_dialogue` and the
   trainer handling from 00.04. Save at the Oreburgh Center when it ends.
5. **Gate, city and mine grids** (from the images or console screenshots), then step through
   01.02, then write it. Decide walk-out vs Escape Rope after seeing the mine.
6. **Gym**: step through, write 01.03 with a heal at the Oreburgh Center first.
7. **Test** each checkpoint twice from its start save with the testing skill; then the whole
   segment once from the end-of-00 save.

## 6. Open questions

- Is Barry's trigger a tile (walk-in) or a line across the route? Step 2 answers it.
- Do Route 203's trainers stop us on the shortest path, or can the plan go around the optional
  ones? We want the experience, so the plan should walk into Dallas and Sebastian on purpose.
- Money: enough for Potions after Barry (prize money) or not? Read it in the Mart.
- Where exactly is Roark in B1F, and does the scene move us?
