# Have an agent run a program

[Back to the guide](../GUIDE.md)

**Not yet.** The code and its unit tests are merged but have never been used end to end; the
server mode that serves it is not configured; the console-B soak has not been run; and no program
reports its start checks yet, so nothing can be started this way today. Today agents start and
stop runs with the lab scripts (`labctl.sh`). The examples below are samples, not console output.

## How to use it (once it is on)

1. Ask Claude in plain words: *"take console B and run FRLG AutoStory 13.01 to 13.02"*, or
   *"stop the run on B"*.
2. Claude takes the console for a while (a lease), asks the server to run the program with the
   options it wants, and reports what happened.
3. To take over, click Stop in the app. You always win: the agent's lease ends at once and its
   requests are refused until you run `hold.sh off`.

You do nothing to allow a run. Switching this on is your decision, one program family at a time.

## What happens when an agent starts a run

Two stages, split by whether they need the game.

**1. The server's preflight, before Start.** It presses nothing and checks only what it can see
from outside the game:

| Check | Refused with |
|---|---|
| the lab app is up and writing its state | `app_unavailable` |
| the selected panel is the program asked for, and every option is one the program takes, with an allowed value | `panel_not_program`, `invalid_arguments` |
| you don't hold the console, and the agent holds the lease | `console_held_by_user`, `console_held_by_agent` |
| no program is running | `program_running` |
| the program reports its start checks as a run result (below) | `prechecks_unsupported` |
| the controller is ready and the video is live | `app_unavailable` with the reason |
| the save on the console is the one the run starts from (once save records exist, see [saves.md](saves.md)) | `save_mismatch` |

A refusal means nothing was sent to the app. If every check passes, the server sets the options
and clicks Start in the same step; the panel keeps those options afterwards, as when you set them
yourself.

**2. The program's start checks, its first step after Start.** These need the game, so they may
press buttons: open the party to count free slots, look for a black border, check the game is on
the expected screen. They never reset, save or change an option. A failed check ends the run
before any reset, with a named result the server reports:

```json
{"outcome": "start_check_failed", "check": "free_party_slots",
 "message": "Free party slots: needs 1 empty slot, found 0.",
 "evidence": {"needed": 1, "found": 0}}
```

(Sample; the shape is the design, not output.) The same checks run when you click Start yourself,
so a bad setup fails the same way either way. Checks you will see:

| Check | Needs | Program |
|---|---|---|
| `black_border` | the picture fills the screen | FRLG RNG |
| `free_party_slots` | at least n empty party slots | FRLG RNG gift and egg |
| `game_screen` | the program's game is in front, on the screen it starts from | AutoStory |

A program's own setting to skip a check (FRLG RNG has one for free party slots) is for your own
Start; an agent cannot turn a check off.

## Stop a run

*"Stop the run on B."* The server clicks Stop once. Stopping when nothing runs is not an error.
Asking twice with the same request never stops twice.

## Debug batches

For debugging, an agent can send a short batch of allowed button presses and screenshots, only
while no program runs, at most 60 s. Starting programs, closing the game and the Capture button
are refused before anything is pressed. After a batch the save record goes stale, since the
presses may have changed the game.

## What it refuses, in one place

`console_held_by_user`, `console_held_by_agent`, `program_running`, `app_unavailable`,
`panel_not_program`, `invalid_arguments`, `prechecks_unsupported`, `save_mismatch`. Each comes with a sentence, for example
`console_held_by_user`: "The user clicked Stop in the app at 06:10; console B is theirs until they
release it."
