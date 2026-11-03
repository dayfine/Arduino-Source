# Lab console helper scripts

Small wrappers for driving the lab app on console B from a shell. They were job-local helpers
during the FRLG and BDSP AutoStory runs (2026-10/11); paths now come from the environment.
Console rules still apply: run `switch_guard.sh` / `lablock.sh` first, one train on the console,
and never start a program on a save you have not checked.

| Script | What it does |
|---|---|
| `env.sh` | Shared settings, sourced by the others. `LAB_CONSOLE` (default `B`), `LAB_PROFILE_NAME` (default `lab-B`), `LAB_PROFILE` (the profile folder), `LAB_CONTROL` (the folder with `labctl.sh`: [`../control`](../control/README.md) next to these scripts, else `$PA_AGENTS_DIR/lab/control`, default `~/Projects/Arduino-Source-agents`, else the old `~/Projects/Arduino-Source-frlglab/devtools/lab`), `LAB_DIR` (`lablock.sh`'s lock root, default `~/.lab`), `LABLOCK_TOKEN` or `LABLOCK_TOKEN_FILE` (the console lock). |
| `lc.sh ARGS` | `labctl.sh` against that profile: `state`, `select ID`, `start`, `stop`, ... |
| `sel.sh PROGRAM` | Select a program (e.g. `NintendoSwitch:CommandFileRunner`, `PokemonFRLG:AutoStory`) and wait until the log says the controller is ready; one retry, exit 1 if never ready. |
| `cfr.sh NAME < batch` | Queue stdin as a Command File Runner batch, select the CFR, start it, wait for `end` (cap `CFR_WAIT_S`, default 600 s; stops early if the lab app exits) and print the journal. |
| `ciwait.sh RUN_ID...` | Wait for fork CI runs (cap `CI_WAIT_MIN`, default 60), print branch / workflow / conclusion and failed test names. Run it in the background from the main session; subagents don't wait on CI. |
| `decomp.sh ARGS` | `python3 -I -m devtools.maps ARGS` from a worktree with `devtools/maps` (`MAPGRID_WT`, default the FRLG lab tree). |

Example:

```sh
export LABLOCK_TOKEN_FILE=$JOB/tmp/frlg-lock.token
printf '%s\n' 'shot before' 'press A 100 1500' 'shot after_a' | sh lab/scripts/cfr.sh probe1
```

## CFR batch syntax

One command per line; the journal prints `ok N <line> @start-end ms` or an error per line, then
`end`. Durations are milliseconds. `help` prints the full list for the build you run.

| Command | Meaning |
|---|---|
| `press A 100 1500` | Press A, hold 100 ms, then wait 1500 ms. `press A+B ...` presses both. |
| `dpad DOWN 160 400` | D-pad hold 160 ms, release 400 ms. A short tap (40-80 ms) only turns the player in BDSP. |
| `taps DIR N [all]` | N D-pad taps, each reported moved or still; stops at a wall unless `all`. |
| `stick L x y hold release` | Left (or `R`) stick at x, y in -1..1. In CFR, y = +1 is north (up). |
| `walk x y [max_ms]` | Hold the left stick until the screen holds still (a wall) or `max_ms`. A measurement, unlike a timed hold. |
| `wait ms` | Wait. |
| `shot name` | Save the frame as `outbox/<batch>_<name>.png`. End every batch with one and look at it. |
| `detect D` / `wait_for D ms` / `expect D ms` | Run a detector (`home`, `black`, `white`, `user_select`, plus the game's) once, until it fires, or fail on timeout. |
| `state` | Every detector that fires now. |
| `close_game` | From anywhere to HOME with the game closed. |
| `stop` | Stop this file and the program. |

Keep batches to 5-10 commands. CFR cannot press a D-pad direction and a button at once.

## Gotcha: HOME between batches

The console may sit at the HOME menu between CFR batches (after a failed run, a `close_game`, or
when the app reconnects the controller). Then the next stick or D-pad moves the HOME cursor and
can launch another game. Start each batch with `shot` (or `detect home`); if it shows HOME with the
game still suspended, `press A` first to return to the game.
