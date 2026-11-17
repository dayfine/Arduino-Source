# PLZA AutoStory: upstream plan (lean slices)

Updated by: the main session, 2026-11-16. It updates this file whenever a slice PR is opened,
merged or passes on console. Detail notes: `~/BDSPLab/notes/plza-slice1-plan.md` (local).

## Your TODOs

You open every upstream PR and write its description; agents never do.

1. **[#572](https://github.com/dayfine/Arduino-Source/pull/572) is upstream** as [PokemonAutomation/Arduino-Source#1921](https://github.com/PokemonAutomation/Arduino-Source/pull/1921) (you opened it 2026-11-16, one commit 530b12c0a on
   `autostory/story-range-options-upstream`). We are at 3/3 open upstream (checked 2026-11-16):
   [PokemonAutomation/Arduino-Source#1845](https://github.com/PokemonAutomation/Arduino-Source/pull/1845), [#1881](https://github.com/PokemonAutomation/Arduino-Source/pull/1881), [#1921](https://github.com/PokemonAutomation/Arduino-Source/pull/1921). [#1916](https://github.com/PokemonAutomation/Arduino-Source/pull/1916) merged on 2026-11-16. Next up is [#573](https://github.com/dayfine/Arduino-Source/pull/573)
   when a slot frees (TODO 3).
2. **Review the rest of the chain** bottom first: [#573](https://github.com/dayfine/Arduino-Source/pull/573) to [#577](https://github.com/dayfine/Arduino-Source/pull/577) (slice 1), then [#580](https://github.com/dayfine/Arduino-Source/pull/580), [#582](https://github.com/dayfine/Arduino-Source/pull/582),
   [#591](https://github.com/dayfine/Arduino-Source/pull/591), [#595](https://github.com/dayfine/Arduino-Source/pull/595), [#596](https://github.com/dayfine/Arduino-Source/pull/596), [#598](https://github.com/dayfine/Arduino-Source/pull/598), [#604](https://github.com/dayfine/Arduino-Source/pull/604), [#610](https://github.com/dayfine/Arduino-Source/pull/610), [#619](https://github.com/dayfine/Arduino-Source/pull/619), [#620](https://github.com/dayfine/Arduino-Source/pull/620), [#622](https://github.com/dayfine/Arduino-Source/pull/622),
   [#623](https://github.com/dayfine/Arduino-Source/pull/623).
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

| PR | Branch | Base | Tip (2026-11-16 evening) | Purpose | Code / test (+lines) | Console |
|---|---|---|---|---|---|---|
| [#572](https://github.com/dayfine/Arduino-Source/pull/572) | `autostory/story-range-options-sv` | `main` | 953ed98a9 | start/end checkpoint range options, shared with SV | +167 / +267 | upstream as [PokemonAutomation/Arduino-Source#1921](https://github.com/PokemonAutomation/Arduino-Source/pull/1921) |
| [#573](https://github.com/dayfine/Arduino-Source/pull/573) | `plza/slice1/b` | [#572](https://github.com/dayfine/Arduino-Source/pull/572) | 380656d72 | story checkpoint data and its JSON reader | +1880 / +1050 | - |
| [#574](https://github.com/dayfine/Arduino-Source/pull/574) | `plza/slice1/c` | [#573](https://github.com/dayfine/Arduino-Source/pull/573) | b4cbb52ee | story engine for a zone game | +1674 / +1164 | - |
| [#575](https://github.com/dayfine/Arduino-Source/pull/575) | `plza/slice1/d` | [#574](https://github.com/dayfine/Arduino-Source/pull/574) | 23b0d7717 | PLZA opening detectors; CI pins [CLT #18](https://github.com/dayfine/CommandLineTests/pull/18) | +1166 / +997 | - |
| [#577](https://github.com/dayfine/Arduino-Source/pull/577) | `plza/slice1/e` | [#575](https://github.com/dayfine/Arduino-Source/pull/575) | 546eda1e0 | new game to leaving the platform (01.00, 01.01 start) | +1623 / +371 | 01.00 and 01.01 passed (11-15) |
| [#580](https://github.com/dayfine/Arduino-Source/pull/580) | `plza/slice2/a` | [#577](https://github.com/dayfine/Arduino-Source/pull/577) | 579cec66c | 01.01 arrival scenes to Main Mission 01 | +1132 / +881 | 01.01 passed (11-15) |
| [#582](https://github.com/dayfine/Arduino-Source/pull/582) | `plza/slice2/b` | [#580](https://github.com/dayfine/Arduino-Source/pull/580) | efd43d21d | 01.02, Pancham's chase to the battle area | +553 / +684 | 01.02 passed (11-15) |
| [#591](https://github.com/dayfine/Arduino-Source/pull/591) | `plza/slice2/c` | [#582](https://github.com/dayfine/Arduino-Source/pull/582) | ff225d59f | slice 2c: 01.03, the starter and the first battle | +452 / +244 | 01.03 passed (11-15) |
| [#595](https://github.com/dayfine/Arduino-Source/pull/595) | `plza/slice3` | [#591](https://github.com/dayfine/Arduino-Source/pull/591) | 21b4cd70f | slice 3: Main Mission 02 (02.01-02.07) | +644 / +394 | 02.01-02.03 passed (11-15) |
| [#596](https://github.com/dayfine/Arduino-Source/pull/596) | `plza/slice4` | [#595](https://github.com/dayfine/Arduino-Source/pull/595) | e37b81959 | slice 4: 03.01-03.06, Hotel Z to Wild Zone 1's gate | +625 / +52 | 03.01-03.03, 03.05, 03.06 passed (11-16); 03.04 did not |
| [#598](https://github.com/dayfine/Arduino-Source/pull/598) | `plza/slice5` | [#596](https://github.com/dayfine/Arduino-Source/pull/596) | 858d90cb3 | slice 5: 03.07, 03.09-03.11, tutorial catches to Mable's reward; merges [#590](https://github.com/dayfine/Arduino-Source/pull/590) | +1233 / +445 | 03.07 passed (11-16); 03.09-03.11 passed with [#623](https://github.com/dayfine/Arduino-Source/pull/623)'s port |
| [#604](https://github.com/dayfine/Arduino-Source/pull/604) | `plza/slice5b` | [#598](https://github.com/dayfine/Arduino-Source/pull/598) | bf0d1429b | slice 5b: 03.08 catches + 03.08b far-gate exit (split at the catch autosave) | +923 / +247 | passed with [#623](https://github.com/dayfine/Arduino-Source/pull/623)'s port (11-16) |
| [#610](https://github.com/dayfine/Arduino-Source/pull/610) | `plza/newgame` | [#604](https://github.com/dayfine/Arduino-Source/pull/604) | 6162fe260 | 01.00 new game from HOME; one launch by title for every start and reload | +2857 / +1455 | 01.00 passed from HOME (11-15) |
| [#619](https://github.com/dayfine/Arduino-Source/pull/619) | `plza/0304-save-moved` | [#610](https://github.com/dayfine/Arduino-Source/pull/610) | d9180d994 | no retry from a save that moved past the checkpoint's start | +181 / +141 | used from 03.05 on (11-16) |
| [#620](https://github.com/dayfine/Arduino-Source/pull/620) | `plza/0308-catch-in-zone` | [#619](https://github.com/dayfine/Arduino-Source/pull/619) | 68e2938f5 | 03.08: search Wild Zone 1 by camera scans and short out-and-back legs | +1148 / +673 | 03.08 never passed; replaced by [#623](https://github.com/dayfine/Arduino-Source/pull/623)'s port |
| [#622](https://github.com/dayfine/Arduino-Source/pull/622) | `plza/backup-retry` | [#620](https://github.com/dayfine/Arduino-Source/pull/620) | ff3f71086 | rewind to the backup save on failure after an autosave; manual menu saves | +1057 / +1074 | auto rewind to 03.06 worked (11-16) |
| [#623](https://github.com/dayfine/Arduino-Source/pull/623) | `plza/port-original-0308-0311` | [#622](https://github.com/dayfine/Arduino-Source/pull/622) | 9b4ddc9f9 | the original's steps ported back; merges [#624](https://github.com/dayfine/Arduino-Source/pull/624), [#625](https://github.com/dayfine/Arduino-Source/pull/625), [#626](https://github.com/dayfine/Arduino-Source/pull/626) (see below) | +4735 / +3453 | 03.06-03.11 passed (11-16, runs 9 and 10) |
| [#590](https://github.com/dayfine/Arduino-Source/pull/590) | `plza/throw-helper` | `upstream-main` | 2394ee31b | shared ball throw helper for InPlaceCatcher, PostKillCatcher and our catch | +145 / - | - |
| [#576](https://github.com/dayfine/Arduino-Source/pull/576) | `plza/autostory` | `main` | 8827debb8 | long-lived full-story reference (+55k) | - | - |
| [CLT #18](https://github.com/dayfine/CommandLineTests/pull/18) | `plza/all-frames` | `main` | - | every frame the PLZA tests read (84 files) | - | 9 frames missing |

[#623](https://github.com/dayfine/Arduino-Source/pull/623) in detail: the original `plza/autostory` steps for 03.08-03.11 ported onto the lean chain
(the bare scene is a B mash until control, for `leave_mable`, `five_registered` and `arrive_3f`);
the 03.06 start signature checks party 1; an unsigned checkpoint that was asked for runs as asked;
the menu-save test rows. Merged into it: [#624](https://github.com/dayfine/Arduino-Source/pull/624) (`plza/autosave-promote`, a devtool that promotes
`unverified` autosave rows from run logs), [#625](https://github.com/dayfine/Arduino-Source/pull/625) (`plza/reload-switch`, a developer option to skip
`reload_when_chained`), [#626](https://github.com/dayfine/Arduino-Source/pull/626) (`plza/marker-read-plausible`, `walk_to_marker` drops near reads
the walk cannot have reached).

CI status:

- 2026-11-16 consolidation: the log_detail() split went to [#574](https://github.com/dayfine/Arduino-Source/pull/574) (engine) and [#577](https://github.com/dayfine/Arduino-Source/pull/577) (backend); the autosave sightings (a9f619526) to [#582](https://github.com/dayfine/Arduino-Source/pull/582), with the plan flags in [#595](https://github.com/dayfine/Arduino-Source/pull/595) (02.07), [#596](https://github.com/dayfine/Arduino-Source/pull/596) (03.01, 03.02) and [#598](https://github.com/dayfine/Arduino-Source/pull/598) (03.11). [#610](https://github.com/dayfine/Arduino-Source/pull/610)'s tree is unchanged. Scene machine fixes stay in [#604](https://github.com/dayfine/Arduino-Source/pull/604) and the launch fix in [#610](https://github.com/dayfine/Arduino-Source/pull/610) for now; [#576](https://github.com/dayfine/Arduino-Source/pull/576) not yet merged (76 conflicted files). Ubuntu dispatched on #574, #577, #582, #598, #610.

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
- `ZoneGateReader` pillar steering: the 03.08b gate run is heading-only; it passed on console
  several times on 2026-11-16 (with [#623](https://github.com/dayfine/Arduino-Source/pull/623)).
- `fly_to_center_or_stay`, `check_out_of_wild_zone`, `face_objective`.
- No longer cut: menu saves (`save_from_menu`, in [#622](https://github.com/dayfine/Arduino-Source/pull/622) and [#623](https://github.com/dayfine/Arduino-Source/pull/623)).

## Console

All on lab-B. Source: `~/BDSPLab/notes/2026-11-09-checklist.md` (local).

- 2026-11-15, lab build e36a816a4 (slice 1 E + lab tooling): 01.00 passed on a new game (autosave
  badge seen); 01.01 3/3, runs 2 and 3 reloaded the latest save and walked to the plaza fade.
- 2026-11-15 evening, lean slices with [#604](https://github.com/dayfine/Arduino-Source/pull/604)/[#610](https://github.com/dayfine/Arduino-Source/pull/610) fixes, Italian, new game:
  - 21:33 (a6a1f6067) and 22:01 (94b5f7e59): 01.00 passed (full new game by program) and 01.01
    passed; 01.02 failed (Italian choice rows, then the 'A Parla' prompt; both fixed).
  - 23:25 (33401063a): 01.01 to 02.01 passed, five checkpoints one-shot.
  - 23:50 (a3e7f13a1): 02.02 and 02.03 passed one-shot; the run went on into 02.04. The checklist
    has no pass lines for 02.04-02.07; the next run's save was at 03.01's start.
- 2026-11-16 overnight, lean chain: 03.01 passed 01:03 (8bab6e0cc). 03.02 failed at Taunie and was
  split into 03.02/03.02b; 03.02b passed one-shot at 03:49 (3917b1173), 03.03 passed 03:54. 03.04
  failed (after_pokedex, then its walk); its autosave had moved the save to 03.04's end, fixed in
  [#619](https://github.com/dayfine/Arduino-Source/pull/619). 03.05 passed one-shot 07:00 and 03.06 07:01 (dcf1e9e5e); 03.07 passed 07:17
  (b47ca38fb). The lean 03.08 search ([#620](https://github.com/dayfine/Arduino-Source/pull/620)) never passed.
- 2026-11-16 evening, 03.06 to 03.11 on lab builds of [#623](https://github.com/dayfine/Arduino-Source/pull/623), up to 974e29db9: run 9 passed
  03.06-03.08b and failed 03.09 on a marker misread (40 m read as 4; fixed in [#626](https://github.com/dayfine/Arduino-Source/pull/626)). Run 10
  passed 03.08b, 03.09, 03.10 and 03.11: "AutoStory done through 03.11" at 21:41. The
  `leave_mable` B-mash fix and the office save worked. The auto rewind to the 03.06 backup save
  ([#622](https://github.com/dayfine/Arduino-Source/pull/622)) worked earlier that day.
- Next: one full-chain run, 01.00 to 03.11, on a fresh game, on lab build a6c80c35d
  ([#623](https://github.com/dayfine/Arduino-Source/pull/623) 9b4ddc9f9). First delete console B's PLZA save (needs your OK). Afterwards run the
  autosave promotion tool ([#624](https://github.com/dayfine/Arduino-Source/pull/624)) on its log. Optionally run with the skip-reload switch
  ([#625](https://github.com/dayfine/Arduino-Source/pull/625)) to test 03.04.

## Side PRs

- Fork [#585](https://github.com/dayfine/Arduino-Source/pull/585) (StringSelect dropdown text was black in dark mode; found while testing [#572](https://github.com/dayfine/Arduino-Source/pull/572)'s
  dropdowns) went upstream as [PokemonAutomation/Arduino-Source#1916](https://github.com/PokemonAutomation/Arduino-Source/pull/1916): one commit f8ed1ceae on
  branch `upstream/string-select-text-color`. You tested it on console B's lab app. Merged upstream
  2026-11-16.

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
