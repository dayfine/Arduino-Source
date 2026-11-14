# Lab scripts

Three portable `sh` scripts for driving a SerialPrograms app and its console
from the shell, without a window click. They speak the app's **lab
channel**: the folder `Lab/` in the profile's user-files folder, where a
request is a text file in `inbox/`, its journal appears in `outbox/`, and
`state` says what the app is doing. Protocol, commands and file formats:
[`SerialPrograms/Source/CommonFramework/Windows/Lab.md`](../../SerialPrograms/Source/CommonFramework/Windows/Lab.md).

| Script | Does | Parameters |
|---|---|---|
| `lablock.sh` | One user of a console at a time: `run [-w] NAME -- CMD...`, `check`, `status`. | `-c CONSOLE` or `LAB_CONSOLE` (default `default`); `LAB_DIR` for the lock folder (default `~/.lab`); `LABLOCK_TOKEN` passed to the command. |
| `labctl.sh` | One channel command per call: `state`, `start`, `stop`, `select ID`, `set KEY JSON`, `owner claim TOKEN`, `owner release`, `keepawake on\|off [S]`, `send NAME` (any request from stdin); `supports PATH` tells whether a build has the channel. | `-p PROFILE` or `LAB_PROFILE`; `LAB_OWNER` for the owner line; `LABCTL_WAIT` seconds per request and for start/stop to settle (default 60). `start` refuses with `controller not connected` when the app's controller never becomes ready (`controller=` in `state`). |
| `cfrun.sh` | Send one batch of console commands (stdin) under the console's lock and print its journal. | `-p PROFILE` or `LAB_PROFILE`; `LAB_OWNER`; `CFRUN_WAIT` seconds per batch (default 600). |
| `cfr_steps_to_story.py` | Turn a batch's `Lab/outbox/<batch>.steps.jsonl` (one line per command) into draft PLZA story table rows: one step per `lza_*` action, raw inputs collapsed into one `RAW n inputs` line to review. Tests: `test_cfr_steps_to_story.py`. | `STEPS` file; `--first-id` (default `01.01`). |

Each script's header comment has the full usage and exit codes.

## One train on the console

A console is a shared, stateful, slow peripheral, and the app can only do
one thing with it. `lablock.sh` makes that a rule on this machine: whoever
holds the console's lock drives it, and `labctl.sh` and `cfrun.sh` refuse
without it. `run` holds the lock for the life of one command (a run script,
a batch), exports `LABLOCK_TOKEN` to it, and releases the lock when it
exits; scripts started inside that command inherit the token and pass
`check`, so a run script may call `labctl.sh` and `cfrun.sh` freely. A
script from somewhere else, say a probe loop left over from an earlier run,
carries no token and is refused. A lock whose holder process has exited is
stale and is cleared on the next call.

The channel's **owner** is the same rule inside the app, for scripts that
don't share this machine's lock folder: once `owner claim TOKEN` is sent,
the app refuses any request that changes something unless its first line is
`owner TOKEN`. Set `LAB_OWNER=TOKEN` and the scripts add that line. Both
rules are cheap; use both.

## Profiles

`PROFILE` is the app's user-files folder: the one with `SerialPrograms.log`,
`Screenshots/` and the settings. On macOS it is
`~/Library/Application Support/SerialPrograms/Profiles/NAME` for a lab app
launched with `--args --profile NAME` or built with `PA_LAB_PROFILE=NAME`.
Lab builds refuse an unprofiled macOS GUI launch; they never fall back to the
live app's default settings directory. On Windows and Linux it is the folder the app was
started in. `labctl.sh supports BUILD` says whether a build has the channel
at all.

## Running a lab app beside the live app

The lab drives its consoles from apps that run next to the user's own
SerialPrograms, each on its own profile, so nothing of the live app is
touched. On macOS:

1. **A separate copy of the app per console**, with its own bundle id and
   name, so macOS treats it as a distinct application with its own camera
   and microphone permissions:
   ```sh
   cp -R SerialPrograms.app SerialPrograms-B.app
   /usr/libexec/PlistBuddy -c 'Set :CFBundleIdentifier com.PokemonAutomation.SerialPrograms.labB' \
       -c 'Set :CFBundleName SerialPrograms-B' SerialPrograms-B.app/Contents/Info.plist
   codesign --force --deep --sign "$LAB_SIGNING_IDENTITY" SerialPrograms-B.app
   ```
   Two consoles, two copies, two bundle ids.
2. **Launch it through LaunchServices**, never by running the binary from
   a shell: `open -n -a SerialPrograms-B.app --args --profile lab-B`. A
   binary started from a shell inherits the shell's camera permission, and
   the capture card then delivers no frames.
3. **`--profile NAME`** is the app's own startup-profile support: settings,
   log, screenshots and the `Lab/` folder all live under `Profiles/NAME/`,
   so the live app's default profile is untouched.
4. **Each profile's settings bind its panels to that console's** serial port
   and capture device (the `ConsoleSetup` of each program in the settings
   file), so two lab apps drive two consoles at once.

On Windows and Linux the separate copy is a second folder; `--profile` is
unsupported and rejected.

## Startup isolation and tests

For safe preference seeding and checked packaging of two lab apps, see
[INSTANCE_PREPARATION.md](INSTANCE_PREPARATION.md). Its staging tool validates the
linked build's profile receipt before signing and records the final package hashes.
Physical-device ownership remains a separate required boundary.

Bind each deployed macOS lab binary to its console when configuring its existing
build directory, for example:

```sh
cmake -S SerialPrograms -B /path/to/build -DPA_LAB_PROFILE=lab-B
```

