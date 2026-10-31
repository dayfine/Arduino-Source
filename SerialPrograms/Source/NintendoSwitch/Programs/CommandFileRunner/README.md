# Command File Runner

## Program Description

A developer tool for working on other programs. It runs short text files of commands against the live console: press buttons, take screenshots and check whether a detector fires. That way, a question like "does this detector see the menu after I press X?" gets answered in seconds, without a rebuild and a full program run.

You drop a command file into a folder. The program runs it one line at a time and writes a result file with the timing of each line, anything the commands printed, and screenshots. A file stops at its first failing line, with a screenshot of that moment, so you can see exactly where a sequence went wrong.

Because files are picked up from a folder, the commands can come from you, a script or another tool, while the program keeps running.

It is listed under Nintendo Switch → Testing.

## Instructions

1. Select the console, video and controller as for any other program, and start Command File Runner.
2. Write a command file, for example `probe.txt`:
    ```
    # Open Home and check that the Home detector sees it.
    press HOME
    expect home 3000
    shot home_menu
    ```
3. Save it into the `inbox` folder (see [Folders](#folders)). It runs within half a second.
4. Read `outbox/probe.out` when it appears. For the file above it looks like this (times vary):
    ```
    ok 2 press HOME @0-322ms
    expect home after 2ms
    ok 3 expect home 3000 @322-325ms
    shot .../CommandFiles/outbox/probe_home_menu.png
    ok 4 shot home_menu @325-371ms
    end
    ```

Files run one at a time, in name order. Each file is moved to `done` when it starts. Stop the program with the Stop button, or with a file containing `stop`.

## Folders

The command folder holds three folders:

| Folder | Contents |
| --- | --- |
| `inbox` | Command files waiting to run. Only `.txt` files are read. |
| `done` | Command files that have been run. |
| `outbox` | Results (`<name>.out`) and screenshots (`<name>_<label>.png`). |

By default the command folder is `CommandFiles` in the settings folder, next to `UserSettings`.

## Command Files

- One command per line, with words separated by spaces.
- `#` starts a comment, either on its own line or after a command.
- Durations are in milliseconds.
- Arguments in `[brackets]` are optional and default as listed below.

## Result Files

`<name>.out` appears only when the whole file has finished, so a script can wait for it. Until then, `<name>.tmp` shows the progress so far.

| Line | Meaning |
| --- | --- |
| `ok <line> <command> @<start>-<end>ms` | The command on that line of the file succeeded. Times are from the start of the file. |
| `error line <line> (<command>): <reason>` | The first failure. The file stops here and a `shot` line follows with a screenshot of the moment. |
| `end` | Always the last line. |

Any other line comes from a command, such as `detect home yes` or the path of a saved screenshot.

## Commands

### Controller

| Command | What it does | Defaults |
| --- | --- | --- |
| `press BTN[+BTN] [hold] [release]` | Presses one button, or several together, joined with `+` (e.g. `L+R`). Buttons: `A B X Y L R ZL ZR PLUS MINUS LCLICK RCLICK HOME CAPTURE`. | hold 160, release 160 |
| `dpad UP\|DOWN\|LEFT\|RIGHT [hold] [release]` | Presses the D-pad. | hold 100, release 200 |
| `stick L\|R x y [hold] [release]` | Moves the left or right stick. `x` and `y` run from -1 to 1; `y` is +1 for up. | hold 160, release 160 |
| `wait ms` | Waits. | |
| `walk x y [max] [still]` | Holds the left stick in 200 ms slices until two slices in a row change the screen less than `still` (RMSD at 192x108): a wall or someone in the way. Prints `walk still\|limit after N ms, moving M ms` and each slice's change. For measuring maps: a timed `stick` hold moves a different number of tiles from try to try. | max 5000, still 10 |

### Screen

| Command | What it does | Defaults |
| --- | --- | --- |
| `shot label` | Saves the current frame as `<file name>_<label>.png` in the outbox. | |
| `shots label count [interval]` | Saves `count` frames, `<label>_0` to `<label>_<count-1>`, one every `interval`. | interval 500 |
| `detect detector [image]` | Runs a detector once and prints `yes` or `no`. With `image`, it runs on a saved picture instead of the live video: an absolute path, or a file name in the outbox. | |
| `wait_for detector [timeout]` | Checks the video every 100 ms until the detector fires. Prints how long it took, or `timeout`. | timeout 5000 |
| `expect detector [timeout]` | The same as `wait_for`, but a timeout fails the line. | timeout 5000 |
| `state [image]` | Prints every detector that fires on the current frame, or on a saved picture. | |

### Switch

| Command | What it does |
| --- | --- |
| `close_game` | Goes to the Home menu from anywhere and closes the game. |

### Other

| Command | What it does |
| --- | --- |
| `help` | Prints every command and detector, including the ones games add. |
| `stop` | Stops the program after this line. |

## Detectors

These work on any game:

| Name | Detects |
| --- | --- |
| `home` | The Switch Home menu. |
| `user_select` | The "Who is using the software?" screen. |
| `black` | A black screen. |
| `white` | A white screen. |

Games can add their own commands and detectors with `register_command_file_command()` and `register_command_file_detector()` (see `NintendoSwitch_CommandFileRunner.h`); `help` lists them.

## Options

- **Command Folder:** The folder holding `inbox`, `outbox` and `done`. Leave it empty to use `CommandFiles` in the settings folder.
- **Stop When Inbox Is Empty:** Run the files already in the inbox, then stop, instead of waiting for more.

## Tips

- Save a screenshot right before an `expect` you're unsure of. If the line fails, the error screenshot shows what came after.
- To tune a detector, save frames with `shots`, then run `detect` or `state` on the saved images. The console doesn't need to be on that screen again.
- Write the file somewhere else and then move it into `inbox`, so the program never reads a half-written file.
