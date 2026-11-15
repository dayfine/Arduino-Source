# PLZA AutoStory: upstream plan (lean slices)

Updated by: the main session, 2026-11-15. It updates this file whenever a slice PR is opened,
merged or passes on console. Detail notes: `~/BDSPLab/notes/plza-slice1-plan.md` (local).

## Your TODOs

You open every upstream PR and write its description; agents never do.

1. **Review the slice PRs** bottom first: #572, then #573 to #577 (slice 1), then #580, #582.
2. **Hand upstream, one at a time, under the three-open limit:** #572 first (shared with SV), then
   #573 up the chain. Before each one, ask for the pre-flight: an agent merges `main` in, fixes
   every code-health finding, gets the unit tests green and reports the tested SHA.
3. **Land the test frames upstream before #575.** dayfine/CommandLineTests#18 (`plza/all-frames`)
   holds every frame the PLZA tests read; open its PR on PokemonAutomation/CommandLineTests. Nine
   AutoStoryScreenEvents frames are missing and need a recapture on console B (ours): `za2_m3o_s1`,
   `za2_m3p_s1`, `za2_m2q_s1`, `za2_hud_s3`, `za2_m2n_s1`, `za2_hud_s1`, `za2_m3s_s1`,
   `za2_m3w_s2`, `za2_m2m_s1`.
4. **Close the old PRs** listed at the bottom, when you're ready.

No open questions right now.

## Decisions (2026-11-15)

- A checkpoint's save field is `autosave` | `unverified` | `absent`.
- #520's `CheckpointId` is dropped from the chain.
- #572 `StoryRangeOptions` is its own early PR, shared with SV.
- Rebuild as lean vertical slices: cut unused code, add a piece when it is first needed (battle at
  the first battle, catch at the first catch), and put shared code in shared locations with more
  tests.
- `plza/autostory` (#576) is the long-lived branch: it always builds and should end up equal to
  `main`.
- Sharing with upstream PLZA programs:
  - Slice 4 has no fly (you can't fly at that story point); `fly_from_map` is dropped.
  - The gate walk tries upstream's `run_towards_gate_with_A_button` first; our gate-reader steering
    stays only if a console run shows the need.
  - Leaving a zone uses upstream's `leave_zone_gate`.
  - A shared throw helper (lock on, optional ball scroll, throw), used by our catch,
    InPlaceCatcher and PostKillCatcher, goes in its own PR before slice 5.
- The 01.00 save is `autosave` (a run log showed the badge).

## Chain

Fork drafts on dayfine/Arduino-Source, linear, bottom first. Lines added, from the PR file list
(test = unit tests and fakes). CI: Fork Draft CI only runs on PRs into `main`, so the slice PRs
got a dispatched Ubuntu run at their tip; failures are named.

| PR | Branch | Base | Purpose | Code / test (+lines) | CI (2026-11-15) | Console |
|---|---|---|---|---|---|---|
| #572 | `autostory/story-range-options-sv` | `main` | start/end checkpoint range options, shared with SV | +168 / +261 | rerunning at 7078725ae | - |
| #573 | `plza/slice1/b` | #572 | story checkpoint data and its JSON reader | +1882 / +1071 | 1 unit test fails (StoryFiles: trailing space) | - |
| #574 | `plza/slice1/c` | #573 | story engine for a zone game | +1671 / +1190 | 4 fail (StoryFiles; StoryEngine expects `Home`, gets `HOME`) | - |
| #575 | `plza/slice1/d` | #574 | PLZA opening detectors; CI pins CLT #18 | +1167 / +997 | same 4 | - |
| #577 | `plza/slice1/e` | #575 | new game to leaving the platform (01.00, 01.01 start) | +1627 / +371 | same 4 | 01.00 1/1, 01.01 3/3 (below) |
| #580 | `plza/slice2/a` | #577 | 01.01 arrival scenes to Main Mission 01 | +1141 / +984 | not run | - |
| #582 | `plza/slice2/b` | #580 | 01.02, Pancham's chase to the battle area | +554 / +690 | 10 fail (same 4 + 6) | - |
| #576 | `plza/autostory` | `main` | long-lived branch (+54k, 425 files) | - | Fork Draft CI fails on the 9 missing frames; code-health fails | - |
| CLT #18 | `plza/all-frames` | `main` | every frame the PLZA tests read (84 files) | - | - | 9 frames missing |

Console records, lab build e36a816a4 (slice 1 E + lab tooling), 2026-11-15:

- 01.00 passed once: new game to the platform, autosave badge seen.
- 01.01 3/3; runs 2 and 3 reloaded the latest save and walked to the plaza fade.

## Later slices

| Slice | Checkpoints | Adds |
|---|---|---|
| 2c | 01.03 | starter, TALK, first battle, MissionComplete |
| 3 | Main Mission 02 | |
| 4 | 03.01-03.06 | map, ladder, gate (no fly) |
| throw helper | - | shared PLZA throw helper (own PR) |
| 5 | 03.07-03.11 | catches and objective marker walks |
| later | - | stats (#519), captures, fault sweep |

## Old PRs to close (your call)

Still open on 2026-11-15:

- Old shared chain S1/S2: #529-#543 (#544 and #545 are already closed).
- Old PLZA chain: #555-#569.
- Umbrella drafts: #522, #524, #525.
- Older PLZA PRs: #335, #354, #355, #356, #416.

Also open, kept for now: #492 (fault sweep, until the fault-sweep slice replaces it) and #418
(CFR actions, stays on the fork).
