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
| `disk_gc.sh [--dry-run\|--apply]` | The lab disk GC (source of `~/BDSPLab/disk_gc.sh`, launchd `com.dayfine.lab-disk-gc`): old lab profile output, idle agent worktrees and build folders, and `~/Projects/dev-deployments/<slot>/` beyond the newest 5 (never a running app's, never a symlink). Dry run by default. `disk_gc_test.sh` tests the deployment rule on a fake `HOME`. See [`../LAB_BUILD_DISK.md`](../LAB_BUILD_DISK.md). |

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

## Claude triage and watchdog

`triage_failure.py` and `watchdog_shot.py` call Claude through the official `anthropic` SDK
(`pip install anthropic`; Pillow optional, else `sips` downscales frames to 640 px). Credentials:
`ant auth login` or `ANTHROPIC_API_KEY`. Shared setup is in `lab_claude.py`. `--dry-run` prints the
request (images shortened) without network or SDK.

**Budget.** The lab spends Console API credit: $120/month, paced at $4/day. `lab_claude.send()`
prices every call from its token usage into `~/BDSPLab/claude-spend.jsonl` and refuses new calls
(`BudgetExceeded`) once today's or this month's total reaches the cap. Override with
`LAB_CLAUDE_DAILY_USD` / `LAB_CLAUDE_MONTHLY_USD`. This is a local guard; the hard cap is the
workspace spend limit in the Console.

- `python3 lab/scripts/triage_failure.py --frame fail.png --checkpoint 00.04 --step step.json --notes ~/BDSPLab/notes/<date>-checklist.md [--map-slice slice.txt]`
  sends the frames, the last 200 lines of the lab-B log, the step and map slice to `claude-opus-5-5`
  (adaptive thinking, effort high, server-side fallback) and appends a fault-class verdict with cited
  evidence to the notes file.
- `python3 lab/scripts/watchdog_shot.py shot.png` classifies one frame with `claude-haiku-5-5`
  (effort low), prints the JSON verdict and exits 2 on a dead feed, color bars or HOME, 1 on error.

Tests (mocked client): `python3 -m unittest discover -s lab/scripts -p test_lab_claude.py -v`.
