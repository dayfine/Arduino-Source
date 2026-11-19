# Know which save is on the console

[Back to the guide](../GUIDE.md)

**Not yet.** Designed, no code: nothing records saves, so the server always says the save is
`unknown`. Today, check the save yourself before a checkpoint run. The examples below are
samples with made-up values.

Most lab mistakes that cost real progress are about the save: starting a checkpoint run from
the wrong save, a hand session moving the player after the last save, or a PLZA autosave moving
the save past a checkpoint's start.

## What you will get

- **The server knows the save.** When a program saves, or sees the game save itself where the
  plan expects it, it records the place: a checkpoint's start, or a named place for RNG
  programs. `console://B/save` then shows `known`.
- **PLZA rewinds and says where it landed.** After an autosave, the program loads the backup
  (title, Up+B+X), reads the backup card and maps it to a checkpoint start. If the card matches
  nothing, the save stays `stale` and the run stops.
- **No save is a state too.** After you delete the save (or `delete_save.sh` does), the server
  records `no_save`, and a new-game run (PLZA 01.00a) checks for exactly that.
- **A hand save can be confirmed.** If you (or an agent) save by hand, the save goes `stale`; a
  program then opens the save screen, reads where and when it was saved, and records it again.
- **It notices when that stops being true.** Anything that could save without telling the
  server makes the save `stale`, with the reason: you playing (Start/Stop or the hold), keyboard
  input in the app, a controller batch, a program that does not report saves, an autosave the
  plan did not expect, the app restarting. Only a new recorded save makes it `known` again.
- **Moving is not saving.** A program run that walks on and fails leaves the save `known`; the
  server just notes the game has `moved`. FRLG's League is one long checkpoint with no save
  until the Hall of Fame, so a lost League run can be rerun from its start.
- **Runs check it first.** Before an agent's Start, the server's [preflight](actions.md) refuses
  a run whose start checkpoint is not the known save. It presses nothing for this and
  compares the server's record only; your own Start in the app is not checked.
- **Run records you can read.** Each run ends as one record: passed, failed or stopped and by
  whom, the save it started from, the saves it wrote, checkpoint results and the failure frame.

## Examples (samples, not output)

After FRLG AutoStory saved at the start of 18.02, ask *"which save is on console B?"*:

```json
{"status": "known", "reason": "program_save", "game": "at_save",
 "fact": {"game": "PokemonFRLG", "checkpoint": "18.02", "saved_at": "2026-11-15T23:44:13.115Z"}}
```

After you play a bit by hand:

```json
{"status": "stale", "reason": "user_lease", "since": "2026-11-15T23:50:02.000Z",
 "fact": {"checkpoint": "18.02", "saved_at": "2026-11-15T23:44:13.115Z"}}
```

An agent asking to run 18.02 now gets
`{"refusal": {"code": "save_mismatch", "message": "The save on console B is stale: user_lease since 23:50. Save at 18.02's start, or let a program confirm it."}}`.
To fix it, save at 18.02's start yourself and let the program's start confirm it, or have a
program rerun the checkpoint before it, which saves.

PLZA 03.08 after a catch autosave, once the program rewound to the backup:

```json
{"status": "known", "reason": "backup_load", "game": "at_save",
 "fact": {"game": "PokemonLZA", "checkpoint": "03.08", "registered": 4, "play_time": "1:12"}}
```

If the autosave detector fires where the plan expects no autosave (for example on the green zone
walls), you see `"status": "stale", "reason": "unexpected_autosave"`: the next run loads the
backup instead of trusting the newest save.

## Run records

Designed, for after the app gives each run an id. Ask *"how did the last run go?"* and get
one record: the program and options, the save it started from, start and end times, how it ended
(the program, an error, a lab script, or you), the `[Checkpoint]` lines, the saves it wrote and the
failure frame. Sketch, values made up:

```json
{"run_id": "3c1e...", "panel": "PokemonFRLG:AutoStory", "started_at": "2026-11-15T23:44:00.000Z",
 "ended": {"source": "ui", "at": "2026-11-15T23:49:58.000Z"},
 "save_at_start": {"status": "known", "checkpoint": "18.02"},
 "checkpoints": ["18.02: failed on attempt 1"],
 "failure_frame": ".../Screenshots/AutoStory/failures/....png"}
```

## What you do

- Today: before a checkpoint run, check yourself which save is on the console.
- Later: after playing by hand, save at the checkpoint's start and let a program confirm it, or
  let the agent rerun the checkpoint before (which saves). Your own Start is never refused for
  the save.