This compiles the identity into the executable. Finder and macOS permission-dialog
relaunches need no arguments and still select `Profiles/lab-B`. An explicit
`--profile lab-A` against that binary exits with status 2 before Qt, settings,
logging or device discovery. Changing the bundle's name or identifier does not
change its compiled profile; build each console's binary with the correct value.
The existing signing/bundle-identity procedure still applies.

With `PA_LAB_PROFILE` empty (the build default), macOS GUI startup requires
`--profile NAME`; a no-argument relaunch safely exits. Names are ASCII letters,
digits, `_` and `-`, cannot start with `-`, and must be a single path component.
Empty, missing, duplicate and malformed names are rejected rather than rewritten.

On this baseline, run the registered native tests with:

```sh
./SerialPrograms.app/Contents/MacOS/SerialPrograms --command-line-test-mode
# Optional image augmentation:
./SerialPrograms.app/Contents/MacOS/SerialPrograms --command-line-test-mode --augment-color-distortions
```

Explicit tests use `QCoreApplication`, bypass settings loading and window
restoration, and never enter startup's serial/camera discovery. macOS test file
output uses `Profiles/unit-tests`; no console profile can be supplied with tests.
The legacy persisted test-mode setting remains supported, but only after a valid
GUI profile is selected. It also runs before device discovery. Prefer the explicit
flag so no saved settings are loaded.

Unknown or incomplete arguments fail closed with status 2. `--test bdsp` and `--test-filter=...`
are not supported by this baseline. Documented Qt application options (including `-platform offscreen`, `-style Fusion`,
`-style=Fusion`, stylesheet, window and plugin options) are preserved in order and
passed to Qt. Missing values and unknown option spellings are rejected; values in
option/value pairs must not begin with `-`. Qt environment variables such as
`QT_QPA_PLATFORM` remain supported.
The separate `SerialProgramsTests` runner is an independent pending feature;
this lab change does not import it.

The colocated `StartupArguments_Tests.cpp` table covers the general grammar,
ordinary unprofiled GUI startup, and Qt argument preservation. The separate
`LabStartupPolicy_Tests.cpp` table covers rejection, profile binding, no-argument
lab relaunch and exclusive dispatch to the test entry point. Its injected
GUI callback represents the boundary that owns settings/window/hardware startup.
These tests verify that boundary is not entered; they do not exercise real devices
or claim that all unit-test bodies are incapable of opening hardware.

Profiles isolate settings, not physical serial devices. A mistakenly configured
profile can still refer to another console's port. Enforcing ownership before a
physical port opens requires a separate device-lease change; the channel owner
and shell lock do not enforce that boundary.

See [STARTUP_ISOLATION.md](STARTUP_ISOLATION.md) for the review split and upstream path.

## Worked example: run a program with nothing clicked

```sh
export LAB_CONSOLE=B
export LAB_PROFILE="$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B"
export LAB_OWNER=lab-B
cd devtools/lab

mkdir -p "$LAB_PROFILE/Lab"                      # turns the channel on
open -n -a SerialPrograms-B.app --args --profile lab-B
until sh labctl.sh state >/dev/null 2>&1; do sleep 1; done

sh labctl.sh owner claim lab-B                   # ok 2 owner claim lab-B @0-0ms / end
sh labctl.sh select PokemonBDSP:AutoStory        # ok 2 select PokemonBDSP:AutoStory @0-412ms / end
sh labctl.sh set START_CHECKPOINT_TUTORIAL '"15"'
#   set START_CHECKPOINT_TUTORIAL = "15"
#   ok 2 set START_CHECKPOINT_TUTORIAL "15" @0-6ms
#   end
sh labctl.sh set END_CHECKPOINT_TUTORIAL '"15"'
sh labctl.sh start                               # RUNNING (PokemonBDSP:AutoStory)

until sh labctl.sh state | grep -q '^state=STOPPED'; do sleep 2; done
sh labctl.sh state
#   seq=labctl-4242-1762000000
#   state=STOPPED
#   program=PokemonBDSP:AutoStory
#   controller=ready
#   owner=lab-B
#   error=

sh labctl.sh stop                                # for an early end: STOPPED (PokemonBDSP:AutoStory)
sh labctl.sh owner release
```

The same as one request file, sent with `labctl.sh send`:

```sh
sh labctl.sh send run15 <<'EOF'
select PokemonBDSP:AutoStory
set START_CHECKPOINT_TUTORIAL "15"
set END_CHECKPOINT_TUTORIAL "15"
start
EOF
```

(`LAB_OWNER` puts `owner lab-B` on the first line.) Its journal,
`Lab/outbox/run15.out`:

```
ok 2 select PokemonBDSP:AutoStory @0-412ms
set START_CHECKPOINT_TUTORIAL = "15"
ok 3 set START_CHECKPOINT_TUTORIAL "15" @412-418ms
set END_CHECKPOINT_TUTORIAL = "15"
ok 4 set END_CHECKPOINT_TUTORIAL "15" @418-423ms
ok 5 start @423-431ms
end
```

A request from a script without the token is refused and its journal says
who holds it: `error 2 (start): owned by lab-B`.

## Probing the console between runs

```sh
sh lablock.sh run probe-door -- sh -c '
    printf "%s\n" "press A 100 1500" "shot after_a" "detect home" | sh cfrun.sh probe-door
'
#   ok 1 press A 100 1500 @0-1600ms
#   shot .../lab-B/Lab/outbox/probe-door_after_a.png
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
their question, and end each with a `shot`. Presses run only while no
program runs; the app refuses them otherwise and the journal says so. Read
`labctl.sh state` to learn what runs; never stop a program to find out.

Between runs, `sh labctl.sh keepawake on` has the app press B every 15 s
while nothing runs, so the console does not sleep; no Prevent Sleep program
to start and stop.
