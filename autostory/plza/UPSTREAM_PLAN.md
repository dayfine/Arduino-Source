# PLZA AutoStory: upstream plan (lean slices)

Updated by: the main session, 2026-11-15. It updates this file whenever a slice PR is opened,
merged or passes on console. Detail notes: `~/BDSPLab/notes/plza-slice1-plan.md` (local).

## Your TODOs

You open every upstream PR and write its description; agents never do.

1. **Send [#572](https://github.com/dayfine/Arduino-Source/pull/572) upstream when a slot frees.** You approved it ("good enough for 572", 2026-11-15;
   review rounds 1 and 2 done, tip 0758e5c24). We are at 3/3 open upstream:
   [PokemonAutomation/Arduino-Source#1845](https://github.com/PokemonAutomation/Arduino-Source/pull/1845), [#1881](https://github.com/PokemonAutomation/Arduino-Source/pull/1881), [#1916](https://github.com/PokemonAutomation/Arduino-Source/pull/1916). When one closes, an agent folds [#572](https://github.com/dayfine/Arduino-Source/pull/572) into
   one commit on upstream `main` and you open the PR and write its description.
2. **Review the rest of the chain** bottom first: [#573](https://github.com/dayfine/Arduino-Source/pull/573) to [#577](https://github.com/dayfine/Arduino-Source/pull/577) (slice 1), then [#580](https://github.com/dayfine/Arduino-Source/pull/580), [#582](https://github.com/dayfine/Arduino-Source/pull/582).
3. **Hand upstream, one at a time, under the three-open limit:** [#573](https://github.com/dayfine/Arduino-Source/pull/573) up the chain after [#572](https://github.com/dayfine/Arduino-Source/pull/572).
   Before each one, ask for the pre-flight: an agent merges `main` in, fixes every code-health
   finding (code-health `--base-ref origin/upstream-main`, see below), gets the unit tests green
   and reports the tested SHA.
4. **Land the test frames upstream before [#575](https://github.com/dayfine/Arduino-Source/pull/575).** [dayfine/CommandLineTests#18](https://github.com/dayfine/CommandLineTests/pull/18) (`plza/all-frames`)
   holds every frame the PLZA tests read; open its PR on PokemonAutomation/CommandLineTests. Nine
   AutoStoryScreenEvents frames are missing and need a recapture on console B (ours): `za2_m3o_s1`,
   `za2_m3p_s1`, `za2_m2q_s1`, `za2_hud_s3`, `za2_m2n_s1`, `za2_hud_s1`, `za2_m3s_s1`,
   `za2_m3w_s2`, `za2_m2m_s1`.
5. **Close the old PRs** listed at the bottom, when you're ready.

No open questions right now.

## Decisions (2026-11-15)

- A checkpoint's save field is `autosave` | `unverified` | `absent`.
- [#520](https://github.com/dayfine/Arduino-Source/pull/520)'s `CheckpointId` is dropped from the chain.
- [#572](https://github.com/dayfine/Arduino-Source/pull/572) `StoryRangeOptions` is its own early PR, shared with SV.
- Rebuild as lean vertical slices: cut unused code, add a piece when it is first needed (battle at
  the first battle, catch at the first catch), and put shared code in shared locations with more
  tests.
- `plza/autostory` ([#576](https://github.com/dayfine/Arduino-Source/pull/576)) is the long-lived branch: it always builds and should end up equal to
  `main`.
- Sharing with upstream PLZA programs:
  - Slice 4 has no fly (you can't fly at that story point); `fly_from_map` is dropped.
  - The gate walk tries upstream's `run_towards_gate_with_A_button` first; our gate-reader steering
    stays only if a console run shows the need.
  - Leaving a zone uses upstream's `leave_zone_gate`.
  - A shared throw helper (lock on, optional ball scroll, throw), used by our catch,
    InPlaceCatcher and PostKillCatcher, goes in its own PR before slice 5.
- The 01.00 save is `autosave` (a run log showed the badge).
- Fork branch `upstream-main` mirrors upstream `main`. It is the code-health PR base for
  upstream-bound branches: fork `main` has 58 fork-only commits, so diffing against it reports
  unrelated findings.

## Chain

Fork drafts on dayfine/Arduino-Source, linear, bottom first. Lines added, from the PR file list
(test = unit tests and fakes). CI: Fork Draft CI only runs on PRs into `main`, so the slice PRs
got a dispatched Ubuntu run at their tip; failures are named. The CI column is from the earlier
tips; [#572](https://github.com/dayfine/Arduino-Source/pull/572)'s review round 2 was merged up the whole chain on 2026-11-15, giving the tips below,
which have not been rerun yet.

| PR | Branch | Base | Tip (2026-11-15) | Purpose | Code / test (+lines) | CI (earlier tip) | Console |
|---|---|---|---|---|---|---|---|
| [#572](https://github.com/dayfine/Arduino-Source/pull/572) | `autostory/story-range-options-sv` | `main` | 0758e5c24 | start/end checkpoint range options, shared with SV | +168 / +261 | review rounds 1+2 done; approved, waits for an upstream slot | - |
| [#573](https://github.com/dayfine/Arduino-Source/pull/573) | `plza/slice1/b` | [#572](https://github.com/dayfine/Arduino-Source/pull/572) | 015ade1a1 | story checkpoint data and its JSON reader | +1882 / +1071 | 1 unit test fails (StoryFiles: trailing space) | - |
| [#574](https://github.com/dayfine/Arduino-Source/pull/574) | `plza/slice1/c` | [#573](https://github.com/dayfine/Arduino-Source/pull/573) | 8e3f39126 | story engine for a zone game | +1671 / +1190 | 4 fail (StoryFiles; StoryEngine expects `Home`, gets `HOME`) | - |
| [#575](https://github.com/dayfine/Arduino-Source/pull/575) | `plza/slice1/d` | [#574](https://github.com/dayfine/Arduino-Source/pull/574) | 3db337f78 | PLZA opening detectors; CI pins [CLT #18](https://github.com/dayfine/CommandLineTests/pull/18) | +1167 / +997 | same 4 | - |
| [#577](https://github.com/dayfine/Arduino-Source/pull/577) | `plza/slice1/e` | [#575](https://github.com/dayfine/Arduino-Source/pull/575) | a2c0d34a1 | new game to leaving the platform (01.00, 01.01 start) | +1627 / +371 | same 4 | 01.00 passed, 01.01 3/3 (below) |
| [#580](https://github.com/dayfine/Arduino-Source/pull/580) | `plza/slice2/a` | [#577](https://github.com/dayfine/Arduino-Source/pull/577) | dff98199b | 01.01 arrival scenes to Main Mission 01 | +1141 / +984 | not run | - |
| [#582](https://github.com/dayfine/Arduino-Source/pull/582) | `plza/slice2/b` | [#580](https://github.com/dayfine/Arduino-Source/pull/580) | 1d54c5d74 | 01.02, Pancham's chase to the battle area | +554 / +690 | 10 fail (same 4 + 6) | - |
| [#576](https://github.com/dayfine/Arduino-Source/pull/576) | `plza/autostory` | `main` | 2f7b152b8 | long-lived branch (+54k, 425 files) | - | Fork Draft CI fails on the 9 missing frames; code-health fails | - |
| [CLT #18](https://github.com/dayfine/CommandLineTests/pull/18) | `plza/all-frames` | `main` | - | every frame the PLZA tests read (84 files) | - | - | 9 frames missing |

Console records, lab build e36a816a4 (slice 1 E + lab tooling), 2026-11-15:

- 01.00 passed once: new game to the platform, autosave badge seen.
- 01.01 3/3; runs 2 and 3 reloaded the latest save and walked to the plaza fade.

## Side PRs

- Fork [#585](https://github.com/dayfine/Arduino-Source/pull/585) (StringSelect dropdown text was black in dark mode; found while testing [#572](https://github.com/dayfine/Arduino-Source/pull/572)'s
  dropdowns) went upstream as [PokemonAutomation/Arduino-Source#1916](https://github.com/PokemonAutomation/Arduino-Source/pull/1916): one commit f8ed1ceae on
  branch `upstream/string-select-text-color`. You tested it on console B's lab app.

## Later slices

| Slice | Checkpoints | Adds |
|---|---|---|
| 2c | 01.03 | starter, TALK, first battle, MissionComplete |
| 3 | Main Mission 02 | |
| 4 | 03.01-03.06 | map, ladder, gate (no fly) |
| throw helper | - | shared PLZA throw helper (own PR) |
| 5 | 03.07-03.11 | catches and objective marker walks |
| later | - | stats ([#519](https://github.com/dayfine/Arduino-Source/pull/519)), captures, fault sweep |

## Old PRs to close (your call)

Still open on 2026-11-15:

- Old shared chain S1/S2: [#529](https://github.com/dayfine/Arduino-Source/pull/529)-[#543](https://github.com/dayfine/Arduino-Source/pull/543) ([#544](https://github.com/dayfine/Arduino-Source/pull/544) and [#545](https://github.com/dayfine/Arduino-Source/pull/545) are already closed).
- Old PLZA chain: [#555](https://github.com/dayfine/Arduino-Source/pull/555)-[#569](https://github.com/dayfine/Arduino-Source/pull/569).
- Umbrella drafts: [#522](https://github.com/dayfine/Arduino-Source/pull/522), [#524](https://github.com/dayfine/Arduino-Source/pull/524), [#525](https://github.com/dayfine/Arduino-Source/pull/525).
- Older PLZA PRs: [#335](https://github.com/dayfine/Arduino-Source/pull/335), [#354](https://github.com/dayfine/Arduino-Source/pull/354), [#355](https://github.com/dayfine/Arduino-Source/pull/355), [#356](https://github.com/dayfine/Arduino-Source/pull/356), [#416](https://github.com/dayfine/Arduino-Source/pull/416).

Also open, kept for now: [#492](https://github.com/dayfine/Arduino-Source/pull/492) (fault sweep, until the fault-sweep slice replaces it) and [#418](https://github.com/dayfine/Arduino-Source/pull/418)
(CFR actions, stays on the fork).
