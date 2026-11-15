# Command File Runner

## Program Description

A developer tool for working on other programs. It runs short text files of commands against the live console: press buttons, take screenshots and check whether a detector fires. That way, a question like "does this detector see the menu after I press X?" gets answered in seconds, without a rebuild and a full program run.

The same commands are available without running any program through the **lab channel**, the `Lab/` folder documented in [CommonFramework/Windows/Lab.md](../../../CommonFramework/Windows/Lab.md), which also selects programs, sets their options and starts and stops them. That document is the reference for the commands, the detectors, the file formats and the journal. This program remains for running a batch of command files as a program: while it runs, it reads files from a folder of its own and writes the same journals.

It is listed under Nintendo Switch -> Testing.

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

By default the command folder is `CommandFiles` in the settings folder, next to `UserSettings`. Don't point it at the lab channel's `Lab/` folder: the channel reads that one.

## Commands

Every console command of [Lab.md](../../../CommonFramework/Windows/Lab.md#console) (`press`, `dpad`, `taps`, `stick`, `wait`, `walk`, `shot`, `shots`, `history`, `detect`, `wait_for`, `expect`, `state`, `close_game`, `help`), plus `stop`, which stops the file and the program. The channel's own commands (`select`, `set`, `start`, `owner`, `keepawake`) are not commands of this program. Here `state [image]` prints every detector that fires on the current frame or on a saved picture.

## Options

- **Command Folder:** The folder holding `inbox`, `outbox` and `done`. Leave it empty to use `CommandFiles` in the settings folder.
- **Stop When Inbox Is Empty:** Run the files already in the inbox, then stop, instead of waiting for more.

## Tests

`add_tests_CommandFileRunner()` covers the parsing of command arguments (`CommandFileRunner - ...` unit tests); the file and journal formats are covered by the lab channel's tests.
