# Lab Channel

Lets a script drive a running SerialPrograms app through files, with no
window focus and no click: select a program, set its options, start and stop
it, read its state, and press buttons or check detectors on the console
between runs. Meant for a lab that drives one app per console from the shell.

The channel is one folder per app instance, independent of the selected
program. Requests are text files; replies are text files. Nothing else is
needed on the script side.

## Turning it on

Create the folder `Lab/` inside the profile's user-files folder (the folder
that holds `SerialPrograms.log`, `Screenshots/` and the settings; with
`--profile NAME` that is `Profiles/NAME/`) **before launching the app**. An
app that finds it at startup polls it every 500 ms from a worker and creates
`inbox/`, `outbox/` and `done/` inside it; an app that does not find it
starts nothing, so normal users see no change. Creating the folder later
takes effect at the next launch.

## Files

| Path | Direction | Meaning |
|---|---|---|
| `Lab/inbox/<name>.txt` | script writes | A request: one command per line, run in order. Requests run one at a time, in name order. |
| `Lab/done/<name>.txt` | app moves | The request, moved here when it starts. |
| `Lab/outbox/<name>.out` | app writes | The request's journal (below). It appears only once the request has finished; `<name>.tmp` shows the progress so far. |
| `Lab/outbox/<name>_<label>.png`, `.mp4` | app writes | Files that `shot`, `shots` and `history` save. |
| `Lab/state` | app writes | The channel's state, rewritten after every request and whenever the program state changes. |
| `Lab/runs/<run_id>/` | app writes | One folder per run of the program: `result.json`, `events.jsonl`, `log.txt`, `frame-*.png`. See [Run records](#run-records). |

Write a request somewhere else and then move it into `inbox`, so the app
never reads a half-written file. Only `.txt` files are read.

### Requests

- One command per line, words separated by spaces. `#` starts a comment.
- The first line may be `owner <token>`; see [Owner](#owner).
- Commands run in order. The request stops at its first failing line.
- Durations are in milliseconds. Arguments in `[brackets]` are optional.

### The journal

| Line | Meaning |
|---|---|
| `ok <line> <command> @<start>-<end>ms` | The command on that line of the request succeeded. Times are from the start of the request. |
| `error <line> (<command>): <reason>` | The first failure. The request stops here. A console command also saves a screenshot of the moment (a `shot` line before the error). |
| `end` | Always the last line. |

Any other line comes from a command, such as `detect home yes`, `set
START_SEGMENT = "0"` or the path of a saved screenshot.

### The state file

```
seq=r3
state=RUNNING
program=PokemonBDSP:AutoStory
run=20261101-153012123456-003
owner=lab-B
error=
```

- `seq` is the name of the last request handled.
- `state` is `STOPPED`, `RUNNING` or `STOPPING` for a program, `NOT_READY`
  while the panel loads, `NO_PROGRAM` when the selected panel is not a
  program (the settings page).
- `program` is the selected program's identifier (`<game>:<program>`).
- `run` is the ID of the selected program's last run, running or finished,
  whether a request or the window's Start button started it; empty before
  the first run. Its folder is `Lab/runs/<run>/`.
- `owner` is the owner token, empty when the channel is unclaimed.
- `error` is the error of the last request, empty once a request succeeds.

## Commands

### Channel

| Command | What it does |
|---|---|
| `select GAME:PROGRAM` | Shows that program, as clicking it in the program list does. The identifier is the one in `state` and in the settings file's `99-Panels` section, e.g. `PokemonBDSP:AutoStory`, `NintendoSwitch:CommandFileRunner`. Refused while a program runs. |
| `set KEY JSON` | Sets one option of the selected program by its key in the settings file, live: `set START_SEGMENT "0"`, `set NEW_GAME_INTRO true`, `set NOTIFICATIONS.Enabled false` (dots walk into groups). The value is JSON, so strings need quotes. Prints `set KEY = <value the program now holds>`, which differs from what was sent when the program clamped or refused it. Refused while the program runs. |
| `start` | What clicking Start Program does. Refused unless the program is `STOPPED`. Prints `run=<run_id>`, the ID of the run it started. |
| `stop` | What clicking Stop Program does. Refused unless the program is `RUNNING`. The request goes on; the program stops in the background. |
| `state` | Prints the state file's lines into the journal. (In the Command File Runner program, `state [image]` prints the detectors that fire instead; from the channel use `detect` per detector.) |
| `keepawake on\|off [seconds]` | While it is on and no program runs, presses B every `seconds` (default 15) so the console does not sleep. A running program pauses it. Off by default; off again when the app restarts. |
| `owner claim TOKEN`, `owner release` | The owner lock, below. |
| `help` | Prints every command and detector, including the ones games add. |

### Console

These run on the selected program's console. A press runs only while no
program runs (`STOPPED`); the commands that only read the screen (`shot`,
`shots`, `history`, `detect`, `wait_for`, `expect`) run any time.

#### Controller

| Command | What it does | Defaults |
|---|---|---|
| `press BTN[+BTN] [hold] [release]` | Presses one button, or several together, joined with `+` (e.g. `L+R`). Buttons: `A B X Y L R ZL ZR PLUS MINUS LCLICK RCLICK HOME CAPTURE`. | hold 160, release 160 |
| `dpad UP\|DOWN\|LEFT\|RIGHT [hold] [release]` | Presses the D-pad. | hold 100, release 200 |
| `taps DIR N [all] [hold] [release] [still]` | Taps the D-pad `N` times and measures the screen change (RMSD at 192x108, as `walk`) between the frame before and after each tap. Prints `tap k: change X moved\|still` per tap, `moved` when the change is at least `still`, then `moved M of N`. Stops after two still taps in a row (a wall) unless `N` is followed by `all`. On BDSP a step measures 21 to 50, a bump into a wall or an NPC brushing past 4 to 13. | hold 160, release 640, still 16 |
| `stick L\|R x y [hold] [release]` | Moves the left or right stick. `x` and `y` run from -1 to 1; `y` is +1 for up. | hold 160, release 160 |
| `wait ms` | Waits. | |
| `walk x y [max] [still]` | Holds the left stick in 200 ms slices until two slices in a row change the screen less than `still`: a wall or someone in the way. Prints `walk still\|limit after N ms, moving M ms` and each slice's change. For measuring maps: a timed `stick` hold moves a different number of tiles from try to try. | max 5000, still 10 |

#### Screen

| Command | What it does | Defaults |
|---|---|---|
| `shot label` | Saves the current frame as `<request>_<label>.png` in the outbox. | |
| `shots label count [interval]` | Saves `count` frames, `<label>_0` to `<label>_<count-1>`, one every `interval`. | interval 500 |
| `history label` | Saves the console's recent video as `<request>_<label>.mp4` in the outbox, when Stream History is on. Run it right after a program to keep a recording of that run. | |
| `detect detector [image]` | Runs a detector once and prints `yes` or `no`. With `image`, it runs on a saved picture instead of the live video: an absolute path, or a file name in the outbox. | |
| `wait_for detector [timeout]` | Checks the video every 100 ms until the detector fires. Prints how long it took, or `timeout`. | timeout 5000 |
| `expect detector [timeout]` | The same as `wait_for`, but a timeout fails the line. | timeout 5000 |

#### Switch

| Command | What it does |
|---|---|
| `close_game` | Goes to the Home menu from anywhere and closes the game. |

#### Detectors

These work on any game:

| Name | Detects |
|---|---|
| `home` | The Switch Home menu. |
| `user_select` | The "Who is using the software?" screen. |
| `black` | A black screen. |
| `white` | A white screen. |

Games add their own commands and detectors with
`register_command_file_command()` and `register_command_file_detector()` (see
`NintendoSwitch_CommandFileRunner.h`); `help` lists them.

## Owner

Two scripts driving one console would interleave their presses. A script
claims the channel with `owner claim TOKEN`; from then on every request that
changes anything (`select`, `set`, `start`, `stop`, `keepawake`, presses)
must start with the line `owner TOKEN`, or it is refused with
`owned by TOKEN`. Requests that only read (`state`, `help`, `shot`, `detect`,
...) need no token. `owner release`, with the owner line, frees the channel.
The lock lives in memory: it dies with the app. The holder may claim again
with the same token (e.g. after a script restart).

## Run records

Every run of the program gets a folder `Lab/runs/<run_id>/`, whether a
request or the window started it. Runs are recorded only in an app that
found the `Lab/` folder at startup. The run ID is opaque; IDs sort in the
order the runs started, and a restarted app never reuses one. The newest 32
folders are kept; older ones go when a run starts.

| File | Content |
|---|---|
| `meta.json` | What ran: `run_id`, `program`, `version`, `executable` (the app's path), `commit` (the git commit the build was made from, `-dirty` with local changes) and `settings` (the program's settings as the settings file stores them). Written when the run starts. |
| `result.json` | What the run did, without its events and log (below). Rewritten as the run goes; final once `status` is `terminal`. Poll it, or let `labctl.sh wait RUN_ID` do it. |
| `events.jsonl` | One JSON object per line: the events the program reports (checkpoint, step, attempt, pose, ...), the errors it reported (`"kind":"error"`), and the console probes taken during the run or within two minutes after it ended (`"kind":"probe"`). Appended as they happen. |
| `log.txt` | The run's own log: the program's and its console's lines, nothing from before or after. Appended as the run goes, up to 8 MiB; a last line says how many lines were left out past that. |
| `frame-*.png` | `frame-failure.png` (or `frame-end.png` for a finished run) is the frame the exception carried, when it carried one. `frame-<n>[-<checkpoint>][-s<step>][-a<attempt>].png` is the frame of event `n`. |
| `probe-*.png` | Copies of the `shot`s taken during or right after the run. |

`result.json` of a failed run:

```json
{
    "dropped_events": 0,
    "duration_ms": 650333,
    "end_time": "2026-11-01 15:41:02.456",
    "error": "expect_failed: the Route 204 sign's box did not open",
    "event_count": 42,
    "frame": "frame-failure.png",
    "last_event": {
        "attempt": 2,
        "checkpoint": "01.08",
        "frame": "frame-041-01.08-s4_7-a2.png",
        "kind": "attempt",
        "pose": "route_204_south (12, 41) facing N",
        "step": "4/7",
        "text": "the sign's box did not open",
        "time": "2026-11-01 15:41:02.400"
    },
    "dropped_log_lines": 0,
    "log_lines": 2210,
    "outcome": "failed",
    "program": "PokemonBDSP:AutoStory",
    "run_id": "20261101-153012123456-003",
    "start_time": "2026-11-01 15:30:12.123",
    "status": "terminal"
}
```

- `status` is `running`, `stopping` (a stop was asked for) or `terminal`.
- `outcome`, once terminal, is exactly one of `completed` (the program
  returned, or declared itself finished), `cancelled` (it ended through the
  stop path) or `failed` (a setup, download, connection or execution error).
  It comes from the program thread's own exit, never from the state file or
  the log: a stop asked for after a failure was seen does not turn the
  failure into a cancellation, and a program that returns while a stop is
  pending completed. `completed` means the program said so; it does not
  verify the story moved.
- `error` is the primary error of a failed run, the exception's message.
  Errors the program reported on the way (a stats-save failure, a download
  warning) are `error` events, not the outcome.
- `frame` names the frame the exception carried. Absent when it carried none.
- `last_event` is the newest event the program reported, with the same
  fields as a line of `events.jsonl`. For a program that retries a checkpoint
  and reports each attempt, it is the failing attempt.
- `log_lines` counts the run's log lines so far; `dropped_log_lines` the
  lines past the 8 MiB cap, which are only in `SerialPrograms.log`.
- `end_time`, `duration_ms`, `outcome`, `error` and `frame` appear once the
  run is terminal; `last_event` once there is an event.

Lines of `events.jsonl`:

```json
{"checkpoint":"01.07","kind":"checkpoint","text":"ok","time":"2026-11-01 15:38:10.001"}
{"attempt":2,"checkpoint":"01.08","frame":"frame-041-01.08-s4_7-a2.png","kind":"attempt","pose":"route_204_south (12, 41) facing N","step":"4/7","text":"the sign's box did not open","time":"2026-11-01 15:41:02.400"}
{"frame":"probe-r16_after.png","kind":"probe","output":"shot .../Lab/outbox/r16_after.png\n","text":"shot after","time":"2026-11-01 15:42:30.120"}
```

`time` and `kind` are always there; a program fills `checkpoint`, `step`,
`attempt`, `text`, `pose` and `frame` as it has them (a run holds the frames
of its newest eight events, so a frame is written within a poll of its
event). A `probe` line's `text` is the console command as sent and `output`
what it printed; its `frame` is the copied shot, when the command took one.

Programs report events through `record_run_event(env, event)`
(`CommonFramework/RunRecord.h`). The run model, the one-result rule and the
frame capture live in `ProgramSession`; this channel only writes the files.

## Running a program from the shell

Nothing is clicked. The app is launched with its own profile, so its
settings, log, screenshots and `Lab/` folder live under `Profiles/lab-B/`.

```sh
P="$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B"
mkdir -p "$P/Lab"
open -n -a SerialPrograms-B.app --args --profile lab-B   # macOS; see devtools/lab/README.md
until [ -f "$P/Lab/state" ]; do sleep 1; done

# A request: write it elsewhere, then move it into the inbox.
request() {   # request NAME <<EOF ... EOF
    cat > "$P/Lab/outbox/$1.part" && mv "$P/Lab/outbox/$1.part" "$P/Lab/inbox/$1.txt"
    until [ -f "$P/Lab/outbox/$1.out" ]; do sleep 0.5; done
    cat "$P/Lab/outbox/$1.out"
}

request r1 <<'EOF'
owner claim lab-B
EOF

request r2 <<'EOF'
owner lab-B
select PokemonBDSP:AutoStory
set START_CHECKPOINT_TUTORIAL "15"
set END_CHECKPOINT_TUTORIAL "15"
start
EOF

# Wait for the run to end and read what it did.
run=$(sed -n 's/^run=//p' "$P/Lab/state")
until grep -q '"status": "terminal"' "$P/Lab/runs/$run/result.json"; do sleep 2; done
cat "$P/Lab/runs/$run/result.json"
```

`r2.out`, the journal of the second request:

```
ok 2 select PokemonBDSP:AutoStory @0-412ms
set START_CHECKPOINT_TUTORIAL = "15"
ok 3 set START_CHECKPOINT_TUTORIAL "15" @412-418ms
set END_CHECKPOINT_TUTORIAL = "15"
ok 4 set END_CHECKPOINT_TUTORIAL "15" @418-423ms
run=20261101-153012123456-003
ok 5 start @423-431ms
end
```

The state file while it runs, and after:

```
seq=r2                      seq=r2
state=RUNNING               state=STOPPED
program=PokemonBDSP:AutoStory
run=20261101-153012123456-003
owner=lab-B
error=
```

To end a run early, and to hand the console back:

```sh
request r3 <<'EOF'
owner lab-B
stop
EOF
until grep -q '^state=STOPPED' "$P/Lab/state"; do sleep 1; done

request r4 <<'EOF'
owner lab-B
owner release
EOF
```

A request from another script without the token is refused and the journal
says who holds it: `error 2 (start): owned by lab-B`.

## Probing the console between runs

```
# probe.txt: open Home and check that the Home detector sees it.
owner lab-B
press HOME
expect home 3000
shot home_menu
```

```
ok 2 press HOME @0-322ms
expect home after 2ms
ok 3 expect home 3000 @322-325ms
shot .../Profiles/lab-B/Lab/outbox/probe_home_menu.png
ok 4 shot home_menu @325-371ms
end
```

Tips:

- Save a screenshot right before an `expect` you're unsure of. If the line
  fails, the error screenshot shows what came after.
- To tune a detector, save frames with `shots`, then run `detect` on the
  saved images. The console doesn't need to be on that screen again.
- Read `state` to learn what the app is doing; never stop a program to find
  out which one is selected.

## The Command File Runner program

The same console commands are also a program, Command File Runner (Nintendo
Switch → Testing). While it runs it reads command files from the `inbox` of
its own folder (`CommandFiles/` in the settings folder by default, or the
**Command Folder** option) and writes the same journals to its `outbox`. Use
it to run a batch while the Lab folder is off, or to keep a batch going as a
program with its own Stop button. Its `stop` line stops the file and the
program; **Stop When Inbox Is Empty** stops it when the batch is done. Don't
point its folder at `Lab/`: the channel reads that one.

## Changes from the lab control folder

The `LabControl/` folder of earlier lab builds (files `start` and `stop`, a
two-line `state`) is gone. `start` and `stop` are lines in a request, and
`state` has the five lines above. Scripts move from
`printf '' > LabControl/start` to a request file containing `start`.

## Tests

`add_tests_LabChannel()` covers the request format, the journal lines, the
state text, the owner lock, the keep-awake schedule and the run files
(`result.json`, `events.jsonl`, frames, probes, pruning) (`LabChannel - ...`
unit tests); `add_tests_RunRecord()` covers the run model and the run
lifecycle through a scripted fake program; `add_tests_CommandFileRunner()`
covers the console commands' arguments. Run them with the app's
`--command-line-test-mode`. `devtools/lab/labctl_wait_test.sh` checks
`labctl.sh wait` against hand-written result files. Selecting, setting,
starting and the presses need a live app and console.
