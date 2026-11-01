# Lab scripts

Three portable `sh` scripts for driving a SerialPrograms app and its console
from the shell, without a window click. They speak the two file protocols
the app offers to scripts:

- the **Lab Control folder** (`LabControl/` in the profile's user-files
  folder): `start`, `stop` and the app-written `state` file. Protocol:
  [`SerialPrograms/Source/CommonFramework/Windows/LabControl.md`](../../SerialPrograms/Source/CommonFramework/Windows/LabControl.md).
- the **Command File Runner** folders (`CommandFiles/inbox`, `outbox`,
  `done` in the same user-files folder, unless the program's Command Folder
  option says otherwise). Commands and result format:
  [`SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/README.md`](../../SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/README.md).

| Script | Does | Parameters |
|---|---|---|
| `lablock.sh` | One user of a console at a time: `run [-w] NAME -- CMD...`, `check`, `status`. | `-c CONSOLE` or `LAB_CONSOLE` (default `default`); `LAB_DIR` for the lock folder (default `~/.lab`); `LABLOCK_TOKEN` passed to the command. |
| `labctl.sh` | `start`, `stop` or read the `state` of the selected program through the Lab Control folder; `supports PATH` tells whether a build has it. | `-p PROFILE` or `LAB_PROFILE` (the profile's user-files folder); `LABCTL_WAIT` seconds to wait for the state (default 60). |
| `cfrun.sh` | Send one Command File Runner batch (stdin) and print its `.out`. | `-p PROFILE` or `LAB_PROFILE`; `LAB_CFR_DIR` if the Command Folder option is set; `CFRUN_WAIT` seconds per batch (default 600); `CFRUN_SELECT`, see the gap below. |

Each script's header comment has the full usage and exit codes.

## One train on the console

A console is a shared, stateful, slow peripheral, and the app can only do
one thing with it. `lablock.sh` makes that a rule: whoever holds the
console's lock drives it, and `labctl.sh start|stop` and `cfrun.sh` refuse
without it. `run` holds the lock for the life of one command (a run script,
a batch), exports `LABLOCK_TOKEN` to it, and releases the lock when it
exits; scripts started inside that command inherit the token and pass
`check`, so a run script may call `labctl.sh` and `cfrun.sh` freely. A
script from somewhere else, say a probe loop left over from an earlier run,
carries no token and is refused: it cannot stop or relaunch what the current
run drives. A lock whose holder process has exited is stale and is cleared
on the next call.

The console name only names the lock (`$LAB_DIR/locks/CONSOLE`); one lock
per console, one app per console, one profile per app.

## Profiles

`PROFILE` is the app's user-files folder: the one with `SerialPrograms.log`,
`Screenshots/` and the settings. On macOS it is
`~/Library/Application Support/SerialPrograms`, or
`.../SerialPrograms/Profiles/NAME` for an app launched with
`open -n SerialPrograms.app --args --profile NAME` (one profile per console
is how two apps run side by side). On Windows and Linux it is the folder the
app was started in. `labctl.sh supports BUILD` says whether a build has the
Lab Control folder at all.

## What `cfrun.sh` does and does not do

It takes the lock, reads the Lab Control `state` file and acts on what it
finds, never stopping a program to learn which one is selected:

- another program is `RUNNING`: refuse, touch nothing (exit 3);
- Prevent Sleep is `RUNNING`: the lab's own keep-awake, stop it;
- the Command File Runner is selected: start it if stopped, send the batch,
  wait for `outbox/NAME.out`, print it (exit 1 on an `error line`);
- another program is selected: exit 4.

The gap: the app has no way to select a program from a script. The Lab
Control folder starts and stops the selected panel; selecting the Command
File Runner means either a profile whose settings remember it, or local UI
automation (relaunch the app on that profile, click the program). That
automation is site-specific and stays out of this repo; `CFRUN_SELECT` is
the hook for it, a command `cfrun.sh` runs when another program is
selected, after which it expects the runner to be running.

## Worked example

```sh
export LAB_CONSOLE=B
export LAB_PROFILE="$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B"
cd devtools/lab

sh lablock.sh status                       # B: free
sh labctl.sh state                         # state=STOPPED / program=NintendoSwitch:CommandFileRunner

# Take the lock for one script that sends a batch and reads the result.
sh lablock.sh run probe-door -- sh -c '
    printf "%s\n" "press A 100 1500" "shot after_a" "detect home" | sh cfrun.sh probe-door
'
#   ok 1 press A 100 1500 @0-1600ms
#   shot .../lab-B/CommandFiles/outbox/probe-door_after_a.png
#   ok 2 shot after_a @1600-1650ms
#   detect home no
#   ok 3 detect home @1650-1660ms
#   end
sh lablock.sh status                       # B: free again; the lock went with the command
```

`cfrun.sh` on its own takes the lock for the one batch, so the `lablock.sh
run` wrapper is only needed when several commands must stay one train (a
build, a deploy and a run; a batch and the screenshots it leaves). Write a
batch to a file first when it has more than a few lines; name batches by
their question, and end each with a `shot`.

Stop whatever runs and read the state:

```sh
sh labctl.sh stop                          # STOPPED (NintendoSwitch:CommandFileRunner)
```
