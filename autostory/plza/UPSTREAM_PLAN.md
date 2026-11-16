# PLZA AutoStory: upstream plan (lean slices)

Updated by: the main session, 2026-11-15. It updates this file whenever a slice PR is opened,
merged or passes on console. Detail notes: `~/BDSPLab/notes/plza-slice1-plan.md` (local).

## Your TODOs

You open every upstream PR and write its description; agents never do.

1. **Send [#572](https://github.com/dayfine/Arduino-Source/pull/572) upstream when a slot frees.** You approved it ("good enough for 572", 2026-11-15;
   review rounds 1 and 2 done, tip 0758e5c24). We are at 3/3 open upstream:
   [PokemonAutomation/Arduino-Source#1845](https://github.com/PokemonAutomation/Arduino-Source/pull/1845), [#1881](https://github.com/PokemonAutomation/Arduino-Source/pull/1881), [#1916](https://github.com/PokemonAutomation/Arduino-Source/pull/1916). When one closes, an agent folds [#572](https://github.com/dayfine/Arduino-Source/pull/572) into
   one commit on upstream `main` and you open the PR and write its description.
2. **Review the rest of the chain** bottom first: [#573](https://github.com/dayfine/Arduino-Source/pull/573) to [#577](https://github.com/dayfine/Arduino-Source/pull/577) (slice 1), then [#580](https://github.com/dayfine/Arduino-Source/pull/580), [#582](https://github.com/dayfine/Arduino-Source/pull/582),
   [#591](https://github.com/dayfine/Arduino-Source/pull/591), [#595](https://github.com/dayfine/Arduino-Source/pull/595), [#596](https://github.com/dayfine/Arduino-Source/pull/596), [#598](https://github.com/dayfine/Arduino-Source/pull/598), [#604](https://github.com/dayfine/Arduino-Source/pull/604).
3. **Hand upstream, one at a time, under the three-open limit:** [#573](https://github.com/dayfine/Arduino-Source/pull/573) up the chain after [#572](https://github.com/dayfine/Arduino-Source/pull/572).
   Before each one, ask for the pre-flight: an agent merges `main` in, fixes every code-health
   finding (code-health `--base-ref origin/upstream-main`, see below), gets the unit tests green
   and reports the tested SHA.
4. **Land the test frames upstream before [#575](https://github.com/dayfine/Arduino-Source/pull/575).** [dayfine/CommandLineTests#18](https://github.com/dayfine/CommandLineTests/pull/18) (`plza/all-frames`)
   holds every frame the PLZA tests read; open its PR on PokemonAutomation/CommandLineTests. Nine
   AutoStoryScreenEvents frames are missing and need a recapture on console B (ours): `za2_m3o_s1`,
   `za2_m3p_s1`, `za2_m2q_s1`, `za2_hud_s3`, `za2_m2n_s1`, `za2_hud_s1`, `za2_m3s_s1`,
   `za2_m3w_s2`, `za2_m2m_s1`. Also still fork-only and needed upstream: the tutorial card, the
   objective marker with distance digits, and the PartyBarDetector frames.
5. **Close the old PRs** listed at the bottom, when you're ready.

No open questions right now.

## Decisions (2026-11-15)

- A checkpoint's save field is `autosave` | `unverified` | `absent`.
- [#520](https://github.com/dayfine/Arduino-Source/pull/520)'s `CheckpointId` is dropped from the chain.
- [#572](https://github.com/dayfine/Arduino-Source/pull/572) `StoryRangeOptions` is its own early PR, shared with SV.
- Rebuild as lean vertical slices: cut unused code, add a piece when it is first needed (battle at
  the first battle, catch at the first catch), and put shared code in shared locations with more
  tests.
- `plza/autostory` ([#576](https://github.com/dayfine/Arduino-Source/pull/576)) is the long-lived branch and the full-story reference: it always
  builds and should end up equal to `main`.
- Sharing with upstream PLZA programs:
  - Slice 4 has no fly (you can't fly at that story point); `fly_from_map` is dropped.
  - The gate walk tries upstream's `run_towards_gate_with_A_button` first; our gate-reader steering
    stays only if a console run shows the need.
  - Leaving a zone uses upstream's `leave_zone_gate`.
  - A shared throw helper (lock on, optional ball scroll, throw), used by our catch,
    InPlaceCatcher and PostKillCatcher, is its own PR ([#590](https://github.com/dayfine/Arduino-Source/pull/590)); slice 5 merges it in.
- The 01.00 save is `autosave` (a run log showed the badge).
- Fork branch `upstream-main` mirrors upstream `main`. It is the code-health PR base for
  upstream-bound branches: fork `main` has 58 fork-only commits, so diffing against it reports
  unrelated findings.
- Fix style throughout before upstream, the PLZA chain first (you, 2026-11-15). See
  [Style pass](#style-pass-2026-11-15).

## Chain

Fork drafts on dayfine/Arduino-Source, linear, bottom first. The chain now covers [#576](https://github.com/dayfine/Arduino-Source/pull/576)'s whole
story, 01.00 to 03.11. Lines added, from the PR file list (test = unit tests and fakes). CI: Fork
Draft CI only runs on PRs into `main`, so the slice PRs get a dispatched Ubuntu run at their tip.

| PR | Branch | Base | Tip (2026-11-15) | Purpose | Code / test (+lines) | Console |
|---|---|---|---|---|---|---|
| [#572](https://github.com/dayfine/Arduino-Source/pull/572) | `autostory/story-range-options-sv` | `main` | 0758e5c24 | start/end checkpoint range options, shared with SV | +167 / +267 | approved, waits for an upstream slot |
| [#573](https://github.com/dayfine/Arduino-Source/pull/573) | `plza/slice1/b` | [#572](https://github.com/dayfine/Arduino-Source/pull/572) | 02b01faf6 | story checkpoint data and its JSON reader | +1880 / +1050 | - |
| [#574](https://github.com/dayfine/Arduino-Source/pull/574) | `plza/slice1/c` | [#573](https://github.com/dayfine/Arduino-Source/pull/573) | 7e45a127a | story engine for a zone game | +1674 / +1164 | - |
| [#575](https://github.com/dayfine/Arduino-Source/pull/575) | `plza/slice1/d` | [#574](https://github.com/dayfine/Arduino-Source/pull/574) | ead8d7133 | PLZA opening detectors; CI pins [CLT #18](https://github.com/dayfine/CommandLineTests/pull/18) | +1166 / +997 | - |
| [#577](https://github.com/dayfine/Arduino-Source/pull/577) | `plza/slice1/e` | [#575](https://github.com/dayfine/Arduino-Source/pull/575) | 84c21cfdc | new game to leaving the platform (01.00, 01.01 start) | +1623 / +371 | 01.00 and 01.01 passed |
| [#580](https://github.com/dayfine/Arduino-Source/pull/580) | `plza/slice2/a` | [#577](https://github.com/dayfine/Arduino-Source/pull/577) | 85eabc107 | 01.01 arrival scenes to Main Mission 01 | +1132 / +881 | - |
| [#582](https://github.com/dayfine/Arduino-Source/pull/582) | `plza/slice2/b` | [#580](https://github.com/dayfine/Arduino-Source/pull/580) | 5f80b7e59 | 01.02, Pancham's chase to the battle area | +553 / +684 | - |
| [#591](https://github.com/dayfine/Arduino-Source/pull/591) | `plza/slice2/c` | [#582](https://github.com/dayfine/Arduino-Source/pull/582) | a6863623e | slice 2c: 01.03, the starter and the first battle | +452 / +244 | - |
| [#595](https://github.com/dayfine/Arduino-Source/pull/595) | `plza/slice3` | [#591](https://github.com/dayfine/Arduino-Source/pull/591) | 43dbc2e6d | slice 3: Main Mission 02 (02.01-02.07) | +644 / +394 | - |
| [#596](https://github.com/dayfine/Arduino-Source/pull/596) | `plza/slice4` | [#595](https://github.com/dayfine/Arduino-Source/pull/595) | 34c5d1433 | slice 4: 03.01-03.06, Hotel Z to Wild Zone 1's gate | +625 / +52 | - |
| [#598](https://github.com/dayfine/Arduino-Source/pull/598) | `plza/slice5` | [#596](https://github.com/dayfine/Arduino-Source/pull/596) | cb6d287a6 | slice 5: 03.07, 03.09-03.11, tutorial catches to Mable's reward; merges [#590](https://github.com/dayfine/Arduino-Source/pull/590) | +1233 / +445 | - |
| [#604](https://github.com/dayfine/Arduino-Source/pull/604) | `plza/slice5b` | [#598](https://github.com/dayfine/Arduino-Source/pull/598) | 3e8d835bc | slice 5b: 03.08 catches + 03.08b far-gate exit (split at the catch autosave) | +923 / +247 | - |
| [#590](https://github.com/dayfine/Arduino-Source/pull/590) | `plza/throw-helper` | `upstream-main` | 2394ee31b | shared ball throw helper for InPlaceCatcher, PostKillCatcher and our catch | +145 / - | - |
| [#576](https://github.com/dayfine/Arduino-Source/pull/576) | `plza/autostory` | `main` | 2f7b152b8 | long-lived full-story reference (+55k) | - | - |
| [CLT #18](https://github.com/dayfine/CommandLineTests/pull/18) | `plza/all-frames` | `main` | - | every frame the PLZA tests read (84 files) | - | 9 frames missing |

CI status:

- Ubuntu green on [#598](https://github.com/dayfine/Arduino-Source/pull/598) (12094fd94) and [#604](https://github.com/dayfine/Arduino-Source/pull/604) (3e8d835bc) after the style and test passes.
- A one-line label fix is being merged up the chain from [#577](https://github.com/dayfine/Arduino-Source/pull/577) (84c21cfdc): the GroupOption
  label "Checkpoints" held HTML that was shown literally.

Saves per checkpoint in slices 5 and 5b: 03.07 `autosave`, 03.08 `autosave`, 03.08b `absent`,
03.09 `unverified`, 03.10 `autosave`, 03.11 `unverified`.

## Style pass (2026-11-15)

Done on the chain:

- Style: parameter comments (including the pbf timings), `string_concat`, `out_param_order`,
  header helpers moved into anonymous namespaces in the `.cpp`, `StoryTimedLeg` duration as
  `Milliseconds`.
- Tests: scene-machine helpers made private; 22 tests rewritten to go through
  `run_scene_machine` / the screen machine; 15 vacuous tests deleted; allow-tags on the test fakes
  after the code-health fix [#602](https://github.com/dayfine/Arduino-Source/pull/602).

Remaining: the `test_only_api` advisories left are names that later slices or [#576](https://github.com/dayfine/Arduino-Source/pull/576) call.

## Cut from slices 5 and 5b (still in [#576](https://github.com/dayfine/Arduino-Source/pull/576))

- Lock-on target choice by name plate.
- Bracket check: `BracketWatch`, `LockOnBracketDetector`.
- `CatchPlan` / `CatchStop` throw-again.
- `SpeciesCheck`, `DexRegisteredDetector`.
- `ZoneGateReader` pillar steering: the 03.08b gate run is heading-only and untested on console.
- `fly_to_center_or_stay`, `check_out_of_wild_zone`, menu saves, `face_objective`.

## Console

- 2026-11-15, lab build e36a816a4 (slice 1 E + lab tooling): 01.00 passed on a new game (autosave
  badge seen); 01.01 3/3, runs 2 and 3 reloaded the latest save and walked to the plaza fade.
- Next: one full-chain run, 01.00 to 03.11, on a fresh game (you asked to delete the save and start
  fresh).

## Side PRs

- Fork [#585](https://github.com/dayfine/Arduino-Source/pull/585) (StringSelect dropdown text was black in dark mode; found while testing [#572](https://github.com/dayfine/Arduino-Source/pull/572)'s
  dropdowns) went upstream as [PokemonAutomation/Arduino-Source#1916](https://github.com/PokemonAutomation/Arduino-Source/pull/1916): one commit f8ed1ceae on
  branch `upstream/string-select-text-color`. You tested it on console B's lab app.

## Later slices

Slices 2c to 5b are now in the chain above. Still to come: stats ([#519](https://github.com/dayfine/Arduino-Source/pull/519)), captures, fault sweep.

## Old PRs to close (your call)

Still open on 2026-11-15:

- Old shared chain S1/S2: [#529](https://github.com/dayfine/Arduino-Source/pull/529)-[#543](https://github.com/dayfine/Arduino-Source/pull/543) ([#544](https://github.com/dayfine/Arduino-Source/pull/544) and [#545](https://github.com/dayfine/Arduino-Source/pull/545) are already closed).
- Old PLZA chain: [#555](https://github.com/dayfine/Arduino-Source/pull/555)-[#569](https://github.com/dayfine/Arduino-Source/pull/569).
- Umbrella drafts: [#522](https://github.com/dayfine/Arduino-Source/pull/522), [#524](https://github.com/dayfine/Arduino-Source/pull/524), [#525](https://github.com/dayfine/Arduino-Source/pull/525).
- Older PLZA PRs: [#335](https://github.com/dayfine/Arduino-Source/pull/335), [#354](https://github.com/dayfine/Arduino-Source/pull/354), [#355](https://github.com/dayfine/Arduino-Source/pull/355), [#356](https://github.com/dayfine/Arduino-Source/pull/356), [#416](https://github.com/dayfine/Arduino-Source/pull/416).

Also open, kept for now: [#492](https://github.com/dayfine/Arduino-Source/pull/492) (fault sweep, until the fault-sweep slice replaces it) and [#418](https://github.com/dayfine/Arduino-Source/pull/418)
(CFR actions, stays on the fork).
