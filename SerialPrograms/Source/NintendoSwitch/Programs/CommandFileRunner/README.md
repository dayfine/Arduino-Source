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
| `taps DIR N [all] [hold] [release] [still]` | Taps the D-pad `N` times and measures the screen change (RMSD at 192x108, as `walk`) between the frame before and after each tap. Prints `tap k: change X moved\|still` per tap, `moved` when the change is at least `still`, then `moved M of N`. Stops after two still taps in a row (a wall) unless `N` is followed by `all`. A tap toward a new direction only turns the player, so send one tap first when changing direction. On BDSP a step measures 21 to 50, a bump into a wall or an NPC brushing past 4 to 13. | hold 160, release 640, still 16 |
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

### Pokemon Brilliant Diamond and Shining Pearl

| Name | Detects |
| --- | --- |
| `bdsp_dialog` | A short dialog box: talking to someone, signs, item pickups, the EXP text after a battle. |
| `bdsp_battle_text` | The long dialog box that only appears in battle. |
| `bdsp_battle_menu` | The Fight / Pokemon / Bag / Run menu of a standard (wild) battle. |
| `bdsp_exp_gain` | The experience gain screen after a battle. |
| `bdsp_menu` | The X menu. |
| `bdsp_map` | The town map. |

These are the detectors a lab operator checks between steps while measuring maps and story routes: dialog and the battle menu say what stopped a walk, the menu and map say where a key press landed. They are all cheap box checks, so `state` stays fast.

## Game-specific commands

### Pokemon Brilliant Diamond and Shining Pearl

| Command | What it does |
| --- | --- |
| `bdsp_load_save` | From anywhere: `close_game`, launch the tile the Home menu has highlighted (A, A, wait 35 s, A, wait 3 s, A, A held 6 s, wait 8 s) and print `state`. |

**`bdsp_load_save` launches whatever Home has highlighted, and Home highlights the last game played, not BDSP.** Before sending it, take a `shot` of the Home menu and move the highlight to BDSP with `dpad`; a wrong tile launches another game and the command reports its `state` as if nothing were wrong. The console has been sent into Pokemon Violet this way.

### How a game registers them

A game adds commands and detectors with `register_command_file_command()` and `register_command_file_detector()` from `NintendoSwitch_CommandFileRunner.h`, and `help` lists them. The registration has to run at startup, before any command file: put it in a function in the game's own module (BDSP: `register_command_file_commands()` in `PokemonBDSP/Programs/PokemonBDSP_CommandFileCommands.cpp`, guarded so a second call does nothing) and call it from the game's `PanelListFactory` constructor, which the program list builds when the app starts. A static registrar object would not do: the game code is in a static library, and the linker drops an object file nothing references.

A registered command gets a `CommandFileSession` (the environment, controller context, output stream and outbox) and its line's words, and can run built-in commands with `run_command_file_line()`, as `bdsp_load_save` does for `close_game` and `state`. Detector names are prefixed with the game (`bdsp_`) so `state` output reads at a glance.

## Options

- **Command Folder:** The folder holding `inbox`, `outbox` and `done`. Leave it empty to use `CommandFiles` in the settings folder.
- **Stop When Inbox Is Empty:** Run the files already in the inbox, then stop, instead of waiting for more.

## Tips

- Save a screenshot right before an `expect` you're unsure of. If the line fails, the error screenshot shows what came after.
- To tune a detector, save frames with `shots`, then run `detect` or `state` on the saved images. The console doesn't need to be on that screen again.
- Write the file somewhere else and then move it into `inbox`, so the program never reads a half-written file.
