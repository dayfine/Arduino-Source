> **DRAFT.** This page describes a feature that is only partly built. It is updated as each piece lands.
> Status (2026-11-15): the check list and `describe_program` are built ([#485](https://github.com/dayfine/Arduino-Source/pull/485), [#511](https://github.com/dayfine/Arduino-Source/pull/511), [#512](https://github.com/dayfine/Arduino-Source/pull/512)); FRLG AutoStory's list is a design note ([#488](https://github.com/dayfine/Arduino-Source/pull/488)); the server does not yet run the checks before a start (that is [actions.md](actions.md), Slice 5b).

# Start checks: what a program needs before it presses anything

Every program can declare a short, named list of things that must be true before it starts:
the game is in front, the screen has no black border, a party slot is free, the save is the
one the program expects. Each check has a name and a one-line "needs" sentence, and none of
them presses a button, resets, saves or changes an option.

## What you get

- **Read what a program needs without running it.** Ask for `describe_program` on a panel
  and you get its options (keys, types, defaults, allowed values) and its start checks with
  their "needs" lines.
- **One list guards both doors.** The same list runs when you click Start in the app and when
  an agent asks the server to start a run, so a bad setup is refused the same way either way,
  before any reset.
- **Clear refusals.** The first failing check stops the start with a message saying which
  check failed and what it needs.

## Checks you will see

| Check | Needs |
|---|---|
| `game_open` | the program's game is in front, not HOME, user select or a black screen |
| `black_border` | the picture fills the screen (no black border) |
| `free_party_slots(n)` | at least n empty party slots (FRLG RNG gift/egg) |
| `save_at(place)` | the save on the console is the one this run starts from (needs [saves.md](saves.md)) |

Program-specific checks sit beside the program: for example a Sweet Scent user in the last
slot, or enough Safari advances. A program without a list says `unsupported`; that never
counts as passing.

## Planned lists

| Program | Checks |
|---|---|
| FRLG RNG | `black_border`, then `free_party_slots(1)` (gift, egg); Sweet Scent user last (wild); Safari advances ≥ 4000 |
| FRLG / BDSP / PLZA AutoStory | `game_open`; the start checkpoint is chosen and visible; `save_at(start checkpoint)`; LANGUAGE matches the game |
