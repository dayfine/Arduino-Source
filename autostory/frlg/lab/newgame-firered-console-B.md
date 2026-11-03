# FireRed new game on console B, by hand through the Lab channel (2026-11-01)

Purpose: the observed sequence is the spec for the FRLG new-game checkpoint
(program code on its own branch, collected into frlg/mvp later). Each row is
one CFR batch; requests are in `lab-B/Lab/done/<name>.txt`, journals and
screenshots in `lab-B/Lab/outbox/<name>.out`, `<name>_<shot>.png`.

Setup: lab app build frlg-l0 (lab/frlg 536f4eb92) on profile lab-B, console B,
lock `frlg-mvp-session`, channel owner lab-B. Game: Pokémon FireRed (Switch, NSO
GBA-style app), English, user profile "di". Cartridge had no save.

| Batch | Inputs | Screen after | Notes for the program |
|---|---|---|---|
| look0 | shot | color bars | capture shows bars = console asleep (no HDMI); not "no video" |
| wake1 | HOME, HOME | HOME menu, Pokémon HOME "Playing" | first press wakes; second press is taken |
| close1/close2 | X, X | close dialog | first X after wake only connected the controller (icon appears bottom-left) |
| tile1 | A, RIGHT×4 | FireRed tile highlighted | HOME order: HOME, BD, Violet, LG, FR, … (order = last played) |
| boot1/boot2 | A; A (user picker) | intro movie (Gengar vs Nidorino) | user-select screen appears at launch; ~20 s to movie |
| title1 | A, A | controls tutorial page 1 | no save → title A goes straight to New Game (no Continue menu) |
| intro-a | A×5 | tutorial last page "Press the A Button…" | tutorial = 5 A pages |
| oak-a, oak-b | A×19 (1.5–2 s apart) | "Now tell me…" | Oak's text prints slowly; 1 A per line, ~2 s/line |
| gender-a | A, A | "Let's begin with your name" | A on boy/girl menu = BOY (default cursor) |
| name-a | A | keyboard "YOUR NAME?" | Switch release: no preset-name list, keyboard opens directly; cursor on A |
| name-empty | PLUS, A | "Right… So your name is KAMON." | empty name + OK (PLUS jumps to OK) = game picks a default name: no typer needed for the MVP |
| rival-a, rival-b | A×6 | rival keyboard with a stray "A" typed | the keyboard opens right after Oak's line; one A too many types a letter. Program: detect the keyboard screen, don't count A presses |
| rival-empty | B, PLUS, A | "…Er, was it GREEN?" YES/NO, cursor on YES | B deletes a letter; empty rival name = GREEN, asked with YES/NO (player name was not asked) |
| oak-end | A×9 | shrink-to-sprite fade, then black | A=YES (GREEN); rest of Oak's speech ~8 lines |
| room-a | wait 6 s | bedroom (Red's house 2F), player on the rug facing N at the TV, stairs NE | **Step 1 outcome reached**: overworld, controllable. Total intro ≈ 5 min by hand |

New-game sequence for the program (step 1, "new game to bedroom"):
1. HOME → FireRed tile (verify by shot) → A → user picker A → movie → A (title) → A (no save: New Game).
2. Controls tutorial: A until the tutorial page is gone (5 pages).
3. Oak: A per line until the boy/girl menu → A (BOY).
4. Name keyboard: detect it → PLUS, A (empty name → game default, e.g. KAMON).
5. Oak: A per line until the rival keyboard → detect it (do not over-press) → PLUS, A → "Er, was it GREEN?" YES → A.
6. A per line until the screen goes black; wait until the bedroom shows (overworld detector).
| save-a..c | PLUS, DOWN×2, A, wait, A | menu BAG/KAMON/SAVE/OPTION/EXIT → card "PALLET TOWN, KAMON, 0 badges, 0:01" → YES → back to overworld | **Saved 00.01.** START menu before Pokémon: BAG, name, SAVE, OPTION, EXIT (SAVE = 3rd). Card shows map name: usable as the location check |

## Checkpoints (Brock MVP), from the pokefirered story scripts; saved at the end of each

| Id | From → to | Story event / check | Status |
|---|---|---|---|
| 00.01 | new game → bedroom | intro, names (empty → defaults), overworld visible | ✅ by hand, saved 0:01 |
| 00.02 | bedroom (2F 6,6) → Pallet Town outside the front door | stairs warp 2F (10,2) → 1F; door 1F (4/5,8) | |
| 00.03 | Pallet → north exit → Oak stops you → lab | trigger at Pallet's north edge; Oak walks you to his lab; cutscene ends in the lab | |
| 00.04 | choose a starter → rival picks → rival battle when leaving | balls on Oak's table; battle triggered walking toward the door | |
| 00.05 | Route 1 → Viridian City → Mart → Oak's Parcel | entering Viridian Mart gives the parcel | |
| 00.06 | back to Pallet → Oak's lab → Pokédex (+ Poké Balls) | deliver parcel to Oak | |
| 00.07 | Viridian Mart FIRST (7 Potions + Antidotes; user: "next time we should do that first"), heal + verify HP, old-man tutorial, Route 2, Viridian Forest, Pewter, heal | trainers in the forest; Pewter Center heal | |
| 00.08 | Pewter Gym → Brock | Jr. Trainer, then Brock | |

### 00.02 attempts (frlg_route PalletTown_PlayersHouse_2F → PalletTown 6 8)

| Run | Build | Result | Layer / cause | Fix |
|---|---|---|---|---|
| r1 | frlg-m1 | parse error | direction words are lowercase (`south`, not `SOUTH`) | use lowercase |
| r2 | frlg-m1 | **app crashed** (SIGSEGV in run_until) | lab process: the Lab channel's console had no inference pivots (#107 never called initialize_inference_threads) | #107 5c2621859 |
| r3 | frlg-m2 | "Failed to flee battle" after east 5 to the wall | perception: a wild-battle watcher fired indoors; the screen darkens at the small room's edge like a flash | frlg/walk-wild-legs 43ac578a0: watch for battles only on steps over grass/water |
| r4 | frlg-m3 | "a message didn't close" on north 4 (to wall) | map/route: pressing north into the wall poster at (11,1) reads it (FRLG walk-into signs, `TrySetUpWalkIntoSignpostScript`: N/S holds into MB_SIGNPOST / Center / Mart signs); the map has no sign tiles | next: mark signs in frlg_maps.py, planner never holds N/S into a sign |
| r5 | frlg-m5 (+ sign tiles) | ✅ ends at PalletTown (6,8) below the door, banner PALLET TOWN | new full plan: east 4, north 4 (to wall) in column 10, west 1 onto the stairs, west 6, south 7 |
| save0002 | PLUS, A, A, (wait) A | card PALLET TOWN 0:18 → "There is already a saved file. Is it okay to overwrite it?" → YES | **Saved 00.02.** START menu remembers the last entry (cursor was on SAVE): read the cursor, don't count from the top. Second save asks to overwrite |

Hot swap: FRLG maps are read from Resources at run time (no rebuild), but `overworld_maps()` caches them in a static: an edit needs an app restart.
| cp0003-a | frlg_route PalletTown 6 8 → 12 2 north | ✅ south 2 (to wall), east 6 (to wall), north 8 → (12,2) below the gap | Oak's trigger: coord events (12,1)/(13,1), VAR_MAP_SCENE_PALLET_TOWN_OAK==0 |
| cp0003-b..d | dpad UP (1 step); wait; A×3; A | "Don't go out!" closes by itself; Oak walks in; "It's unsafe!…Here, come with me!" (last line waits for A, no arrow); game walks the player into the lab | lab scene starts by itself after the warp to Oak's Lab (6,12) |
| cp0003-e..k | A×~30, then B×7 | lab scene: GREEN "Gramps!…", Oak's speech, "Which one will you choose…" | text prints slowly (~2 s/line). Once the scene ends, A talks to Oak again ("Now, KAMON…"): **advance story text with B**, which never starts a talk. Program: set OPTION text speed FAST first |
| save0003 | PLUS, A, A, A | card 0:24 → overwrite YES | **Saved 00.03**: in the lab, player (6,4) facing N below Oak, Green (5,4); scene var = 2 (choose a starter) |

### 00.04 plan (pokefirered PalletTown_ProfessorOaksLab scripts)
Balls: Bulbasaur (8,4), Squirtle (9,4), Charmander (10,4). Stand on row 5 below one, face N, A → Oak faces right, picture, "So, you want the WATER POKéMON SQUIRTLE?" YES/NO → YES → "This POKéMON is really energetic!" → received + fanfare → "give a nickname?" YES/NO → NO → rival walks to the ball strong vs ours (Squirtle → Bulbasaur), scene var 3. Walking down, (5|6|7, 8) triggers the early-rival tutorial battle (losing continues the story); rival leaves, scene var 4. Row 8 before choosing = "Hey! Don't go away yet!" (scene var 2): avoid.
| cp0004-a..f | frlg_route lab 6 4 → 9 5 north; A; A (YES); B (nickname NO); B×5 | Squirtle picture, "Hm! SQUIRTLE is your choice… you've decided on the WATER POKéMON SQUIRTLE?" YES/NO (YES default) → received → nickname YES/NO → B = NO → Green takes Bulbasaur, stands at (8,5) | B on a YES/NO = NO; A = YES (cursor starts on YES) |
| cp0004-g,h | frlg_route lab 9 5 → 7 7 south; dpad DOWN | "GREEN: Wait, KAMON! Let's check out our POKéMON!" | trigger (5\|6\|7, 8) with scene var 3 |
| cp0004-i..p | B to the menu; per turn: B×4, A (FIGHT), A (TACKLE, top-left); B×~28 after | Oak narrates first (tutorial); battle menu FIGHT/BAG/POKéMON/RUN, moves TACKLE / TAIL WHIP; won in 5 Tackles, Squirtle Lv 6, 12/21 HP; Oak "Hm! Excellent!"; Green leaves | B does nothing on the battle menu, so "B×4 then A, A" is a safe turn loop. Mash B for text everywhere (user: "in many situations you just need to mash A or mash B") |
| save0004 | PLUS, DOWN, A, A, A | menu now POKéMON/BAG/KAMON/SAVE/OPTION/EXIT; cursor kept index 2 = KAMON | **Saved 00.04**, lab (7,8), 0:34. The START menu keeps the cursor *index*, and items get inserted (POKéMON) → read the cursor, never count |

Program helpers wanted (lab port first, then program code): `frlg_dialog` detector (WhiteDialogDetector), `frlg_battle_menu` detector, `mash BTN ms` on the Lab channel, `frlg_save` (open menu, find SAVE by reading the cursor, YES, overwrite YES).
| cp0005-a | frlg_route PalletTown_ProfessorOaksLab 7 8 → ViridianCity 36 20 north | ✅ 80 s: 15 legs over Route 1's ledge terraces; 2 real wild battles, both fled ("Successfully fled the battle"), walker re-walked the leg | wild-step watcher fires correctly on grass |
| cp0005-b,c | dpad UP (enter Mart); B×12 | "Hey! You came from PALLET TOWN?" → clerk walks the player to the counter, gives OAK'S PARCEL | the parcel scene is a map script on entering (no coord event) |
| cp0005-d..f | PLUS, UP×2 (BAG), A, RIGHT (KEY ITEMS), B | OAK'S PARCEL in KEY ITEMS | effect confirmed. B from the bag returns to the START menu |
| save0005 | DOWN×2, A, A, A | card VIRIDIAN CITY 0:38 | **Saved 00.05**: Viridian Mart, at the counter beside the clerk |
| cp0006-a | frlg_route Mart 3 3 → lab 6 4 | ❌ "no fade after walking onto the warp": stopped on Mart (5,7) beside the mat | my start tile was wrong: the clerk's scene leaves the player at (4,3) (clerk 2,3, pillar between). Program: the scene's end tile is fixed, take it from the script, not from a screenshot guess. Mart: only (4,7) is the exit mat although the map has 3 warp events |
| cp0006-b | frlg_route Mart 5 7 → lab 6 4 | ❌ "Interrupted 6 times on: south 20": in Pallet, held south into GREEN's mailbox (sign (13,7)) | walk/physics: the player was in column 13, not 12 (measured from the doors (6,7)/(15,7) in the shot). One column drift on Route 1, after a late-seen wild battle on "south 7" (walker: "about 0 tiles walked, 7 to go") then the open "east 4". Follow-up: after an interrupt, re-anchor on a wall before open legs; check the column on entering a map |
| cp0006-c..g | B (close mailbox); frlg_route PalletTown 13 6 → lab 6 4; A (talk to Oak); B×~75 | parcel delivered, Green comes in, Pokédexes taken from the desk, catching talk, Green leaves | START menu now POKéDEX/POKéMON/BAG/name/SAVE/OPTION/EXIT (POKéDEX = received) |
| save0006 | PLUS, DOWN (cursor was on KAMON), A, A, A | | **Saved 00.06**: lab (6,4) under Oak |

### 00.07 attempts (lab → Viridian Center → Route 2 → forest → Pewter)
| batch | inputs | screen | notes for the program |
|---|---|---|---|
| cp0007-a | frlg_route lab 6 4 → ViridianCity_PokemonCenter_1F 7 4 north (no start facing) | ❌ before moving: "Unable to open file …/PokemonFRLG/PlayerSprites/red_normal.png" | facing reader needs PlayerSprites resource (not in lab build). Always pass start facing until packaged |
| cp0007-b | same + start facing north | ❌ "Interrupted 6 times on: north 4 (to wall)": actually stuck under the Route 1 sign (9,31) | **drift #2**: "west 4" in grass from (12,32), wild battle froze the player on tile 3, flash seen after the hold → "a wild battle at the end of the leg" → player at (9,32), next "north 5" holds into sign. Walker fix delegated (branch frlg/walk-interrupt-recount) |
| cp0007-c | B×3; frlg_route Route1 9 32 → Center 7 4 | ✅ | |
| cp0007-d | A×4, B×4 at nurse | dialog closed but **not healed** (12/21 later in battle) | heal sequence unverified: program must check the "healed to perfect health" line / HP. Blackout later still returned to Viridian Center |
| cp0007-e | route Center → Viridian 21 7 (west of old man col), dpad LEFT | ✅ (20,7) | old man object (21,6); coord triggers (20,8)/(22,8) scene var 1 |
| cp0007-f | frlg_route Viridian 20 7 → forest | ❌ "a message on the overworld didn't close": old man tutorial started anyway at (20,6) | tutorial is unavoidable in practice; program: walk into it, mash B ~60 lines (≈90 s), get TEACHY TV. Player ends (20,6), old man (21,6) |
| cp0007-g,h | B×60 | tutorial done | |
| cp0007-i | frlg_route Viridian 20 6 → ViridianForest 29 61 | ✅ 37 s (Route 2 south) | |
| cp0007-j | frlg_route forest 29 61 → Pewter Center | ❌ "no fade … warp to NorthEntrance": 4 late-seen wild battles, "about 0 tiles walked" estimates; ended (20,55) | **drift #3** (forest). Localized from screenshot: tree block x17-25 rows 51-54 |
| cp0007-k | route forest 20 55 → 13 46 | ✅ verified by shot | short hops + shot work as a fallback |
| cp0007-l | route forest 13 46 → 20 33 | ❌ trainer (Bug Catcher) "Come on! Let's battle 'em!" → message didn't close | trainers: need sight-line awareness or battle handler on the walker |
| cp0007-m..p | battle loop B×4–6, A, A | Weedle beaten (Squirtle Lv7, learned Bubble), Caterpie next; poisoned 12→0 → **fainted, blackout** to Viridian Center (healed) | MVP needs: heal check, Potions/Antidotes, levels (Lv ~12+ for Brock), fight wild battles for EXP instead of fleeing in the forest |
| cp0007-r..t | B×10 (blackout text); frlg_route Center → Mart | ❌ planner "south 2 (to wall)" from (26,27): row 30 is a ledge 'J' → player **hopped the ledge** to (26,31), then "north 10" blocked | **planner bug: a ledge is not a wall in its jump direction (south)**; "to wall" legs must not end against a south ledge |
| cp0007-u | frlg_route Viridian 36 31 → Mart 4 3 west; A | ✅ BUY/SELL/SEE YA | Mart clerk from (4,3) facing west |
| cp0007-v..zf | BUY: Potion ×4 (₽1200), Antidote ×2 | money 3024 → ~1624 | quantity UP taps are dropped while "How many?" text still prints: wait ~2 s before UP; confirm = A, wait 3–4 s, A (YES), A. Cursor returns to POKé BALL after each buy |
| sv0007a | PLUS (cursor already SAVE), A, A (overwrite YES), A | card "VIRIDIAN CITY, KAMON, 0 badges, POKéDEX 1, 1:12" | **Saved 00.07a: Viridian Mart (4,3), healed, 4 Potions, 2 Antidotes, Squirtle Lv7 (Bubble)** |
| cp0007-zg..zx | route Mart → forest 29 61 → 13 46 → (east path) ; Bug Catcher (Rick) at (43,44) by (42,44); battle loop; Potion in battle; Antidote + Potion from START→BAG | ✅ won, Squirtle Lv8 | battle menu remembers cursor (was on BAG): battle loop must LEFT+UP before A, and LEFT+UP in move list. Bag in battle opens on the last pocket (KEY ITEMS) → LEFT to ITEMS. Poison does 1/8 per turn: Antidote early |
| cp0007-zy | B×5 then frlg_route forest 42 44 → 39 25 | ⚠️ "ok" but **nothing moved**: START menu was still open, walker reported all 3 legs done | **walker gap: no check that the overworld is up before/after a leg; "(to wall)" legs pass with zero movement** |
| cp0007-y1..m6f | forest hops; wild Caterpie fought (Lv9); lab build → **frlg-m6** (2dcf8bc60 = +#119 tile-at-a-time); trainer Weedle Lv9 (walker tried to flee → "Failed to flee… 5 attempts"; fought by hand); route gate → Pewter Center ✅ | #119: late-seen battle "10 of 18 tiles walked" recovered ✅. Regressions sent to agent: warp fade on grass→warp step read as battle; trainer battle after message must not flee |
| cp0007-heal, sv0007 | A, A (YES), A wait 6 s, B×3; START (cursor on EXIT now) UP×2 → SAVE, A, A, A | healed (heal anim seen); card "PEWTER CITY, 0 badges, 1:35" | **Saved 00.07: Pewter Center (7,4), Squirtle Lv9 full HP, 2 Potions, 1 Antidote.** START cursor remembered = EXIT (after last close via B? no — closes leave cursor) → always read cursor |

### 00.08 attempts (Pewter Center → Gym → Brock)
| batch | inputs | screen | notes for the program |
|---|---|---|---|
| cp0008-a | frlg_route PewterCity_PokemonCenter_1F 7 4 → PewterCity_Gym 6 6 north | route "ok" but **Camper Liam (3,8) stopped us at (6,8)**: "Stop right there…" | gym trainer sight 4 covers column 6 at row 8: expect the trainer battle mid-route |
| cp0008-b..g | B×10; battle: LEFT,UP,A (FIGHT) then LEFT,UP,DOWN,A (BUBBLE) ; B×7 per turn | Liam: Geodude Lv10, Sandshrew Lv11; won; Squirtle Lv11 (learned WITHDRAW at 10 automatically: 4th slot was empty) | move menu: TACKLE / TAIL WHIP / BUBBLE / WITHDRAW. Bubble = bottom-left |
| cp0008-h,i | route gym 6 8 → Center 7 4; A, wait until YES/NO (~5 s), A, wait 8 s, B×4 | healed to full (verified 31/31 in next battle) | **heal recipe: A; wait for YES/NO box; A; wait 8 s; B until overworld**. Earlier A,A,A,A at 1.5 s did NOT heal reliably |
| cp0008-j..q | route Center → Gym 6 6 north; A; B×28 (intro); Bubble loop; Lv13 "Stop learning WATER GUN?" → **A (Yes)**; B×55 | Brock Geodude Lv12, Onix Lv14 beaten with Bubble; Boulder Badge + TM39 | move-learn prompt with 4 moves: "delete a move?" B=NO → "Stop learning X?" must answer **A** (B=NO loops back) |
| sv0008 | START (cursor SAVE), A, A, A | card "PEWTER CITY, KAMON, BADGES 1, POKéDEX 1, 1:48" | **Saved 00.08: Pewter Gym (6,6) facing Brock, 1 badge. FireRed MVP done by hand.** |

## MVP summary (by hand, 2026-11-01)
Total game time 1:48. Squirtle Lv13 alone suffices with Potions/Antidotes; key moves Tackle → Bubble.
Program must handle: text mash (B), YES/NO (A), START menu cursor, save, heal recipe, shop, battle loop (FIGHT/move index, cursor reset LEFT+UP), Potion/Antidote use, trainer battles (never flee), wild battles (flee or fight), move-learn prompt, blackout recovery, old-man tutorial, warp-on-grass.

## Deleting save data (console B, 2026-11-01)
- System Settings → Data Management on console B shows Quick Archive, Manage Software, a **blank (unselectable) row**, Save Data Cloud, Transfer Your Save Data, Manage Screenshots and Videos. "Delete Save Data" is not offered (probably hidden by Parental Controls); the cursor skips the blank row. Manage Software → FireRed offers only archive and delete software.
- Path to HOME → settings: HOME, DOWN, RIGHT×7 (gear), A; then DOWN×6 → Data Management. D-pad taps at 80 ms hold / 400 ms gap get dropped in settings: use 200 ms / 1000+ ms.
- Fallback: delete from the game's own title screen. FireRed (GBA NSO): Up + Select(−) + B at the title → "Delete all saved data?" YES. BDSP: Up + X + B at the title. Lab: `press UP+MINUS+B 500 2000` (press accepts BTN+BTN).
- Plan: keep the FireRed Brock save until the AutoStory build is ready to start 00.01; delete just before that run.
- **Delete Save Data path (verified 2026-11-01):** HOME → gear (DOWN, RIGHT×7 from the game tile) → A → DOWN×6 (Data Management) → A → DOWN×8 (one blank row below Manage Software and one below Manage Screenshots and Videos are skipped) → "Delete Save Data" is the LAST item. Taps 200 ms hold, 1000 ms gap.
- **2026-11-01 ~11:20: FireRed and Brilliant Diamond save data DELETED on console B (user request).** Both now start from a new game. Caution dialog: Delete button disabled ~3 s, then UP, A; A again to close 'deleted'.

## AutoStory program runs (frlg/autostory-mvp)
| run | build | result | notes |
|---|---|---|---|
| r1 (01:48) | frlg-m7 = ff8ef1370 + lab/main + 5c2621859 | ❌ 00.01 intro: "still in the intro after 10 minutes" | 10 s in, logged "a keyboard, confirming an empty name" with no keyboard on screen; B×8 sends Oak's speech back to the controls pages; looped. By hand: A only goes pages → Oak "Hello, there!▼". Lab `detect white` is not the dialog detector (says no on Oak's box). Sent to agent: no B in intro without a positive keyboard check; A when the arrow is unseen; lab detect commands. Setup issues fixed: AutoStory panel had no ConsoleSetup (console_profile.py crashed on a null panel → patched); app ignored quit → two instances, killed |
| r2 (02:05) | 'frlg-m8' — **actually the m7 binary**: the m8 build FAILED (lab command file used the pre-#118 interrupt type) and I deployed without checking the binary date | ❌ same intro loop | not a test of 42a651324. Lesson (already a rule): check binary mtime + a new string (grep -a) before every run |
| r3 (02:19) | frlg-m9 (4a3d9f8e7) | ✅ 00.01 (intro, options, save) ❌ 00.02 x2: 'north 4 (to wall)' into the bedroom notice (11,1) | deployed maps had NO sign tiles: cmake re-copies Packages/Resources/PokemonFRLG/Maps (old, sign-less) over build-reorg on reconfigure. Fixed by regenerating into Packages/ (symlink to ~/Projects/Arduino-Source/Packages) with frlg_maps.py: 71 'S' tiles. Checklist: check a sign tile in the deployed map before runs |
| r4-r6 (02:24-03:06) | m9-m11 | ✅ 00.02, 00.03 ❌ 00.04: unknown facing after rival pick (fixed: NORTH, measured) → battle 'over after 0 turns' in slide-in + false learn prompt (fixed: neither before turn 1) → stuck 10 min on Oak's white text box in the battle ('TOSH.▼': battle-dialog yes, arrow no, loop saw NONE and only waited) (fixed: B every 1.5 s on NONE) | rival name was TOSH this time (default list), not GREEN |
| BDSP v3 r1 | bdsp-r252 = bdsp/mvp-v3 9eca24be8 + lab/main (7f9abc18b), 16 JSON maps | full new game 00.00→01.11, Piplup, Kosmos, stop at first failure | started 03:25 |
| BDSP v3 r1 | r252 | ❌ not a test: intro 'Restarting the game' launched the FIRST HOME tile = FireRed (most recently played), pressed through FireRed's Mart text 5 min | home-tile rule: launch BD by hand first so it is tile 1 |
| BDSP v3 r2 | r252 | ✅ 00.00-00.03 (new game → Pokedex) ❌ 00.04 step 2: Sandgem Center 'not through the door after 12 tries'; player 1 tile EAST of the door. 'south 14 (to wall) stopped early after ~10 tiles' before. #111 grid suspect → v3 agent |
| r8 (04:00) | frlg-m13 (+#119 e6197d9bb) | ❌ 00.06 ×2: warp into Oak's Lab OK, but new post-warp check (BattleDialogWatcher) fired on the black below the small room → 'Failed to flee' ×6 | frlg_battle_dialog = yes on Lab mat screen. Fixed: watch BattleOpponentDetector (HP box) 5 s → m14 4c97e7188 |
| r9 (04:09) | frlg-m14 | ❌ 00.06 ×2: 'no fade … warp to Oak's Lab': late battle on 'south 20 → Pallet' logged '0 of 20' and walker never reached Pallet (player at Route1 ~(12,33)). **HARD STOP** (4 fails same step): handed re-anchor design to walker agent (to-wall/map-edge legs through grass) |
| BDSP v3 r3 | r253 = mvp-v3 cd3d0b30a (Sandgem: road then town plan) + lab/main | full new game | started 04:17 |
| BDSP v3 r3 | r253 | ❌ not a test: NEW_GAME_INTRO with the r2 save present → loaded Route 202 save, intro saw 'other' 61× | rerun from 00.04 on the save |
| BDSP v3 r4 | r253 | ❌ start mismatch: START=4 expects Rowan's lab but save is Route 202 = 00.04b. Use DIAGNOSTIC_POST_CATCH_ONLY |
| BDSP v3 r5 | r253 + POST_CATCH | ❌ 00.04b step 1: training 'no battle in 3 min'; player behind the tree row below Route 202 grass (pocket). Sent to v3 agent (#111 Route202 grid / training walk) |
| BDSP v3 r6 | r254 (bba98e972) + POST_CATCH | ❌ 00.04b: 'south 2 blocked 0 of 2 … too many plans'. Measured: the save loads the player in the pocket behind the tree row (bd6-e_s.png). Start pose / grid mismatch → v3 agent. Lab app hung once in close_game (killed, relaunched) |
| r10 (04:46) | frlg-m15 (+#119 re-anchor 0d220a584) | ✅ **00.06** (attempt 2; attempt 1 'stopped part-way 9 times') ❌ 00.07 unknown facing at Lab (6,4) → measured SOUTH, fixed (m16) |
| r11 (04:57) | frlg-m16 | ✅ 00.07 events 1-4 (Mart shopping, walk to nurse) ❌ heal: 'answer_yes_no(): no YES/NO' ×2 although heal happened ('We've restored your POKéMON'). Fixed: timed A ×4 (m17) |
| r12 (05:09) | frlg-m17 | ✅ 00.07 events 1-8 ×2 (Mart, heal, old man trigger + tutorial) ❌ forest_entrance ×2: after tutorial old man stands east of player (not in map) → 'east 2' no-op → north up col 8 → stuck under Route 2 ledge (8,71). Fixed: past_old_man step north to (20,5) (m18) |
| r13 (05:19) | frlg-m18 | ✅ 00.07 events 1-13 (Mart, heal, old man, past_old_man, forest entrance, forest W/E/N) ❌ pewter_nurse: attempt 1 trainer battle >10 min; attempt 2 used Antidote (row 2) twice when hurt → starter fainted, blackout. → program agent (PSN read, stall, potions) |
| BDSP v3 r7 | r255 (cb14b8fcb) + POST_CATCH | ✅ 00.04b step 1 training (Route 202 fix works) ❌ Sandgem Center door ×12: plan landed next to the MART's sign (~6-7 cols east); door finder turned to the Mart door. HARD STOP on this step → v3 agent (Sandgem grid rows 4-12) |
| r14 (06:05) | frlg-m19 (95d92d635) | ✅ 00.07 events 1-12 ❌ A: forest_north, after a won battle advance_text 60 s (likely level-up/learn prompt) ❌ B: attempt 2 Route 1: battle 'at end of leg' → re-anchor without fighting/fleeing → Start menu fails. → program agent |
| r15 (06:21) | frlg-m20 (08db49386) | ✅ 00.07 events 1-13, forest_exit fought 5 wild battles ❌ false 'late battle' on the forest north gate mat: BattleDialogDetector fires on black under small rooms → 10 min A/B. → program agent (require opponent HP box) |
| BDSP v3 r8 | r256 (d3749932a) + POST_CATCH | ✅ training, road, town plan to (20,11) ❌ 'taps west moved 2 tiles' (expected 3); player at the door's right edge → v3 agent |
| r16 (06:59) | frlg-m21 (35cb58756) | ✅ 00.07 events 1-13 + 4 forest battles (gate false-battle fixed) ❌ forest_exit: 'west 6' ended 2 short (col 9) with no interrupt; late battle → re-anchor 'south 23 to wall' assumed col 5 → held into TRAINER TIPS sign (9,29) ×6. → walker agent |
| BDSP v3 r9 | r257 (4a74ffcf9) + POST_CATCH | ✅ training, Sandgem road + town plan, taps, INTO Center + HEALED ❌ 'out the door: did not get back out': outside but evening tint (5:20 pm) → 'still indoors' → walked south into log house. → v3 agent (time-of-day robustness) |
| r17 (07:58) | frlg-m22 (35cb58756 + #119 59538a4f5) | ✅ 00.07 events 1-13 ❌ forest_exit: att.1 blackout (0 Potions left, fought every wild); att.2 lost after re-plan → warp 'no fade'. → program agent (run when low, potion between battles, to-wall legs) |
| BDSP v3 r10 | r258 (59d88d93f) + POST_CATCH | ✅ training ❌ into Sandgem: from grass patch 'south 2 to wall' overshot, then east/south legs lost → near house+pond, 'too many plans'. → v3 agent |
| r18 (08:35) | frlg-m23 (50b0dad92 + #119 59538a4f5) | ✅✅ **00.07 PASSED** (forest exit, gate, Pewter nurse, heal, saved) ❌ 00.08 ×2: Liam spotted us on the walk back (south), battle won, re-anchor 'no walls near it' in the gym → program agent |
| r19 (09:06) | frlg-m24 (6138b2c0f) | ✅✅ **00.08 PASSED**: Liam (5 turns), nurse, Brock (4 turns), saved. All 8 checkpoints have now passed on console B (separately, from saves) |
| BDSP v3 r11 | r259 (50edfb0e9) + POST_CATCH | ✅ training, Sandgem, Center door, HEAL ❌ 'out the door: arrived after 800ms' while still inside (screen change from turning/walking) → walked east inside the Center → Mart 'counter not reached'. → BDSP agent |
- lab app hung again in close_game (wait_for_all after HOME) right after a full_run_watch FAIL; second time tonight. Workaround: kill + relaunch; use press HOME/X/A instead of close_game after a failed AutoStory run.
| BDSP v3 r12 | r261 (82fec359b, Mart lady) + POST_CATCH | ✅ training, Sandgem, Center, HEAL, door exit ❌ Mart lady event 3 read_lab_sign: exit walk overshot south ~3 rows (2 s walk), west walk on wrong row into trees. → BDSP agent |
| FULL r1 (20:00 console clock) | frlg-m25 (#120 88d8ffdb6 + lab) | new game → Brock, 3 attempts/checkpoint | started |
| FULL r1b | frlg-m26 (frlglab 1e3b8a432: Mart fix + shots) | resume 00.07→00.08 from save; r1 stopped in 00.07 attempt 2 (old Mart bug) after forest_gate warp-fade fail | started |
| r1c | frlg-m27 (979d5dc42: forest save split) | 00.07→00.08 from lab save, 1 attempt; makes forest save | started |
| r1d | frlg-m28 (4fa1832d5: east-side forest + leg shots) | 00.08 from forest save, 1 attempt | started |
| r1e | frlg-m28 | 00.08 PASSED 11:54 (forest→Pewter Center); now 00.09 Brock from Pewter save, 1 attempt | started |
| r1f | frlg-m28 | 00.08→00.09 from forest save (r1e stopped: no Pewter save exists after an END=8 run), 1 attempt | started |
| r1e2 | frlg-m28 | 00.09 Brock from Pewter save (saved 11:53:49 by heal_pewter); r1f wrongly walked 00.08 legs inside Pewter Center, stopped | started |
| r1e2 | frlg-m28 | 00.09 PASSED 12:05: Liam 4 turns, Brock 4 turns, HP green throughout, 0 Potions used | done |
| pause | | FRLG paused by user after 00.09 pass (12:05); lab app closed, lock released | |
| drift | m28 lab | Route2 corner 13 legs/5 battles, forest x=3 flee 17 legs/20 battles, fight 7 legs: no drift. 2 holds cut by focus-out cancel (lab-only). Not reproduced; next = validator in walker | paused |
| full2 | frlg-m29 (074814f7e = #120 769d7a427 + lab) | new game → Brock (00.01–00.09), 3 attempts/checkpoint | started 10:57 |
| full2 result | frlg-m29 | 00.01–00.05 ✅ first try; 00.06 ✅ attempt 3 (att.1 lab-door fade missed after late-seen Route 1 battle and wandering re-anchors; att.2 ledge wall check read a 20.1 tap change at (12,21)); 00.07 ✅ first try (forest save); 00.08 ❌ att.1 blackout: bag read 'potion x-1; antidote x-1' (screen showed ×5/×3), battle treated unknown as 0, never healed vs Bug Catcher poison; stopped by user during att.2 (11:30). Resume from 00.08 after the bag fix. | stopped |
| full2b | frlg-m30 (9b2dc5d0f = lab + #120 510aa66f6 bag fix) | resume 00.08→00.09 from forest save, 3 attempts | started 12:07 |
| full2c | frlg-m30 | 00.09 from Pewter save, CHECKS=fast (thorough wall-tap check failed 3/3 falsely: NPC in top band, 24.6–26.7 > guessed 20), 3 attempts | started 12:25 |
| full2c | frlg-m30 | 00.09 from Pewter save, CHECKS=fast (thorough wall-tap check failed 3/3 falsely: NPC in top band, 24.6–26.7 > guessed 20), 3 attempts | started 12:26 |
| full2b | frlg-m30 | 00.08 PASSED 12:20 attempt 1 (bag fix: potion used at yellow+PSN; counts 5/3/1 read). 00.09 failed 3/3 at below_liam_line: thorough wall-tap check false positive (NPC in top band, 24.6–26.7 > 20) | 12:22 |
| full2c | frlg-m30 | 00.09 CHECKS=fast: Liam + Brock beaten with Bubble (Water Gun declined at L13: 4 moves, never-delete). Post-battle Brock speech read as learn prompt → B/A loop; stopped 12:32 | |
| hand | CFR | finished Brock's speech (B×16), saved in gym (6,6) N: 1 badge, 0:38. New game → Brock DONE. Next: 00.10 running shoes (agent), move-set task #9 | 12:40 |
| r3a | frlg-m39 | 00.11→00.12 (Route 3 trainers, Mt. Moon Center) from (17,26) S; first run | started 19:46 |
| r3b | frlg-m40 | 00.11 passed all 3 trainers; end save OK but yes_no_cursor false-matched rock → fix 20dbd3e6c; now 00.12 | started 19:58 |
| r3c | frlg-m41 | 00.12 retry: advance_text arrow false-match + last_no overflow fixed (4f68ce021) | started 20:01 |
| r3d | frlg-m41 | 00.12 retry 2: Lass at (40,10-12) wanders; spotted on run east row 9 → msg after 500ms watch → threw | started 20:04 |

## 2026-11-02 — Shiny Magikarp to Box 1 (by hand, CFR)
| step | input | result |
|---|---|---|
| pc0–pc4 | resume from HOME (A); walk salesman_front (2,3) → PC front (11,2), face N | Route4_PokemonCenter_1F PC is at (11,1); boy wanders (4–6,3–5) and blocks row 4 |
| pc5–pc9 | A ×4 (boot, SOMEONE'S PC, storage opened); DOWN, A (DEPOSIT); RIGHT, DOWN (slot 3), A, A (STORE), A (BOX1 0/30) | party was Squirtle **Lv16 (not evolved)**, Rattata, shiny Magikarp ♀ Lv5 → Magikarp stored in BOX1 |
| pc10, sv3–4 | B ×4 to overworld; PLUS (cursor on SAVE), A, A (overwrite YES), A | **Saved: Route 4 Center (11,2) facing N at the PC**, party Squirtle Lv16 + Rattata, 1 badge, dex 7, 1:17 |
Captures for PC/box detectors: ~/FRLGLab/captures/pc-deposit-20261102/.

Note: the party Rattata is the user's (caught for a trade evolution with another save), not the Cut user.
| pr1–pr7 | A×4, DOWN, A came too early (menu not up) → WITHDRAW opened; B → "Continue BOX operations?" NO (A); DOWN, A (DEPOSIT), RIGHT (slot 2), A, A (STORE), A (BOX1); B×4; PLUS, A, A, A | user's Rattata ♂ Lv3 stored in BOX1. **Saved 1:22: Route 4 Center (11,2) N, party = Squirtle Lv16 only.** Blind inputs before the menu appears go to the text box: wait for each menu |
| sm0–sm4 | (console was at HOME between CFR batches: A to continue first) PLUS, UP×3 (POKéMON), A, A (SUMMARY), RIGHT×2, A; B×4 | Squirtle ♀ Lv16 moves: **WATER GUN 25/25, TAIL WHIP 30/30, BUBBLE 30/30, WITHDRAW 40/40** (Tackle was forgotten for Water Gun). Captures: ~/FRLGLab/captures/summary-20261102/ |
