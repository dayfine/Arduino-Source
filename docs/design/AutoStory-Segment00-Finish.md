# AutoStory segment 00: what is left (scheduled after segment 01's first pass)

**Superseded 2026-10-29:** segment 00 now runs on the MVP branch `bdsp/floaroma-mvp`
(fork #68, on #1717 and the shared core); 00.00 to 00.03 passed unattended from a new game
(r108b) and 00.04 got past Mom (r109). Current results are in
[AutoStory-Lessons.md](AutoStory-Lessons.md) §7 and §13. Kept for the history below.

Status 2026-10-28. Code: fork branch `bdsp/new-game-intro-v2`, worktree
`~/Projects/Arduino-Source-bdsp`, latest commit 6dbcc8e10 (00.05 on a Jubilife grid).
Testing: the `bdsp-autostory-test` skill.

## Where it stands

| Checkpoint | State |
|---|---|
| 00.00 to 00.03 (new game, intro, starter, Pokédex) | run unattended earlier (new-game intro on the screen machine) |
| 00.04 (Potions, training, Route 202, Dawn) | ran unattended from the Route 202 Potion save (r79) |
| 00.05 (Jubilife: Barry, president, clowns, Pokétch, Center PC) | done by hand on the console, then rewritten on a tile grid (6dbcc8e10); **not yet run** |

The only save on console B is the end of segment 00 (at the Jubilife Center PC). There is no
00.05-start save any more.

## Why it is deferred

Testing 00.05 for real needs a new game through 00.00 to 00.04, which overwrites the
end-of-00 save that segment 01 starts from. Segment 01 is stepped through first from that save;
segment 00 is finished afterwards.

## Plan

1. **Dry run in town (no new game).** From the current save the NPCs are still there and still
   talk. Walk the 00.05 legs with the Lab Console or a diagnostic entry: school door, president
   (44,26), the three clowns (Mart clown at (54,23) is unverified), the Center door (51,30).
   Fix the grid where the console disagrees. Story-specific dialogue lengths differ after the
   story, so only walking and talk-starting are checked here.
2. **Shorten the zig-zag plans.** The school-to-president plan is about 30 legs because the
   plazas have few walls to end legs on. Either accept open legs with localisation after them,
   or add known stopping features (planters, curbs) to the grid as walls.
3. **New game, full segment 00, twice** (needs the user's OK at that time: it overwrites the
   save). Each run must end with "Completed: 00.05" and a save at the PC; keep both runs' logs
   and the save screenshots.
4. Push the branch and update the fork PR's description (2 to 5 sentences).
