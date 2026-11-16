> **DRAFT.** This page describes a feature that is not built yet. It is updated as it lands.
> Status (2026-11-15): planned (Slice 6). `console://B/save` exists and usually says `unknown`.

# Save facts: knowing which save is on the console

Most lab mistakes that cost real progress are about the save: starting a checkpoint run from
the wrong save, or a hand session moving the player after the last save.

## What you will get

- **The server knows the save.** When a program saves (AutoStory and RNG programs first), it
  records where and when. `console://B/save` then shows `known`, with the checkpoint or place.
- **It notices when that stops being true.** Anything that can change the game without a
  recorded save (you playing, a controller batch, an unhooked program) marks the save
  `stale`, with the reason. Only another recorded save makes it `known` again.
- **Runs check it first.** The `save_at(place)` [start check](start-checks.md) refuses a run
  whose start does not match the known save.
- **Run records you can read.** Each run ends as one record (passed, failed, or stopped and by
  whom), with checkpoint results, RNG hits and links to failure screenshots, shown through the
  server's resources and events.

## Example (sketch, not built)

Read `console://B/save` after an AutoStory checkpoint saved:

```json
{"state": "known", "fact": {"program": "PokemonFRLG:AutoStory", "checkpoint": "18.02",
 "saved_at": "2026-11-15T23:44:13.115Z"}}
```

After you play by hand or a controller batch runs:

```json
{"state": "stale", "reason": "user input after the last recorded save (2026-11-15T23:50:02Z)",
 "fact": {"checkpoint": "18.02", "saved_at": "2026-11-15T23:44:13.115Z"}}
```

A run whose start does not match then gets
`{"refusal": {"code": "precheck_failed", "check": "save_at", "message": "The save on console B is stale ..."}}`.
