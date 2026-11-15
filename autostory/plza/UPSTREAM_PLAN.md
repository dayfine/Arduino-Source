# PLZA AutoStory: plan for the upstream PR chain and the verification runs

## Your TODOs

State on 2026-11-15. Agents never open upstream PRs or write their descriptions: every upstream
step below is yours. Each item says what to do and what we recommend.

1. **Answer the eleven plan questions** (details in section 5). Reply "agree" or give your call:
   1. Q1 split S2? Already done finer than planned: S1 is #529-#534, S2 is #535-#545. Recommend: keep that split.
   2. Q2 one PR per Main Mission? Recommend: yes, keep #565, #566, #567 separate.
   3. Q3 fault sweep upstream? Recommend: yes, as one shared PR after #567, once its two known-failing rows (warp_no_op, 03.06 and 03.08) are fixed. It is not in today's chain.
   4. Q4 `BREAKPOINTS` option upstream? Recommend: no option on the upstream panel; keep the small engine hook.
   5. Q5 dev-folder override upstream? Recommend: no folder scan upstream; keep the `dev_files` span as the test seam.
   6. Q6 pass rule? Recommend: 3/3 for walks and scenes, 2/3 for the battles 01.03 (Andi) and 03.02 (Taunie).
   7. Q7 who opens the CommandLineTests PR (C1)? Recommend: you, from a branch an agent prepares on dayfine/CommandLineTests.
   8. Q8 branch names? Already `plza/upstream/<s1-01|...|p4-02>`. Recommend: keep; set `upstream-bound` on each branch before its handoff.
   9. Q9 old fork PRs? Recommend: close #416, #354, #355, #356, #335 and the umbrella drafts #522, #524, #525 with a link to the re-split chain; keep #492 until the fault sweep PR (Q3) replaces it, and #418 (CFR) on the fork.
   10. Q10 S0b on S0a? Moot: both merged into fork main today (#519 shared stats, #520 `CheckpointId`). Recommend: hand them upstream in that order.
   11. Q11 which branch the verification runs use? Recommend: today's tip `plza/fault-sweep`; carry the records over as the test notes of the P and M PRs.
2. **Review the shared PRs yourself before each handoff.** All 17 shared PRs (#529-#545) are
   infra, which upstream reviews strictly; read each one in full. The agent review left every one
   at NEEDS_REWORK, each at an older SHA than its tip; ask for a re-review at the tip and wait
   for a clean one first. For the PLZA PRs (#555-#569) the API review says LGTM with nits on half of
   them; read at least #559 (console backend), #563 (catches) and #568 (story game, +4.2k), the
   ones that drive the console.
3. **Land the test frames upstream (C1) before #544.** CI on #544 and every PR above it fails
   only with `FileException`: the frames in `NintendoSwitch/HomeTileReader`,
   `PokemonLZA/AutoStoryDetectors` and `PokemonLZA/AutoStoryScreenEvents` (plus the five folders
   in section 2) are on dayfine/CommandLineTests, not PokemonAutomation/CommandLineTests. Open
   that PR on PokemonAutomation/CommandLineTests; an agent prepares the branch and file list.
4. **Open the upstream PRs, one at a time, under the three-open limit.** Two of yours are open
   upstream now (#1881 FRLG free party slots, #1845 command-line test mode), so one slot is free.
   Order: #519 (shared stats) first, then #520 (`CheckpointId`), then #529 and up the chain
   bottom first, always the lowest unopened one as a slot frees. Each upstream PR targets `main`;
   you write its description (the fork draft's body is the factual handoff note to draw on).
5. **Before each handoff, ask for the pre-flight**: an agent merges `main` into the branch, fixes
   every code-health finding (advisory ones too; CI's code-health fails today on 13 PRs, listed
   below), confirms Fork Draft CI green, and reports the tested SHA. You then open the PR.
6. **Console verification (section 4.2)** is ours, but tell us when console B is free for the
   3x runs per checkpoint; the P/M PRs link their results.

## Chain

Fork drafts on dayfine/Arduino-Source, bottom first. #519 and #520 merged into fork `main` today
and #529 was retargeted to `main` (its branch is 2 commits behind `main`; merge `main` in before
handoff). CI: "pass" = Fork Draft CI (Ubuntu build + unit tests), code-health and tabs all
pass; failing checks are named. "frames" = Ubuntu build fails only on test frames not yet on
upstream's CommandLineTests (TODO 3). Review = last agent verdict. All are open drafts.

| PR | Branch | Base | What | CI | Review |
|---|---|---|---|---|---|
| #519 | `autostory/shared-stats` | `main` | shared AutoStory stats, Runs, clearer labels (S0a) | - | merged into fork main |
| #520 | `autostory/checkpoint-id` | `main` | `CheckpointId` value type (S0b) | - | merged into fork main |
| #529 | `plza/upstream/s1-01` | `main` | tile grid, step kinds, action args, built-in action table | pass | NEEDS_REWORK |
| #530 | `plza/upstream/s1-02` | s1-01 | story log, failure classes, step outcome | code-health | NEEDS_REWORK |
| #531 | `plza/upstream/s1-03` | s1-02 | steps, checkpoints, breakpoints, save modes as data | code-health | NEEDS_REWORK |
| #532 | `plza/upstream/s1-04` | s1-03 | checkpoint as story events, outcome classes, tile poses | code-health | NEEDS_REWORK |
| #533 | `plza/upstream/s1-05` | s1-04 | tile-game step executor, story JSON readers | code-health | NEEDS_REWORK |
| #534 | `plza/upstream/s1-06` | s1-05 | CommonTools screen events | pass | NEEDS_REWORK |
| #535 | `plza/upstream/s2-01` | s1-06 | checkpoint attempts, story segments | code-health | NEEDS_REWORK |
| #536 | `plza/upstream/s2-02` | s2-01 | read a story from checkpoint files | pass | NEEDS_REWORK |
| #537 | `plza/upstream/s2-03` | s2-02 | frame coverage, covered watches, scene judges | code-health | NEEDS_REWORK |
| #538 | `plza/upstream/s2-04` | s2-03 | story backend interface | pass | NEEDS_REWORK |
| #539 | `plza/upstream/s2-05` | s2-04 | action registry, built-in actions, step runner | pass | NEEDS_REWORK |
| #540 | `plza/upstream/s2-06` | s2-05 | postcondition checks, engine save policy | code-health | NEEDS_REWORK |
| #541 | `plza/upstream/s2-07` | s2-06 | CommonTools screen machine and step trace | code-health | NEEDS_REWORK |
| #542 | `plza/upstream/s2-08` | s2-07 | the story engine | code-health | NEEDS_REWORK |
| #543 | `plza/upstream/s2-09` | s2-08 | run options, language and checkpoint range dropdowns | pass | NEEDS_REWORK |
| #544 | `plza/upstream/s2-10` | s2-09 | NintendoSwitch: which HOME tile is selected | frames | NEEDS_REWORK |
| #545 | `plza/upstream/s2-11` | s2-10 | launch a game to its save through Switch prompts | frames | NEEDS_REWORK |
| #555 | `plza/upstream/p1-01` | s2-11 | LZA lock-on bracket and party bar detectors | frames | none yet |
| #556 | `plza/upstream/p1-02` | p1-01 | story scene detectors | frames | LGTM with nits |
| #557 | `plza/upstream/p1-03` | p1-02 | dialog, choice, Confirm Choices, objective marker readers | frames | none yet |
| #558 | `plza/upstream/p2-01` | p1-03 | screen readings, failure classes, screen events | frames | LGTM with nits |
| #559 | `plza/upstream/p2-02` | p2-01 | console backend: dialogs, scenes, battles | frames | none yet |
| #560 | `plza/upstream/p2-03` | p2-02 | the scene machine | frames | LGTM with nits |
| #561 | `plza/upstream/p2-04` | p2-03 | timed walks, walk screen checks, step checks | frames | none yet |
| #562 | `plza/upstream/p2-05` | p2-04 | start load, launch, Confirm Choices, zone gate walk | frames, code-health | LGTM with nits |
| #563 | `plza/upstream/p3-01` | p2-05 | catch plan, catch stop, lock-on watch | frames | LGTM with nits |
| #564 | `plza/upstream/p3-02` | p3-01 | Wild Zone 1 catches, objective marker walks | frames, code-health | none yet |
| #565 | `plza/upstream/m1-01` | p3-02 | Main Mission 01 plans (01.01-01.03) | frames | LGTM with nits |
| #566 | `plza/upstream/m2-01` | m1-01 | Main Mission 02 plans (02.01-02.07) | frames | none yet |
| #567 | `plza/upstream/m3-01` | m2-01 | Main Mission 03 plans (03.01-03.11) | frames | LGTM with nits |
| #568 | `plza/upstream/p4-01` | m3-01 | the story game and its table | frames, code-health | LGTM with nits |
| #569 | `plza/upstream/p4-02` | p4-01 | the program | frames | none yet |

Not in the chain: #522 (`plza/upstream/s1`, base `autostory/checkpoint-id`, CI code-health),
#524 (`s2a`, pass) and #525 (`s2b`, pass) are the earlier three-PR cut that #529-#545 re-split;
close them (Q9). #418 (CFR actions) and #492 (fault sweep) stay fork-only for now.

The rest of this doc is the plan of 2026-11-14 that the chain above was built from.


Status: **proposal for the user to approve, 2026-11-14.** The fork chain was rebuilt on 2026-11-15 (see Chain above).
Written after the user's call: "PLZA -- this is done, so next step is to tidy things up and
get every PR in a good shape: have the proper PR chain structure for what we really need (for
upstream); have a clear testing instruction (which we will also execute again in the next couple
testing runs, e.g. in verification mode that runs 3x each checkpoint)".

Rules this plan follows: `AGENTS.md` and `pokemon_sv/PR_WORKFLOW.md` on this branch (fork
drafts only; the user opens every upstream PR and writes its description; at most three open
upstream PRs; a stacked upstream PR targets `main` and shows all its parents' commits), and the
PR-shape decision of 2026-11-12 (3 shared + 3 per-game foundation PRs, then one PR per segment,
one linear stack per game; fixes land on the tip and are folded into the owning PR).

Numbers below (files, +/-) are GitHub's for each fork PR on 2026-11-14 unless a `git diff`
command is given. Checkpoint ids are the ones on the tip after the renumber that landed today
(`a6e32757b`, "drop 03.07d, renumber 03.08b-03.09b to 03.09-03.11"): 01.00-01.03, 02.01-02.07,
03.01-03.11, 22 checkpoints, dropdown indices 0-21. In MM03's tail: 03.07d dropped, 03.08 kept,
03.09 (was 03.08b), 03.10 (was 03.09), 03.11 (was 03.09b); indices 18-21. 03.08 has no save of
its own at its start, so rerunning 03.08 needs 03.07 first (user, 2026-11-14). If the user
renumbers again, every id in section 4 moves with it.

The user's addition of 2026-11-14 (relayed by the coordinator): the chain starts with two
standalone shared PRs off `main`, before anything PLZA (S0a, S0b in section 3), and then a clean
chain the user reviews closely one PR at a time. So **every PR is self-contained: one purpose, an
explicit scope (what is in, what is not), and its own test notes** (which suites, which console
runs, how to run them).

## 1. Today's PLZA fork stack

Bases followed from the tip down to `main`. Tips at the time of writing:
`plza/story-detectors` bfea2d536, `plza/story-base` fcb1074c3, `plza/story-mm1` 961911268,
`plza/story-mm2` 26c021902, `plza/story-engine` a6e32757b, `plza/fault-sweep` 24133e149,
`plza/cfr-actions-v2` 4c568833d, `nav/shared` 3df87aa4e.

```
main
└─ #62   switch/system-keyboard         +702/-1       8 f   open   Switch on-screen keyboard (PLZA does not use it; the chain passes through it)
   └─ #45   autostory/r3-shared-stats   +438/-2       8 f   open   shared run report (AutoStoryStats)
      └─ #325  autostory/story-range-options +390/-49 10 f  open   START/END checkpoint options
         └─ #391  autostory/sv-retry-core  +32762/-130 259 f open   share SV's checkpoint retry loop; nav/shared (#114) merged into it
            └─ nav/shared  (integration branch, no PR of its own since #114 merged)
               merged into it: #312 story step types + engine (+4788), #313 ScreenEvents (+793),
               #332 engine over a backend (+17220, 154 f), #360 tile navigation (+5135),
               #361 scene judges (+2280), #362 story actions/interrupts (+3972), #419 sim goldens,
               #422 fault counters, #448 breakpoints (+467), #474 shared story files + dev folder
               (+1080), #477 launch from HOME (+1182), #479 failure classes (+1439),
               #490/#494/#495/#497 review fixes, #227, #390, #399
               └─ #416  plza/story-detectors   +1947/-0     22 f   base autostory/story-breakpoints (= nav/shared after #448 merged)
                  └─ #354  plza/story-base     +6269/-0     50 f   program, StoryEngine<ZonePose> backend, actions glue, 01.00
                     └─ #355  plza/story-mm1   +762/-35     17 f   01.01-01.03
                        └─ #356  plza/story-mm2 +474/-23    18 f   02.01-02.06 (02.07 came later, in #335)
                           └─ #335  plza/story-engine +16866/-1877 228 f  03.01-03.11, Wild Zone 1 catches, PartyBar/MinimapZone/CaughtMark/DexRegistered detectors
                              ├─ #492  plza/fault-sweep +1298/-30 16 f   failure classes + fault sweep tests (03.06, 03.08)
                              └─ plza/cfr-base-v2 (no PR) = story-engine + lab/main (#103) + switch/system-keyboard + build/headless-tests (#207)
                                 └─ #418  plza/cfr-actions-v2 +5301/-195 64 f  Command File Runner: lza_* actions, type, steps file
```

Sizes that matter for the restructure:

| Range | `git diff --shortstat A...B` | Meaning |
|---|---|---|
| `main...nav/shared` | 276 files, +34248/-173 | everything shared the PLZA chain sits on (incl. the SV retry core and tile navigation PLZA does not use) |
| `main...plza/fault-sweep` | 419 files, +53279/-175 | the whole chain to the PLZA tip |
| `nav/shared...plza/story-detectors` | 22 files, +1947 | #416's own change |
| `plza/story-mm2...plza/story-engine` | 256 files (three-dot, several merge bases) | #335 shows nav/shared commits mm2 has not taken: its own part is `VideoGames/PokemonLZA/Programs/AutoStory` (132 files) and six `Inference/` files (PartyBarDetector, ...) |

So the PLZA-only code is about 19k lines in ~140 files (53.3k - 34.2k), and the shared base it
depends on is 34k lines. #335 also shows +1912/-229 in `Pokemon/AutoStory`, +2305/-378 in
`Pokemon/AutoStory/Engine` and +1197/-89 in `Pokemon/AutoStory/Data`: those are #474/#477/#332
commits merged in, not PLZA changes; they vanish once #356 takes nav/shared.

`plza/story-engine` is 0 commits behind `main`, `nav/shared` and `autostory/story-breakpoints`,
1 behind `autostory/r3-shared-stats` (#45 review fix) and 4 behind `autostory/sv-retry-core`.

## 2. What upstream needs for PLZA AutoStory 01.00 -> 03.11, and what stays on the fork

### Needed (what the PLZA code includes)

PLZA's AutoStory sources include, outside their own folder, only:

- `Pokemon/AutoStory/Engine/*` (StoryEngine, StoryBackend, StoryGame, StoryLaunch, CoveredWatch,
  ScreenObservation, StoryActionRegistry, StoryFakeBackend, StoryActionNotImplemented);
- `Pokemon/AutoStory/*` (StoryStepExecutor, SceneJudges, StoryOutcome(+Class), StoryLog,
  StoryFailureClasses, StoryFaultSweep, StorySwitchPrompts, BuiltinStoryActions, AutoStorySelect,
  AutoStoryStats);
- `Pokemon/AutoStory/Data/*` (AuthoredCheckpointJson, StoryFiles, StoryDevFiles, StoryRowExecution);
- `Pokemon/AutoStory/Types/*` (StoryCheckpoint, StoryStep, StoryActionSpec, StoryCheckLevel,
  StoryBreakpoints);
- `Pokemon/Options/Pokemon_StoryRunOptions.h`, `Pokemon_GameLanguages.h`;
- `CommonTools/ScreenEvents` (#313), `NintendoSwitch/Inference` additions from #335
  (3 files, +306), `cmake/EmbedTextFiles.cmake` (shared, on `nav/shared` as b426f749f
  "pa_embed_text_files", with #474's story files; it embeds the plan JSON into the binary).

It does **not** include `Pokemon/Navigation` tile navigation (#360/#361/#362/#419/#440/#464), the
on-screen keyboard (#62: the user types the name by hand before 01.00), or anything from
`VideoGames/PokemonSV`. The first task of the restructure is therefore a **dependency-closure
build**: `main` + the shared story engine subset + the PLZA folder, built and unit-tested, to prove
the list above is complete and to find what in #391 (sv-retry-core, 32k lines) PLZA really pulls
(`AutoStorySelect`, `StoryRunOptions` and the retry loop are the suspects).

Resources: PLZA AutoStory adds **no** `RESOURCE_PATH()` or Packages files. The platform reference
cells (`ReferenceData/plza-platform.json`) and the 22 plan files are embedded at build time; the
`CategoryIcons/PokemonLZA.png` and `PokemonLZA/LocationName.json` resources already exist upstream.
Confirm with `git grep RESOURCE_PATH -- SerialPrograms/Source/VideoGames/PokemonLZA/Programs/AutoStory`
on the final chain (today: only the test helper's `RESOURCE_PATH(), test.input.folder`).

Test images: `CommandLineTests/PokemonLZA/AutoStoryDetectors/` (811 frames) is already on
dayfine/CommandLineTests `main`. The fork CI pin (`cpp-ci-serial-programs-base.yml` on the PLZA
branches checks out `dayfine/CommandLineTests` at `02f2c8e3e…`, branch `plza/story-engine-pin`)
adds 82 fork-only files in five folders that must go to PokemonAutomation/CommandLineTests before
the matching code PR: `AutoStoryConfirmChoices` (30), `LockOnBracketDetector` (30),
`AutoStoryLookPicker` (18, only if the intro picks go upstream; see below), `PartyBarDetector` (3),
`AutoStoryDialogChoices` (1). The `.github/workflows` edits that point CI at the fork's
CommandLineTests never go upstream; the upstream chain keeps `PokemonAutomation/CommandLineTests`.

### Dependency closure, measured 2026-11-14 (batch 1)

Include closure of `VideoGames/PokemonLZA/Programs/AutoStory` on `plza/fault-sweep` 24133e149,
tests and the fault sweep left out: **121 files, +12.0k lines** outside the PLZA folder (vs
`autostory/checkpoint-id`). Nothing from `Pokemon/Navigation` tile navigation, the SV retry core
(#391: `CheckpointRetry`, `StoryRun`, interrupts, `StoryActions`) or the keyboard is in it. It does
pull `Pokemon/Navigation/{Pokemon_Direction,Pokemon_Grid}` and `Pokemon_TilePose`: the backend's
`tap(Direction)`, `GridPose` (woven into the step executor, postcondition check and step JSON) and
the engine's `GridPose` instantiations need them; cutting them is a rewrite, so they stay.

Rebuilt fork PRs, each one's own change with its tests:

| PR | Branch, tip | Own size | Of it, tests |
|---|---|---|---|
| S1 #522 | `plza/upstream/s1` 5236786c6 | 72 files, +7.5k | +2.8k |
| S2a #524 | `plza/upstream/s2a` 5f5f6d205 | 69 files, +9.4k (loader +1.0k of it) | +4.4k |
| S2b #525 | `plza/upstream/s2b` 00c6ceb84 | 25 files, +2.3k | +0.9k |

S2a is under 10k with the loader, so no S2a'. Where the cut differs from section 3:
- Failure classes are in S1, not S2b: `Outcome` carries `std::optional<StoryFailureClass>`. Their
  sim-fault mapping (`injected_by`, `story_failure_class_of(FaultClass)`) is cut, which drops
  `Pokemon_SimFaults`; it returns with the fault sweep (S3, after M3).
- `record_story_counts()` writes #520's stats (Segments Started, Checkpoints Started, Resets);
  a segment report now counts checkpoints started, not passed.
- The `BREAKPOINTS` option is out of `StoryRunOptions`; the engine's pause hook and
  `StoryBreakpoints` type stay (S1). PLZA's `Moves.h` still includes the type.
- `Pokemon_StoryDevFiles.h` keeps only the `StoryDevFile` struct (the loader's test seam); the
  folder scan (`story_dev_folder`, `read_story_dev_files`) is out, so P2's `StoryFiles` passes none.
- `cmake/EmbedTextFiles.cmake` waits for P2, its first caller.
- The shared suites register through `Pokemon_AutoStory_Tests.h` (`add_tests_AutoStory()`).
- C1 needs `NintendoSwitch/HomeTileReader/` (6 frames) too: S2b's HomeTileReader test reads them,
  so S2b's fork CI pins `dayfine/CommandLineTests` at 0195f3b0c as `plza/fault-sweep` does.

### Fork- or lab-only, stays out

| Item | Where today | Why it stays |
|---|---|---|
| Command File Runner LZA actions, `type`, steps file, `cfr_steps_to_story.py` | #418 and its base `plza/cfr-base-v2` | lab recording tool; its base carries `lab/main` (#103, HttpServer, Sockets, lab channel) |
| the lab channel, lab device ownership, `set_story_pause_wait()` lab wait | `lab/main` | lab infrastructure |
| `BREAKPOINTS` run option | #448 (shared), surfaced in PLZA's panel | "only a lab build waits": a no-op option upstream (question Q4) |
| dev-folder override (`story_dev_folder("plza")`, `Pokemon_StoryDevFiles.h`, `dev_files` span) | #474 (shared), PLZA reader in #354/#335 | lab tuning of plan files without a rebuild; upstream's program would pass an empty span (question Q5) |
| intro picks + 00.00 (language, look, name by program) and `AutoStoryLookPicker` frames | dev-only branch #471 | the user does the new-game setup by hand; 01.00 starts at Confirm Choices |
| CI pins to `dayfine/CommandLineTests` | `.github/workflows` edits in #335, #418 | fork only |
| fault sweep **known-failing** rows (warp_no_op on 03.06 and 03.08) | #492 | fix or drop before upstream; the sweep itself is worth upstreaming (see S3) |
| run records, `segments.tsv`, this plan | `agents` branch | notes |

## 3. Proposed upstream chain

Eleven Arduino-Source PRs (two standalone shared PRs off `main`, three shared foundation, three
PLZA foundation, three segments) plus one CommandLineTests PR, in this order. Each upstream PR
targets `main` and shows its parents, so sizes below are each PR's **own** change. The user writes
every upstream description; the fork draft that mirrors each one carries the factual handoff note
(branch, tested revision, tests, dependencies) for the user to draw on. Each PR's row names its
purpose and scope; its test notes are the unit suites it adds (4.1) and, for P2 onward, the
console runs of 4.2 it needs.

| # | PR | Own files | Own size | Purpose and contents |
|---|---|---|---|---|
| S0a | Shared AutoStory stats (standalone, off `main`) | `Pokemon/AutoStory/Pokemon_AutoStoryStats.{h,cpp}` moved from SV, SV's program using it, stats tests | ~0.5k (#45's +438, reshaped) | SV's `AutoStoryStats` moves to `Pokemon/AutoStory/` and is shared; adds "Runs"; renames labels with `StatsTracker` aliases (Segment -> Segments Started, Checkpoint -> Checkpoints Passed, Reset -> Resets). No PLZA code. Tests: the stats/alias unit test; SV's panel still loads its saved stats. |
| S0b | `CheckpointId` value type (standalone, off `main`, after S0a) | `Pokemon/AutoStory/Types/Pokemon_CheckpointId.{h,cpp}` + tests, loader change, log line | ~0.5-1k | parses "03.08" -> {segment 3, checkpoint 8}; the loader rejects malformed or suffixed ids (so the renumber above is a prerequisite); logs read "[Segment] 03" (not the game's name); Segments Started counts each new segment number; the order test checks ids are unique and increasing. Tests: parse/reject table, order test, a log-line test. Depends on S0a for the stat it feeds (or stub it if the user wants S0b independent; Q10). |
| S1 | Screen events + story step types and engine | `CommonTools/ScreenEvents`, `Pokemon/AutoStory/Types`, `Pokemon/AutoStory/Engine` core, `Pokemon/AutoStory/Pokemon_Story{Step,StepExecutor,Outcome,Log}` | ~5.6k (#313 + #312) | what the screen is doing since when; the step vocabulary and the executor. Already reviewed on the fork (#312, #313 and their nits #490/#497). |
| S2 | Engine over a backend, story files, run options, launch, failure classes | rest of `Pokemon/AutoStory/Engine`, `Pokemon/AutoStory/Data`, `Pokemon/Options/Pokemon_StoryRun*Options`, `Pokemon_AutoStoryStats`, `Pokemon_StorySwitchPrompts`, `Pokemon_StoryFailureClasses`, `cmake/EmbedTextFiles.cmake` | 10-20k; **measure with the closure build** | #332 (17k) is the elephant: the closure build says how much of it PLZA needs. Folds #45, #325, #448 (minus lab wait), #474 (minus dev folder, Q5), #477, #479 and the parts of #391 PLZA includes. Split in two if the closure is above ~12k (S2a engine+data, S2b options+launch+failure classes). |
| S3 | Story fault sweep (shared) | `Pokemon_StoryFaultSweep.*`, shared fault counters (#422's `Pokemon/Navigation/Simulation` part if PLZA's sweep uses it) | ~1-2k | unit-test-only infrastructure that found two real bugs (timed walk replay, `press_a_first`). Can wait until after P3 if the user prefers fewer shared PRs first (Q3). |
| C1 | **CommandLineTests**: PLZA AutoStory frames | the five folders above (82 files) | images | before P1 opens; the pin on the fork chain then returns to upstream's repo. |
| P1 | PLZA AutoStory detectors | #416's 19 files + the later detectors now in #335: `PartyBarDetector`, `LockOnBracketDetector` changes, `MinimapZoneDetector`, `CaughtMarkReader`, `DexRegisteredDetector`, `ObjectiveMarkerReader`, `ConfirmChoicesReader`, `DialogChoiceBox` | ~3-4k | pixel readers, no presses; tests on synthetic frames and `CommandLineTests/PokemonLZA/*`. |
| P2 | PLZA AutoStory program and new game | #354 as is: program, `PlzaConsoleBackend`, `ScreenObserver`, `SceneMachine`, `StoryFiles` (built-in plans only), `StartLoad`, `Launch`, `Table`, 01.00 | ~6k | runs 01.00 end to end; its table/engine/scene-machine tests. |
| P3 | PLZA AutoStory actions | `Actions/*` beyond what P2 needs for 01.00: Moves, MarkerWalks, GateWalk, Zone1Road, Catches/CatchPlan/CatchStop, SpeciesCheck, BracketWatch, WalkScreenChecks, battle entry | ~5-7k | the per-game "battles/actions/setup" foundation; tests `AutoStoryGateWalk`, `AutoStoryCatchPlan`, `AutoStoryCatchStop`. |
| M1 | Main Mission 01 (01.01-01.03) | three plan files + their table rows and any MM01-only action | ~0.8k | #355 as is |
| M2 | Main Mission 02 (02.01-02.07) | seven plan files + hotel elevator/corridor actions | ~0.6k | #356 + 02.07 from #335 |
| M3 | Main Mission 03 (03.01-03.11) + Wild Zone 1 | eleven plan files, catch routines' wiring, `AutoStoryFailures` classes, PLZA `FaultSweep_Tests` | ~3-5k | the rest of #335 + #492 |

Order under the three-open-PR limit (C1 is on another repo and does not count against it, but it
must merge before P1):

1. Open S0a and S0b first (standalone off `main`, the user's call of 2026-11-14), plus C1. S1 is
   prepared meanwhile, based on S0b.
2. As S0a/S0b merge: open S1, then S2 (or S2a). The fork drafts for P1-M3 are rebuilt meanwhile.
3. When S1 merges: open S2b or S3. When S2 merges: open P1.
4. Then P2, P3, M1, M2, M3 one at a time as slots free up: at most three open, always the
   lowest unopened one next.

Expected: at least six waves. M1 and M2 are small enough to merge into one "Main Missions 01-02"
PR if the user wants a shorter chain (Q2). S1 and everything after it take the shared stats and
`CheckpointId` from S0a/S0b, so the rebuilt fork branches base on them, not on today's #45.

How the fork restructure prepares this (not started; the user approves first): rebuild fresh
squashed branches bottom-up from the current tip, one agent at a time, so the final tree equals
`plza/fault-sweep`'s tree and the unit tests match (the PR-shape decision of 2026-11-12). The old
PRs #416/#354/#355/#356/#335/#492 close with a link to their replacement. #418 and
`plza/cfr-base-v2` stay fork drafts, rebased by merge onto the new tip.

Shared PRs that must land first: S1 and S2 before P1 (P1's tests use `ScreenEvents` frames and
the story types); S3 before M3 (its `FaultSweep_Tests`). C1 before P1.

## 4. Testing instructions

### 4.1 Unit tests

PLZA AutoStory suites (`PokemonLZA_Tests.cpp` registers them under `PokemonLZA::`):
`AutoStoryDetectors`, `AutoStoryConfirmChoicesReader`, `AutoStoryObjectiveMarkerReader`,
`AutoStoryZoneGateReader`, `AutoStoryGateWalk`, `AutoStory_StoryTable`, `AutoStoryScreenEvents`,
`AutoStoryFailures`, `AutoStoryFaultSweep`, `AutoStoryCatchPlan`, `AutoStoryCatchStop`,
`AutoStorySceneMachine`, `AutoStoryEngine`, `AutoStoryStartLoad`; plus the Inference suites
`LockOnBracketDetector` and `PartyBarDetector`. Shared: `Pokemon::StoryRunOptions`,
`Pokemon::StoryRangeOptions`, the engine and story-data tests of S1/S2.

Run them as upstream CI does (`.github/workflows/cpp-ci-serial-programs-base.yml`):

1. Build `SerialPrograms` (CMake preset `default`, Ninja, `-j4`).
2. Check out `CommandLineTests` next to the program (`SerialPrograms/bin/../../CommandLineTests`).
3. From `SerialPrograms/bin`: `./SerialPrograms --profile unit-tests --command-line-test-mode`
   (headless Linux adds `QT_QPA_PLATFORM=offscreen` and `TESSDATA_PREFIX=<bin>/Resources/Tesseract`).
   On the lab Mac never run the binary without `--profile`: the default profile opens console A's
   serial port. Wait for "Tests Finished" in the log, then stop the process.
4. Pass = every `PokemonLZA::*` and `Pokemon::*` suite reports 0 failures. Until C1 lands,
   the frames of the five fork-only folders fail with `FileException` on upstream's
   CommandLineTests; on the fork chain they pass through the pin.

On the fork, Fork Draft CI runs this on every push of a draft; marking the draft ready runs the
six upstream workflows.

### 4.2 The console verification run

One checkpoint at a time, from its own start save, three runs, then the postcondition check.
Phases and saves follow `autostory/SAVE_POLICY.md`; the program options are the shared
`StoryRunOptions` (`Pokemon_StoryRunOptions.h`).

Before the run (precondition):
- Console B shows the title screen with the checkpoint's start save as the newest save, read on
  the save card. PLZA autosaves (catches, check-in, scenes): if a later autosave moved the save,
  load the backup save (title screen: hold Up, press B + X) and read the BACKUP card first, or
  set `START_FROM_BACKUP_SAVE` on the panel. Never save by hand from a position nothing checked.
- Previous checkpoint is verification-passed (`segments.tsv`); N+1 never starts after a hand
  recovery of N.
- Panel: `LANGUAGE` = the save's game language; `STARTER` as the save has it; `RUN`:
  `START_CHECKPOINT` = `END_CHECKPOINT` = the checkpoint; `VERIFY` on, `RUNS` = 3, `PASSES` = 3
  (battle checkpoints: `PASSES` = 2, Q6); `SAVE_MODE` = After verification; `RELOAD_AT_START` on;
  `POSTCONDITIONS_ONLY` off; `BREAKPOINTS` off; `CHECKS` = the default level.

The run: the engine reloads the save, plays the checkpoint, reloads, plays it again, three times.
Only the last run may save, and only when every run passed (the engine enforces this). A failed
run never saves.

After the run (postcondition, a separate step): run once more with `POSTCONDITIONS_ONLY` on, and
confirm by screenshot the end position (zone/place and heading), the party (species count on the
party bar, registered count on the Pokedex where the checkpoint catches), and the mission state
(the next main mission's banner or objective marker). Write the result in `segments.tsv` and the
run record.

What counts as a pass: three runs each ending with the engine's verdict `passed` for the
checkpoint (log line per step with its verdict, no `(class=...)` failure line), no hand input,
no error dialog, the postcondition check passed, and the save afterwards reloads at the next
checkpoint's start. 2/3 is a pass only for a battle checkpoint, under the battle rule (Q6).

Start save per checkpoint (place from each plan's `start`; `-` = the checkpoint has no spot of
its own, the autosave before it is the start). These are the ids after today's renumber; they move
again if the user renumbers once more.

| Id | Title | Starts from |
|---|---|---|
| 01.00 | New game from Confirm Choices to the station platform | a new game, set up by hand to the Confirm Choices screen; no save |
| 01.01 | The platform to Main Mission 01 | Lumiose City, the station platform autosave |
| 01.02 | Main Mission 01: chase Pancham to the battle area | Lumiose City autosave after 01.01 |
| 01.03 | Main Mission 01: the starter and Andi | Lumiose City autosave after 01.02 (battle checkpoint) |
| 02.01 | Main Mission 02: Battle Zone to Brigitte | Lumiose City, 01.03's menu save |
| 02.02 | Main Mission 02: after Brigitte to Hotel Z | Lumiose City autosave after Brigitte |
| 02.03 | Main Mission 02: into the Hotel Z lobby | Lumiose City autosave outside Hotel Z |
| 02.04 | Main Mission 02: Hotel Z check-in | the lobby autosave (before the AZ talk) |
| 02.05 | Main Mission 02: Hotel Z elevator to Room 202 | autosave after check-in |
| 02.06 | Main Mission 02: the night in Room 202 | autosave in Room 202 |
| 02.07 | Main Mission 02: the first evening to Main Mission 03 | autosave after the night |
| 03.01 | Down to the lobby | Hotel Z, Room 202 autosave |
| 03.02 | Out of Hotel Z, battle Taunie | Hotel Z lobby autosave (battle checkpoint) |
| 03.03 | Over the rooftops to Station Front | Lumiose City autosave after Taunie |
| 03.04 | Pokemon Center, the Pokedex | Lumiose City, Station Front autosave |
| 03.05 | Heal at the Pokemon Center | Pokemon Center autosave |
| 03.06 | To Wild Zone 1 | the heal's save at the Pokemon Center counter |
| 03.07 | Wild Zone 1: first catches | Wild Zone 1 entry autosave |
| 03.08 | Five species, out of Wild Zone 1 | Wild Zone 1 autosave after 03.07's catches (3/5 registered); no save of its own, so rerunning 03.08 means running 03.07 first (START = 03.07, END = 03.08) |
| 03.09 | From Wild Zone 1's gate into the Research Lab | Lumiose City, the zone's gate autosave |
| 03.10 | Up the Research Lab's elevator to the Director's Office | Pokemon Research Lab autosave |
| 03.11 | Mable's research, the Rock Smash TM | Pokemon Research Lab autosave |

The "Starts from" column is from the plan files' `start` and the console notes; confirm each on
console during the first verification pass and record the observed autosave spot in the plan's
source note (the 2026-11-13 rule: a checkpoint's start equals where its autosave reloads).

### 4.3 Checklist for PR descriptions to link

```
PLZA AutoStory verification (link: agents:autostory/plza/UPSTREAM_PLAN.md#43-checklist-for-pr-descriptions-to-link)
[ ] Unit tests: PokemonLZA::* and Pokemon::* suites, 0 failures (Fork Draft CI or local run per 4.1)
[ ] Code health: 0 findings, advisory ones included (upstream-bound)
[ ] Console: each checkpoint this PR adds or changes ran 3x in VERIFY mode from its own save
[ ] Each run: engine verdict passed, no hand input, no (class=...) line, no error dialog
[ ] Postconditions checked separately (POSTCONDITIONS_ONLY + screenshot): position, party, mission state
[ ] Save after verification reloads at the next checkpoint's start
[ ] segments.tsv and the run record updated (agents branch), checkpoint ids as renumbered
[ ] No fork-only pins: CommandLineTests is PokemonAutomation's, frames landed upstream
```

## 5. Open questions for the user

**Q1. Split S2 or keep #332 whole?** #332 is 17k lines. Recommendation: run the dependency-closure
build first; if PLZA's closure of #332 + #474 + #477 + #391 is under ~12k lines, one S2; else S2a
(engine over a backend + story data) and S2b (run options, launch from HOME, failure classes).

**Q2. One PR per Main Mission, or merge M1+M2?** They are ~0.8k and ~0.6k. Recommendation: keep
one per segment as decided on 2026-11-12; the chain is already short and each shows one mission.

**Q3. Fault sweep upstream?** The sweep is unit-test-only and found two real bugs, but two rows are
known-failing (warp_no_op). Recommendation: upstream it (S3 + the PLZA `FaultSweep_Tests` in M3)
after fixing the two rows, S3 opened after P1 so the first wave is S1, S2, C1.

**Q4. `BREAKPOINTS` option upstream?** It only pauses in a lab build. Recommendation: leave the
option out of the upstream panel (keep the engine hook; it is small), so users do not see a
setting that does nothing.

**Q5. Dev-folder override upstream?** `story_dev_folder("plza")` lets the lab replace plan files
without a rebuild. Recommendation: keep the `dev_files` span in the loader (it is the test seam)
but no folder scan in the upstream program; the scan stays on the fork, re-added by the lab branch.

**Q6. Pass criterion: 3/3 everywhere, or 2/3 for battle checkpoints?** The battle rule
(2026-11-03) allows 2 of 3 for a battle that lost twice; the user's wording today is "3x each".
Recommendation: `PASSES` = 3 for walks and scenes; `PASSES` = 2 for 01.03 (Andi) and 03.02
(Taunie), and record the loss.

**Q7. Who opens C1 (CommandLineTests)?** The frames are console captures of the game's screens.
Recommendation: the user opens it, like every upstream PR; an agent prepares the branch on
dayfine/CommandLineTests with the five folders and a file list.

**Q8. Branch naming of the rebuilt stack.** Recommendation: `plza/upstream/<s1|s2|p1|...>`
on the fork, each fork draft marked ready only when its upstream twin opens, and
`git config branch.<name>.upstream-bound true` on each so the pre-push code-health gate blocks.

**Q9. The old fork PRs.** Recommendation: close #416/#354/#355/#356/#335/#492 with a link to the
rebuilt PR once its tests match the old tip; #418 stays open on the fork, merged onto the new tip.

**Q10. S0b on S0a, or independent?** `CheckpointId` feeds "Segments Started", which S0a adds.
Recommendation: S0b based on S0a (both small, reviewed back to back); if the user wants them
truly independent, S0b counts segments in its own test only and S1 wires the stat.

**Q11. Which fork branch is "done" for the verification runs?** The runs of 4.2 happen before the
restructure (next couple of runs) on today's tip, `plza/fault-sweep` 24133e149 or later.
Recommendation: run them on the current tip, record per checkpoint in `segments.tsv`, and carry the
records over as the test notes of P2/P3/M1-M3; a rebuilt branch whose tree equals the tip needs
no rerun, only the unit tests.
