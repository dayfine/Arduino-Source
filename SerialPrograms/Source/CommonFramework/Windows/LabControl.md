# Lab Control

Lets a script start and stop the selected program of a running SerialPrograms
app, and read its state, through files in a folder: no window focus, no
click. Meant for a lab that drives one app per console from the shell.

## Turning it on

Create the folder `LabControl/` inside the profile's user-files folder (the
folder that holds `SerialPrograms.log`, `Screenshots/` and the settings).
While it exists the app polls it every 500 ms from the UI thread. Without it
the app behaves as before: normal users see no change.

## Files

| File | Direction | Meaning |
|---|---|---|
| `start` | script writes | Does what clicking Start Program does for the selected panel. The file is deleted once acted on. |
| `stop` | script writes | Does what clicking Stop Program does. Deleted once acted on. |
| `state` | app writes | The current state, rewritten whenever it changes (see below). |

Write `start` and `stop` atomically (write `start.part`, then rename it to
`start`): only the exact names `start` and `stop` act, so a half-written
file is ignored.

`state` holds two or three lines:

```
state=STOPPED
program=PokemonBDSP:AutoStory
error=The selected panel is not a program.
```

- `state` is `STOPPED`, `RUNNING` or `STOPPING` for a program, `NOT_READY`
  while the panel loads, `NO_PROGRAM` when the selected panel is not a
  program.
- `program` is the selected program's identifier (`<game>:<program>`), empty
  for `NO_PROGRAM`.
- `error` appears only after a refused or failed `start`/`stop` and names the
  reason; it clears on the next successful action.

The selected panel is the one the window shows; Lab Control does not select
programs. A script selects a program by launching the app with a profile
whose settings remember that panel, or by its own means, then reads `program`
to confirm before writing `start`.

## From a script

```sh
P="$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B"   # the profile
mkdir -p "$P/LabControl"
sed -n 's/^program=//p' "$P/LabControl/state"          # which program is selected
printf '' > "$P/LabControl/start.part" && mv "$P/LabControl/start.part" "$P/LabControl/start"
until grep -q '^state=RUNNING' "$P/LabControl/state"; do sleep 0.5; done
```

Read `state` to learn what the app is doing; never stop a program to find out
which one is selected. A `stop` while a program is `STOPPING` is ignored.

## Tests

`add_tests_LabControl()` covers the file-name parsing and the `state` file's
text (`LabControl: ...` unit tests); the unit test runner reads the same folder
when it runs under a lab profile.
