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
| `Lab/inbox/<name>.txt` | script writes | A request: one command per line, run in order. Requests run one at a time, in name order; a request of `snapshot` lines only runs beside them (see [Snapshot](#snapshot)). |
| `Lab/done/<name>.txt` | app moves | The request, moved here when it starts. |
| `Lab/outbox/<name>.out` | app writes | The request's journal (below). It appears only once the request has finished; `<name>.tmp` shows the progress so far. |
| `Lab/outbox/<name>_<label>.png`, `.mp4` | app writes | Files that `shot`, `shots` and `history` save. |
| `Lab/state` | app writes | The channel's state, rewritten after every request and whenever the program state changes. |
| `Lab/state.json` | app writes | The same state and more, for the lab server: rewritten at once when the state changes, else about once a second. See [state.json](#statejson). |

Write a request somewhere else and then move it into `inbox`, so the app
never reads a half-written file. Only `.txt` files are read.

### Requests

- One command per line, words separated by spaces. `#` starts a comment.
- The first line may be `owner <token>`; see [Owner](#owner).
- The first lines may also hold `request <uuid>`: the lab server's id for the
  request, a lower-case UUID. `state.json` names it on a start or stop the
  request makes; without it, the app makes one up. Anything else after
  `request` is a command, and fails as unknown.
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
controller=ready
owner=lab-B
error=
```

- `seq` is the name of the last request handled.
- `state` is `STOPPED`, `RUNNING` or `STOPPING` for a program, `NOT_READY`
  while the panel loads, `NO_PROGRAM` when the selected panel is not a
  program (the settings page).
- `program` is the selected program's identifier (`<game>:<program>`).
- `controller` is `ready` when the console's controller can take commands,
  `none` while it is not connected (after a program switch the app
  reconnects it, which can stall), empty for a panel without a console.
- `owner` is the owner token, empty when the channel is unclaimed.
- `error` is the error of the last request, empty once a request succeeds.

### state.json

`Lab/state.json` is the lab server's view of the app, version 1 of the
contract in `lab/server/` on the fork's `agents` branch (its README lists
the fields; `python3 -m pa_lab check app_state <file>` checks one). It
holds the state file's fields (as `program.state` in lower case, `panel`,
`controller.state` and `channel`), and also:

- `written_at`: the heartbeat. The file is checked at every poll (500 ms)
  and rewritten at once when anything but `written_at`, `video.frame_seq`
  and `video.captured_at` changed: a new `instance.id`, a finished request
  (`channel.last_request`), a start or stop, the video state. When only
  those three moved, it is rewritten at most once a second, with the newest
  frame. A long request (a `walk`) still holds the poll, and the heartbeat,
  until it ends.
- `instance.id`: a new UUID at every app start; `instance.pid`; on macOS
  `instance.profile`, the `--profile` name.
- `program.last_transition`: the last start or stop, and where it came
  from: `lab` (a request, with its `request_id`), `ui` (the Start/Stop
  button, or anything else outside the channel), `program` (it finished by
  itself) or `error` (it ended with an error). `null` until the first one,
  and again after a panel switch until that panel's program starts or
  stops. The program records who asked as it starts or stops
  (`ProgramSession`, `ProgramTransition.h`), and a stop keeps its asker
  through `stopping` to `stopped`: an error or the run's end after a Stop
  does not change who stopped it.
- `program.options_hash`: the hash of the selected program's options (the
  JSON the settings file saves for it, without `ConsoleSetup`): SHA-256 in
  hex of its compact text with sorted keys, so any option change, from the
  UI or from `set`, changes it. Absent without a program. A logical change:
  written at once.
- `video.frame_seq` and `captured_at`: the newest frame from the console's
  video, counted since the app started. `state` is `live` while that frame
  is at most 2 s old, `stale` when older, `missing` without video.
- `video.signal`, `observation.game` and `observation.screen` stay
  `unknown`: no detector feeds them yet.

Unlike the state file, `panel.id` is `null` when the selected panel is not a
program.

## Commands

### Channel

| Command | What it does |
|---|---|
| `select GAME:PROGRAM` | Shows that program, as clicking it in the program list does. The identifier is the one in `state` and in the settings file's `99-Panels` section, e.g. `PokemonBDSP:AutoStory`, `NintendoSwitch:CommandFileRunner`. Refused while a program runs. |
| `set KEY JSON` | Sets one option of the selected program by its key in the settings file, live: `set START_SEGMENT "0"`, `set NEW_GAME_INTRO true`, `set NOTIFICATIONS.Enabled false` (dots walk into groups). The value is JSON, so strings need quotes. Prints `set KEY = <value the program now holds>`, which differs from what was sent when the program clamped or refused it. Refused while the program runs. |
| `start [SECONDS]` | What clicking Start Program does. Refused unless the program is `STOPPED`. First waits up to `SECONDS` (default 30) for `controller=ready`, and refuses with `controller not connected` if it never is, instead of starting a program that would fail with "Incompatible Controller". |
| `stop` | What clicking Stop Program does. Refused unless the program is `RUNNING`. The request goes on; the program stops in the background. |
| `state` | Prints the state file's lines into the journal. (In the Command File Runner program, `state [image]` prints the detectors that fire instead; from the channel use `detect` per detector.) |
| `describe_program` | Prints the selected program's id and the static contract of its options as one line, `describe_program <JSON>`; see [Describe program](#describe-program). Reads only: needs no owner token and no console. Refused when the selected panel is not a program. |
| `preflight JSON` | Runs the selected program's start checks with these options, without starting it, and puts the prior options back; prints one line, `preflight <JSON>`; see [Preflight](#preflight). Needs the owner token, like a press. Refused unless the program is `STOPPED`. |
| `snapshot LABEL [TIMEOUT_MS]` | Saves the first video frame newer than this line as `<request>_<LABEL>.png` and names it; see [Snapshot](#snapshot). Reads only: needs no owner token, no program state and no controller. |
| `keepawake on\|off [seconds]` | While it is on and no program runs, presses B every `seconds` (default 15) so the console does not sleep. A running program pauses it. Off by default; off again when the app restarts. |
| `popups [close]` | Lists the app's open message boxes (title and text), such as the "Error" box a failed program leaves. `close` presses each one's default button. |
| `owner claim TOKEN`, `owner release` | The owner lock, below. |
| `help` | Prints every command and detector, including the ones games add. |

### Console

These run on the selected program's console. A press runs only while no
program runs (`STOPPED`); the commands that only read the screen (`shot`,
`shots`, `history`, `detect`, `wait_for`, `expect`, `game_open`) run any time.

#### Controller

| Command | What it does | Defaults |
|---|---|---|
| `press BTN[+BTN] [hold] [release]` | Presses one button, or several together, joined with `+` (e.g. `L+R`). Buttons: `A B X Y L R ZL ZR PLUS MINUS LCLICK RCLICK HOME CAPTURE`. | hold 160, release 160 |
| `dpad UP\|DOWN\|LEFT\|RIGHT [hold] [release] [BTN[+BTN]]` | Presses the D-pad, holding the buttons with it when given. | hold 100, release 200 |
| `taps DIR N [all] [hold] [release] [still]` | Taps the D-pad `N` times and measures the screen change (RMSD at 192x108, as `walk`) between the frame before and after each tap. Prints `tap k: change X moved\|still` per tap, `moved` when the change is at least `still`, then `moved M of N`. Stops after two still taps in a row (a wall) unless `N` is followed by `all`. On BDSP a step measures 21 to 50, a bump into a wall or an NPC brushing past 4 to 13. | hold 160, release 640, still 16 |
| `stick L\|R x y [hold] [release] [BTN[+BTN]]` | Moves the left or right stick. `x` and `y` run from -1 to 1; `y` is +1 for up. Buttons, when given, are held for the whole hold: `stick L 0 1 1000 160 B` runs north in BDSP. | hold 160, release 160 |
| `wait ms` | Waits. | |
| `walk x y [max] [still] [BTN[+BTN]]` | Holds the left stick (and the buttons, when given) in 200 ms slices until two slices in a row change the screen less than `still`: a wall or someone in the way. Prints `walk still\|limit after N ms, moving M ms` and each slice's change. For measuring maps: a timed `stick` hold moves a different number of tiles from try to try. | max 5000, still 10 |

#### Screen

| Command | What it does | Defaults |
|---|---|---|
| `shot label` | Saves the current frame as `<request>_<label>.png` in the outbox. | |
| `shots label count [interval]` | Saves `count` frames, `<label>_0` to `<label>_<count-1>`, one every `interval`. | interval 500 |
| `history label` | Saves the console's recent video as `<request>_<label>.mp4` in the outbox, when Stream History is on. Run it right after a program to keep a recording of that run. | |
| `game_open [image]` | Prints `game_open yes` when a game is in front, or `game_open no REASON` with `REASON` `home` (the Home menu), `user_select` ("Who is using the software?"), `black` (a black screen: loading, or no signal) or `no_video`. It does not tell one game from another. Never fails the line. With `image`, it checks a saved picture, as `detect` does. | |
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

### Snapshot

`snapshot LABEL [TIMEOUT_MS]` waits up to `TIMEOUT_MS` (100 to 60000,
default 5000) for a frame of the selected console's video that arrives
after the line starts, saves that frame, and prints one line naming it with
the same `instance.id` and `frame_seq` that `state.json` uses. `LABEL` is 1
to 64 of `A-Z a-z 0-9 _ -`. Unlike `shot`, it runs on any panel with video,
whatever the program state and the controller; a panel with no video (the
settings page, no panel) gives `unavailable`.

A request whose commands are all `snapshot` lines (the lab server's `shot`
writes `request <uuid>` and `snapshot frame <ms>`) runs on a second worker
that checks the inbox every 100 ms, so it never waits behind a long request
on the first (a `start` waiting for the controller, a batch of presses). Its
journal, `done/` move and `seq` are the same as any request's; requests
taken by the two workers may finish out of name order. A `snapshot` line in
any other request runs in order with that request's other lines.

| Line | Meaning |
|---|---|
| `snapshot saved instance=<uuid> frame_seq=<n> captured_at=<time> after=<n> path=<path>` | Saved. `frame_seq` and `captured_at` are the saved frame's; `after` is the newest `frame_seq` when the line started (0 before the first frame), so `frame_seq > after`. The path runs to the end of the line. |
| `snapshot timeout instance=<uuid> after=<n> timeout_ms=<ms>` | No newer frame in time. The line fails: `timeout: no frame newer than frame_seq <n> within <ms> ms`. |
| `snapshot unavailable instance=<uuid> reason=<reason>` | No frame to save. The line fails with `unavailable: ...`. `reason` is `no_video` (the selected panel has none), `panel_changed` (during the wait), `app_closing` or `save_failed`. |

```
request 5f0c2a8e-3d41-4b7a-9c55-0e6a1f2b7c10
snapshot frame 2000
```

```
snapshot saved instance=d8ea6a9d-1eab-4a40-b355-d88072b289a1 frame_seq=918275 captured_at=2026-11-14T06:10:12.124Z after=918274 path=.../Profiles/lab-B/Lab/outbox/r7_frame.png
ok 2 snapshot frame 2000 @0-41ms
end
```

### Describe program

`describe_program` tells the lab server what the selected program takes,
without running it or touching the console. The JSON (version 1, keys
sorted):

```
describe_program {"id":"PokemonFRLG:AutoStory","options":[...],"prechecks":"unsupported","schema":1}
```

- `id`: the program's identifier, as `select` takes it.
- `options`: one entry per option the settings file saves, in the order the
  program shows them. Each has `key` (what `set` takes; a group's options
  are under the group's key, so `set GROUP.KEY`), `type`, and by type:

  | `type` | Fields |
  |---|---|
  | `boolean`, `string` | `default` |
  | `integer`, `number` | `default`, `min`, `max` (`null` when a number has no bound) |
  | `enum`, `select` | `default` and `entries`, each `{"slug", "enabled"}`; `set` takes the slug |
  | `group` | `toggleable` (it has its own `Enabled` key) and its own `options` |
  | `other` | nothing yet: the contract does not describe that type |

  The defaults are the program's defaults, not the values it holds now.
  Which options show and whether a combination is valid depend on the other
  options' values; they are not part of the contract.
- `prechecks`: the checks the program runs before it starts, in that order,
  each `{"name", "needs", "skippable"}`: a stable name (`black_border`,
  `free_party_slots`, ...), one line of what the program needs, and whether
  the user can turn it off. `unsupported` when the program declares no
  precheck list (`ProgramInstance::precheck_summaries()`); that never means
  the checks passed. An empty list means the program needs nothing.

### Preflight

`preflight JSON` asks whether a run with these options would pass its start
checks, without starting it:

```
preflight {"NOTIFICATIONS":{"Enabled":false},"START":"02"}
```

The JSON holds only the options to change, by the keys of `describe_program`
(a group's value is an object of its own keys). The program session
(`ProgramSession::Prechecks::run_only()`, `run_prechecks_only()`) loads and
reads back options on the UI thread, as `set` does, and runs the checks on
the channel's worker, which holds the panel throughout:

1. refuses unless the program is `STOPPED`, and makes Start refuse until it
   is done;
2. loads the options over the current ones (not saved to the settings
   file), refuses a key the program does not have, and checks that the
   program holds every value (a clamped number or a disabled choice is
   refused) and that `check_validity()` passes;
3. runs the program's prechecks in order (`ConsoleProgramInstance::run_prechecks()`;
   a Switch program overrides `run_switch_prechecks()` with
   `precheck_summaries()`), up to the first failed one; a check may open a
   menu or go Home, and comes back;
4. loads the prior options back, also when a check throws, and compares;
   but if the options differ from what step 2 loaded, a user edited them in
   the panel during the checks: it keeps them and names the edited keys.

It prints one line (version 1, keys sorted):

```
preflight {"checks":[{"message":"","name":"black_border","status":"passed"}],"edited_keys":[],"failure":null,"id":"PokemonFRLG:GiftRng","invalid":null,"options_hash":"9f2c...","restored":true,"schema":1,"verdict":"passed"}
```

- `verdict`: `passed`; `failed` with `failure` `{"check", "message"}`, the
  first failed check; `invalid_options` with `invalid` `{"key", "message"}`
  (`key` is the dotted key path, or `""` when `check_validity()` refused the
  combination); `cancelled` when the checks were cancelled before they
  finished (a panel change, or the app closing); `unsupported` when the
  program declares no precheck list.
- `checks`: `{"name", "status", "message"}` in the order they ran,
  `status` `passed`, `failed` or `skipped`; empty unless checks ran.
- `restored`: whether the program holds the prior options again.
- `edited_keys`: the top-level option keys a user changed during the checks,
  which the run kept; `restored` is then `false`. Empty otherwise.
- `options_hash`: the hash of the options the program holds once the run is
  over, read after the restore, as `program.options_hash` in `state.json`:
  the prior options when `restored`, the user's edit when `edited_keys` is
  not empty; never the candidate's.

A program that is not `STOPPED`, a controller that is not ready or a check
that throws fails the line (no `preflight` line); the options are put back
all the same. A panel change during the run cancels the checks; the options
are still put back before the panel goes, and the reply says `cancelled`
with `restored`. The UI thread is then waiting in `set_panel()` for the
panel this worker holds, so the worker loads them itself (the settings are
saved after that, on exit too).

## Owner

Two scripts driving one console would interleave their presses. A script
claims the channel with `owner claim TOKEN`; from then on every request that
changes anything (`select`, `set`, `start`, `stop`, `keepawake`, `popups`, presses)
must start with the line `owner TOKEN`, or it is refused with
`owned by TOKEN`. Requests that only read (`state`, `help`, `shot`, `detect`,
...) need no token. `owner release`, with the owner line, frees the channel.
The lock lives in memory: it dies with the app. The holder may claim again
with the same token (e.g. after a script restart).

## User hold

When the user drives the console, the lab keeps off it. The file `Lab/hold`
names the holder:

```
owner=user
reason=ui_start
acquired_at=2026-11-15T21:04:05.120Z
```

While it names an owner, every command that presses, starts, stops or changes
the program (`press`, `stick`, `dpad`, `close_game`, `start`, `stop`, `set`,
`select`, `preflight`, `popups`, `keepawake on`, ...) fails with
`console held by user since ... (REASON): the user is on the console;
~/BDSPLab/hold.sh off releases it`, and the channel's own keep-awake presses
stop. `help`, `state`, `owner`, `snapshot`, `describe_program`, `keepawake off`
and the console commands that only read the screen (`shot`, `detect`, ...)
still run. Owner tokens do not matter: the hold refuses every client.

The app writes the file itself when the user drives the selected program:
a Start or Stop clicked in the window (`ui_start`, `ui_stop`; a start a lab
request did not ask for is a UI start) or a press from the window's
keyboard or gamepad input that reached the console (`user_input`). Each is
noticed once, at the next poll or before the next refused command. Nothing
clears the hold by itself, an app restart included: `~/BDSPLab/hold.sh off`
or the lab server's `lease release-user` removes the file, and `hold.sh on`
puts it on by hand. A command already running when the user takes over
(a long `walk`) finishes; the next one is refused.

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

# Wait for the run to end.
until grep -q '^state=STOPPED' "$P/Lab/state"; do sleep 2; done
cat "$P/Lab/state"
```

`r2.out`, the journal of the second request:

```
ok 2 select PokemonBDSP:AutoStory @0-412ms
set START_CHECKPOINT_TUTORIAL = "15"
ok 3 set START_CHECKPOINT_TUTORIAL "15" @412-418ms
set END_CHECKPOINT_TUTORIAL = "15"
ok 4 set END_CHECKPOINT_TUTORIAL "15" @418-423ms
ok 5 start @423-431ms
end
```

The state file while it runs, and after:

```
seq=r2                      seq=r2
state=RUNNING               state=STOPPED
program=PokemonBDSP:AutoStory
controller=ready
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
`state` has the six lines above. Scripts move from
`printf '' > LabControl/start` to a request file containing `start`.

## Tests

`add_tests_LabChannel()` covers the request format, the journal lines, the
state text, the owner lock, the keep-awake schedule, the controller wait before `start` and which
program states let a console command run (`LabChannel - ...`
unit tests); `add_tests_LabHold()` covers the `Lab/hold` text, which commands run while held and
which Start, Stop or press puts the hold on, once each; `add_tests_LabSnapshot()` covers `snapshot`'s arguments, its wait for a newer frame and its lines; `add_tests_LabStateJson()` checks `state.json` against the
contract's golden files, which transition it names after a panel switch, and when it is rewritten; `add_tests_ProgramTransition()` covers
who each start and stop is said to come from; `add_tests_CommandFileRunner()` covers the console commands'
arguments. Run them with the app's `--command-line-test-mode`. Selecting,
setting, starting and the presses need a live app and console.
