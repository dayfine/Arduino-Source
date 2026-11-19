> **DRAFT.** This page describes a feature that is not built yet. It is updated as it lands.
> Status (2026-11-18): planned (Slice 7) in the design doc (`docs/design/Agent-Integration.md` on branch `design/agent-integration`); not designed in [README.md](../README.md), no code. Today `labrun.sh` launches the lab app.

[Back to the guide](../GUIDE.md)

# The server runs the lab app

## What you will get

- **The right build, the right way, every time.** The server launches the lab app itself, with
  the lab profile, the audio settings and the build you picked, and refuses anything else
  (no more running a build without `--profile`).
- **Crash handling.** It notices a crash or a hung app, records it, and refuses to start a
  second copy. It only ever stops the app it started.
- **You keep an escape hatch.** You can still launch or quit the app yourself; the server sees
  that and stands back.
- **Agents stop launching apps directly.** After a soak, agents ask the server instead of
  running `labrun.sh` or writing to the app's inbox.

## Example (sketch, not built)

*"Start the lab app on the newest FRLG build."* The server answers with what it launched:

```json
{"pid": 13122, "game": "frlg", "commit": "72f365065", "profile": "lab-B",
 "deployment": "slot-B-frlg/72f3650657d2-8bf901e7"}
```

or refuses, e.g. `{"refusal": {"code": "app_running_program", "message": "PokemonFRLG:AutoStory is running; stop it first."}}`.
