# App channel, version 1

What the daemon needs from a SerialPrograms app, as files in the profile's `Lab/` folder. Only
`pa_lab/channel_file.py` reads or writes these; the rest of the daemon sees the typed values in
`pa_lab/channel.py`. The app's full command set is Lab.md (`SerialPrograms/Source/CommonFramework/Windows/Lab.md`);
this page is the part the daemon relies on.

## Layout

```
Lab/
  inbox/<name>.txt         daemon writes (rename in): a request
  done/<name>.txt          app moves the request here before it runs anything
  outbox/<name>.out        app writes (rename in) once the request finished: the journal
  outbox/<name>_<label>.png  a snapshot's frame
  outbox/<name>.part       daemon's scratch file, renamed into inbox/
  state.json               app writes (rename in): contracts/v1/app_state
  save.json                program writes after a hooked save: contracts/v1/save_fact
```

`<name>` is the daemon's: `palab_<tool>_<request id without dashes>[_<8 hex>]`. A run, stop or
other leased action adds the random part, so a journal left by an earlier attempt with the same
request id never answers a new one.

## Request

```
request 5f0c2a8e-3d41-4b7a-9c55-0e6a1f2b7c10
start 10
```

The first line is the daemon's request id (lower-case UUID); `state.json` names it in
`program.last_transition.request_id` for a start or stop. Then one command per line, run in order;
the request stops at its first failing line. The commands the daemon sends:

| Command | Line |
|---|---|
| Start, waiting up to 10 s for the controller | `start 10` |
| Stop | `stop` |
| Snapshot | `snapshot frame <timeout_ms>` |
| Describe program | `describe_program` |
| Game open | `game_open` |

## Journal

```
describe_program {"id":"PokemonFRLG:AutoStory","options":[],"prechecks":"unsupported","schema":1}
ok 2 describe_program @0-3ms
end
```

```
error 2 (start 10): Program is RUNNING.
end
```

- `ok <line> <command> @<start>-<end>ms`: that command ran. `<line>` counts the request's lines from 1,
  so the first command is line 2.
- `error <line> (<command>): <message>`: the first failure; nothing after it ran.
- `end`: always last. A journal without it is not finished (the app writes `.tmp` and renames).
- Any other line is a command's printed reply (`describe_program {...}`, `game_open yes`,
  `game_open no home`, `snapshot saved instance=... frame_seq=... captured_at=... after=... path=...`).

Error messages the daemon tells apart:

| Message | Meaning |
|---|---|
| `owned by <token>` | a script holds the owner lock; the message names its token, so it is never stored |
| `Program is <STATE>.` | the program is not in the state the command needs (`RUNNING`, `STOPPING`, ...) |
| contains `not a program` | the selected panel is not a program |
| contains `controller not connected` | `start` waited for the controller in vain |

Anything else is an app error the daemon reports with the journal as evidence.

## Ordering

1. The app takes a request by moving it from `inbox/` to `done/` before it runs anything. A request
   still in `inbox/` has not run, and deleting it withdraws it.
2. The finished journal is renamed into `outbox/` first. `state.json` is rewritten after it, on
   the app's next poll: up to 500 ms later, and later still while another request runs. So a
   journal is judged against `state.json` only once `written_at` is after the journal was read.
3. A request of `snapshot` lines only runs beside other requests; the rest run one at a time, in
   name order.

## Retention

Whoever cleans `Lab/` (the lab's `disk_gc.sh` keeps the newest 100 files of `done/` and `outbox/`)
may delete a request's `done/` file and journal once it is old. The daemon stores the first time it
sees a request taken or answered, so pruning never turns a taken request into one never sent.
