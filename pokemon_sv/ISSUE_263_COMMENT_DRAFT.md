Draft comment for PokemonAutomation/ComputerControl#263 (not posted; the user posts it).

---

Update: the first two stages of the plan were checked on a Switch today, each with a build of
upstream `main` plus that stage's PRs.

- **Stage 0 (measure it)**: the BBQ Farmer stats now show BP earned, spent, net and BP/hour, and
  at the end of a run the program checks the balance change against the stats. 8 quests in 24
  minutes, and the check matched exactly. An error report with no screenshot no longer crashes
  the app.
- **Stage 1 (never stuck)**: two runs, 15 quests in 57 minutes, never more than 11 minutes
  without a completed quest. Every reroll hit the intended quest and was confirmed replaced;
  quests that failed their retry and quests the farmer can't do were rerolled; saves happened
  at each milestone. The BP check matched in every run.

These were 30-minute smoke tests, so they show the code works on the real game rather than a
BP/hour gain; that needs the 2-hour runs in the plan. Two setup problems turned up and will be
handled at program start: the hatch-egg quest with no egg left in the boxes leaves the game in
the box screen, and a Switch that has dimmed its screen after a few idle minutes isn't
recognised as the overworld.

Details: https://github.com/dayfine/Arduino-Source/blob/agents/pokemon_sv/STAGE_RUNS.md
