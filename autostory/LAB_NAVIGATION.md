# AutoStory navigation in the lab

How we run the map / route / walk process (see `devtools/maps/README.md` on
the code branches) on our lab Switch. Code-agnostic facts live there; this
file is our setup and what bit us.

## Measuring a map (console B)

- CFR build with `walk`: `cfrwalk1` in `~/BDSPLab/builds` (branch
  `lab/lab-console`, fork #58). Start it with
  `sh ~/BDSPLab/stage_run.sh cfrwalk1 lab-B <label> B cfr` after dropping a
  file in the lab-B CommandFiles inbox; later batches with `labctl.sh lab-B start`.
- Start from a proven anchor, not wherever the last run left you: reload the
  save and run the checkpoint that ends on one (BDSP: the Route 202 save,
  then the Sandgem Center checkpoint, which ends under the Center's sign).
- One `walk` per leg, a `shot` after it, and a sign read (`press A`) where a
  sign should be. Record the stop and the ms in the data file's header.

## Traps (2026-10-30)

- A timed hold is not a measurement: 300 ms up/down moved one row or two;
  200 ms moved exactly one. Use `walk`.
- Rows read off screenshots were wrong twice (an NPC on our row looked a row
  up; a sign a row below the trees looked right under them).
- After a failed AutoStory run the game is closed to HOME; the next stick or
  A moves the HOME cursor (it once landed on Pokemon Shield). `shot` first.
- Grass: wild battles stop a measurement. Run: `dpad UP` (wraps to Run), `A`.
- NPC dialogues run longer than they look (the Mart lady: Potions, the Bag
  line, tips). Press A one at a time with a `shot` until the box is gone; an
  extra A starts the talk again.
- Deploy from the worktree you changed:
  `BDSP_WT=<worktree> sh ~/BDSPLab/bdsp_deploy.sh bdsp-rNN`, and check the
  printed commit. r136 ran an old worktree's 00.04 end to end and saved over
  the Route 202 save with Jubilife.
- A full_run_watch run doesn't load the save; reload first (`close_game`,
  then the A / wait sequence) or it runs on HOME.
