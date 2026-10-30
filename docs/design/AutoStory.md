# AutoStory: FRLG pilot first, shared run flow and 2D movement later

Status: internal plan, revised 2026-10-28 after the map-reuse review. The upstream-facing
one-page summary is [AutoStory-Proposal.md](AutoStory-Proposal.md). The
review comment drafts for the BDSP PR are in
[AutoStory-1717-review-drafts.md](AutoStory-1717-review-drafts.md). Neither
file has been posted.
Sources: SV AutoStory on `main`; BDSP WIP
[PokemonAutomation/Arduino-Source#1717](https://github.com/PokemonAutomation/Arduino-Source/pull/1717)
at `65094d4cd`; SwSh WIP PokemonAutomation/Arduino-Source#1207 at `32d506c27`;
FRLG code on `main`; the [pret/pokefirered](https://github.com/pret/pokefirered)
decompilation; the [52poke walkthrough](https://wiki.52poke.com/wiki/%E9%99%84%E5%BD%95:%E5%AF%B6%E5%8F%AF%E5%A4%A2_%E7%81%AB%E7%B4%85%EF%BC%8F%E8%91%89%E7%B6%A0%E6%B5%81%E7%A8%8B).
Paths: `SV/` = `SerialPrograms/Source/VideoGames/PokemonSV/Programs/AutoStory/`,
`BDSP/` = `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/` (PR ref),
`FRLG/` = `SerialPrograms/Source/VideoGames/PokemonFRLG/`.

## Current implementation order (2026-10-28 review)

The [map-reuse review](AutoStory-Map-Reuse-Review.md) updates the work order
below. The [FRLG demo work plan](FRLG-Demo-Work-Plan.md) specifies map resources,
startup prerequisites, SID Helper/Starter RNG handoffs and acceptance gates. First prove the existing BDSP spatial model with a small FRLG map
demo (post-rival save → Oak’s Parcel → Pokédex), and a startup demo through
the first verified save. Then settle the common executor using both games.
The old ten-PR sequence in §7.7 is retained as a candidate inventory, not a
prerequisite chain. In particular, migrating the Elite Four farmer and SV
is no longer on the critical path to the first runnable FRLG demo.

Shared map and screen modules already exist in our BDSP work. Reuse them
through explicit prerequisites; do not rewrite them. Keep game-specific
observations and movement execution local until both implementations prove
what needs sharing. The source findings, validation limits, map structure,
and revised gates are in the review. Earlier interface sketches in §3/§5
are candidates to test, not settled interfaces.

## 0. Summary

**Problem.** AutoStory programs (play a game's story from a new save,
unattended) are being written one game at a time. SV has one on `main`
(22.4k lines). A team is building BDSP
(PokemonAutomation/Arduino-Source#1717, 32.7k lines, WIP), SwSh has an
early draft (PokemonAutomation/Arduino-Source#1207), and we want one for
FireRed. Each copies or reinvents the same two things:

- **The run flow:** an ordered list of checkpoints, each starting from a
  save, retried from that save when it fails, with start/end selection,
  stats and notifications.
- **Walking on a grid** (FRLG and BDSP): following a path, and getting back
  on it after a trainer, a wild Pokémon or a dialog interrupts the walk.

**Situation.**

- BDSP copied SV's run flow almost name for name, and the copies have
  already drifted: different retry limits (SV allows 100 but runs 102, BDSP
  20), error reporting and day skip only in SV, BDSP's backup reload still a
  TODO. SwSh does not use the framework at all. None of them has unit tests
  of the flow.
- Neither SV, BDSP nor the existing FRLG code can tell which step of a path
  it was on when something interrupted it. BDSP's later segments push the
  player back by hard-coded amounts per trainer, keyed by strings; one of
  those pushes never runs because the names do not match. Its earlier
  segments have a good image-based landmark alignment loop that the rest of
  the PR does not use.
- FRLG on `main` has most of the low-level pieces (detectors, fly, heal,
  a tested wild-battle exit, a frame-exact tile walker inside the Elite Four
  farmer) but nothing that plays the story.
- The pokefirered decompilation gives exact positions and sight ranges of
  every trainer, so FRLG paths can be planned offline instead of tuned by
  hand.
- It has been done before outside this project: a Poke-Controller script
  (`FRLG_Story_Auto.py`, [junari's blog](https://junari.hatenablog.com/entry/2024/08/02/234339))
  plays FRLG to the Hall of Fame with the starter alone in about 12–14
  hours, on a GameCube Game Boy Player or a Switch, resetting to the last
  save on failure. Users asked for the same here
  ([ComputerControl#259](https://github.com/PokemonAutomation/ComputerControl/issues/259));
  Mysticial's reply there is that an AutoStory takes far more effort than it
  looks (SV took over a year).

**What that year was, honestly.** Most of it was hardware hours per
checkpoint: writing a checkpoint, then tuning it and rerunning it on a
console until it is reliable. Shared infrastructure does not shorten that.
Decomp-planned paths might reduce the tuning, but that is unproven. What we
can do is measure the cost per checkpoint early and stop if it is too high
(the gate in §7.7).

**Order (revised 2026-10-25): own something before proposing changes to
anyone else's code.** Revised again 2026-10-27: new code goes in its final
home from its first PR (§0.1). Adding new files to a shared module changes no
one else's code, so "FRLG files only" bought nothing, and "write it in FRLG,
move it out later" was a plan for move-only PRs of our own code.

1. **Phase A, the pilot: about 10 PRs, touching only new files and FRLG.**
   Build a FireRed AutoStory from a new game to Brock. The run flow (S0)
   and the tile-path executor (T1) are written in `Pokemon/AutoStory/` and
   `Pokemon/Navigation/` from the start, used only by FRLG; SV and BDSP are
   not touched. The Elite Four farmer moves onto the tile path (T2). Users
   can run something at PR 5 (new game → rival battle); the pilot is
   complete at PR 10 (Boulder Badge).
2. **Gate F3**, a public commitment in the proposal: every pilot checkpoint
   passes 3 times in a row from its save, at an average of at most 3 Switch
   hours per checkpoint, counting development and validation. Otherwise we
   stop at Brock.
3. **Phase B:** the remaining segments to the Champion, one PR per segment.
4. **Phase C, any time and independent of A and B:** the BDSP bugs found
   while reading #1717 become standalone review comments with no request
   attached. They are drafted in the sibling file and not posted; the user
   decides.
5. **Phase D, only after the Brock pilot runs and people use it:** "FRLG
   runs on the shared core; I'll migrate SV and offer it to BDSP." Nothing
   of ours moves: phase D changes SV's code to use the shared core (R1–R5),
   and BDSP gets an offer.

Requirements unchanged (§9): FireRed, to the Champion, solo with no trades,
shortcuts as options, the team worked out backwards from what the route
needs.

**What reviewers should expect:** about 10 PRs to the pilot, most 150–500
lines, in the order of §7.7, then a go/no-go decision with measured
numbers. The likely reviewers are the FRLG authors in §8, plus Mysticial
for the new shared folders.

The rest of the doc gives the code layout (§0.1), the evidence (§2, §4),
the interfaces (§3, §5), the BDSP findings (§6), the FireRed plan and PR
sequence (§7), and who wrote the FRLG code we build on (§8).

### 0.1 Code layout: reusable pieces and their homes (2026-10-27)

Four AutoStory efforts (SV on `main`, BDSP #1717, SwSh #1207, our FRLG
plan) and our BBQ and BDSP lab work keep rebuilding the same pieces. SwSh
#1207 combines timed routes with limited transition, battle-menu and catch
feedback, but lacks verified checkpoint recovery; running BDSP's intro on a
console showed why that breaks (dropped inputs, menus opened by a stray A,
loops that never end). The pieces below are game-neutral and get their
final home in the PR that first adds them, even while one game uses them.

| Piece | What it is | Home | First PR |
|---|---|---|---|
| Screen machine | Closed loop over known screens: each state has a detector, one step, the screens it may lead to, a visit limit and an in-a-row limit; a screen counts once two frames agree; fails on an unexpected screen, a stall (8 steps with no change) or a timeout. Replaces mash-for-N-seconds. | `CommonTools/` (templated on the controller context, like `run_until`) | BDSP new-game intro (lab), then FRLG D4 |
| Step trace | Logs each recognized screen and action, saves a screenshot when the screen changes, and fails with the last screens attached. | `CommonTools/` | same |
| Failure policy | On a failed operation: keep the error screenshot, go HOME, close the game, rethrow, so every run ends in a known state. | `NintendoSwitch/Programs/` (next to `GameEntry`) | same |
| List selection | Read the selected row, move until the target row, confirm, verify the result. Readers are per game (BDSP name list, FRLG start menu, SV menus). | routine in `CommonTools/`; readers per game | same |
| System keyboard | Detector, cursor finder, `type_on_system_keyboard`. | `NintendoSwitch/Inference/` | lab Lab Console PR |
| Lab Console | Probe and debug runner (command files, per-game detector registry, `detect name image` offline). | `NintendoSwitch/Programs/LabConsole/` | lab Lab Console PR |
| Run flow | Checkpoint list, save, retry from save, start/end range, stats (§3). | `Pokemon/AutoStory/` | fork #43–#46 / S0 |
| Tile path | Path data, interrupt and resume, landmark alignment (§5). | `Pokemon/Navigation/` (console-free) | T1 |
| Decision tests | Game steps injected as fakes (`WildBattleExitActions`, the BBQ Tera tests). | a test file per program, from its first PR | every program PR |

Per game, deliberately not shared: detectors and templates, timings and
speeds, battle UI, menus, new-game screens, path data.

Rules that follow (also in the PR workflow): no "copy now, move later"
step in any plan; tests go into the program's own test file in the PR that
adds them; a seam PR sets the interface later PRs need, and behaviour PRs
change policy, not signatures.

## 1. Design from first principles

Written before reading SV or BDSP; the rest of the doc checks it against them.

1. **Checkpoint = story between two saves.** Precondition and postcondition
   are observable states ("standing in Pewter Center, party healed"). On any
   failure: reset to the last save and retry, up to a limit.
2. **Program = ordered checkpoint list.** User picks start and end; the save
   file is the resume state.
3. **Checkpoints are short compositions of a small step vocabulary:** walk a
   path, interact, clear dialog to a stop state, battle, menu choice, shop,
   heal, fly, save. Each step names the screen it ends on and waits for it
   with a timeout.
4. **Movement is the game-specific layer.** Tile games (FRLG, BDSP d-pad) can
   walk by count; 3D (SV) aligns a camera to markers. They should not share
   code.
5. **One battle handler** with policies (trainer: chosen move; wild: flee or
   repel; prompts: evolution, move learn, faint switch; blackout: fail the
   checkpoint).
6. **Game actions behind interfaces** so the runner, the path executor and
   battle decisions get unit tests with scripted fakes, like
   `WildBattleExitActions` (`FRLG/PokemonFRLG_Navigation.h:88-127`).

SV and BDSP match 1–2 closely, partially do 3 (helpers, not a vocabulary),
and do 5–6 weakly (no tests of the flow; BDSP's smarter battle code is
unused). 4 is where BDSP split into two styles.

## 2. Run flow: what exists

| Element | SV (`main`) | BDSP #1717 | Verdict |
|---|---|---|---|
| Segment interface | `name/start_text/end_text/run_segment(env, ctx, AutoStoryOptions, AutoStoryStats&)` `SV/PokemonSV_AutoStoryTools.h:96-107` | identical `BDSP/PokemonBDSP_AutoStoryTools.h:126-138` | same |
| Checkpoint interface | same + `checkpoint_text()` `SVT.h:109-123` | no `checkpoint_text()` `BT.h:140-152` | same |
| Registries | function-local statics, public `ALL_AUTO_STORY_SEGMENT_LIST()` `SV/PokemonSV_AutoStory.cpp:146`, `..._CHECKPOINT_LIST()` `:321` | file-static `ALL_SEGMENTS()/ALL_CHECKPOINTS()` `BA.cpp:51,245` | renamed |
| Select databases | 6 near-duplicate `make_*_database` `SVA.cpp:152-198,326-372` | one `section_database(bool,bool)` `BA.cpp:257-283` | BDSP's is better |
| Index math | `.index() + (LAST_TUTORIAL+1)` `SVA.cpp:1102-1168` | `stoul(slug())` `BA.cpp:735-750` | same result |
| Tutorial split | hard-coded 9 / 20 `SVA.cpp:93-94` | segment 0, 6 checkpoints `BA.cpp:256` | same idea |
| start > end | `UserSetupError` `SVA.cpp:1304-1313` | none (loop clamps) | BDSP missing |
| Options | `{language, starter_choice, notif_status_update&, notif_error_recoverable&}` `SVT.h:89-94` | `{language, starter, notif_status_update&, help_mode, outside_help, save_mode, retry_scope}` `BT.h:111-119` | per game |
| Stats | Checkpoint / Segment / Reset `SVT.h:27-40` | same + non-atomic `m_first_run` `BT.h:50` | same keys |
| Save | `save_game_from_overworld` via `checkpoint_save` `SVT.cpp:1180` | verified save, 3 retries, blind fallback; FAKE diagnostic mode `BT.cpp:75-185` | per game |
| Reset | `reset_game(info, console, ctx)` | `reset_game_from_home(...)`, throws on false `BT.cpp:236-243` | per game |
| Day skip on retry | yes, if enabled `SVT.cpp:1443-1446` | no | SV only |
| Retry limit | `i > 100` → 102 attempts (off by one) `SVT.cpp:1436,1471` | 20, rethrows `BT.cpp:210,230` | diverged |
| Error report | at attempt 10, telemetry `SVT.cpp:1457-1469` | none | SV only |
| Post-success check | `enter_menu_from_overworld` (retry if not overworld) `SVT.cpp:1453` | none | SV only |
| Tutorial loop | separate copy `SVT.cpp:1491-1547` | — | SV only |
| `save=false` path | — | the retry loop calls `reload_backup_save` (`BT.cpp:245-246`), an empty TODO (`:188-197`), and reruns the action without resetting | bug |
| Dev harness | `test_checkpoints` / `test_code` `SVA.cpp:901-1080,1196-1291` | none | SV only |
| End of program | `GO_HOME_WHEN_DONE`, start-up checks | neither; language hard-coded English `BA.cpp:701` | BDSP missing |

Coupling worth knowing before moving anything:

- SV's segment list is used outside AutoStory:
  `ClaimMysteryGift::run_autostory_until_pokeportal_unlocked` runs segments
  0..6 (`SV/../Farming/PokemonSV_ClaimMysteryGift.cpp:371-382`). Keep the
  list and `AutoStoryOptions` exported.
- `ML/Inference/ML_YOLONavigation.cpp:20,87` includes SV AutoStoryTools: a
  layering inversion to leave alone in this work, but not to copy.
- Segments construct their checkpoints inline
  (`AutoStory_Checkpoint_04().run_checkpoint(...)`), so checkpoint order is
  written twice: in each segment and in the registry. In BDSP this already
  went wrong twice (§6).
- Code-health limits (fork #41: functions ≤50 lines, nesting ≤4, ≤7 params)
  are exceeded by: SV constructor ~397 lines, `test_checkpoints` 180 lines /
  8 params / nesting 5, `test_code` 96, `on_config_value_changed` 87,
  `program` 82, both retry loops (64 and 57 lines, 8 and 7 params), and every
  `checkpoint_NN` (8 params). BDSP: constructor 282 (four copies of the
  14-move list), `program` 74, `save_game` 71, retry loop 51 / 8 params. Pure
  moves keep these as legacy; anything new should not add more.

## 3. Run flow: shared core

The shared core is written in `Pokemon/AutoStory/` in the shape below
(hooks, a policy struct, fake-hook unit tests) by the first PR that needs
it: fork #46 already does this for SV, and S0 adds FRLG's hooks on top of
it. FRLG is its first user only if S0 lands before #46; either way the code
is written once, in its final place (§0.1). SV changes only in phase D,
BDSP not at all.

### 3.1 What is shared, what is per game

Shared, in a new `SerialPrograms/Source/Pokemon/AutoStory/` (that module has
no `NintendoSwitch/` includes today; keep it that way):

- `run_checkpoint_with_retries(hooks, policy, checkpoint_text, action)`: the
  loop only.
- `CheckpointRetryPolicy{max_attempts, report_at_attempt, ...}`.
- `AutoStoryStats` (same three keys; persisted stats depend on the strings).
- `make_index_database(list, begin, end)` and `to_global_range(...)`: the
  select/slug/index math.
- Optionally `AutoStorySegmentT<Options, Stats>` / `CheckpointT<...>` so a
  game declares `using AutoStory_Segment = ...<SVOptions, AutoStoryStats>`.

Per game: Options struct, save/reset/day-skip, post-success check, the
start-up checks, the registries, and every checkpoint.

```cpp
//  Pokemon/AutoStory/Pokemon_AutoStoryRetry.h
namespace PokemonAutomation{
namespace Pokemon{

struct CheckpointRetryPolicy{
    size_t max_attempts;        //  SV 100, BDSP 20
    bool fatal_when_greater;    //  SV: i > max (keeps today's off-by-one)
    size_t report_at_attempt;   //  SV 10; SIZE_MAX = never
};

class CheckpointHooks{
public:
    virtual ~CheckpointHooks() = default;
    virtual void log(const std::string& message) = 0;
    virtual void save_checkpoint() = 0;             //  save, m_checkpoint++, status notification
    virtual void before_attempt(size_t attempt) = 0;    //  SV: day skip + save on retries
    virtual void after_success() = 0;               //  SV: confirm overworld; else no-op
    virtual void report_recoverable(const std::string& message) = 0;
    [[noreturn]] virtual void fail(const OperationFailedException& e, size_t max_attempts) = 0;
    virtual void reset_for_retry() = 0;             //  reset, m_reset++, update stats
};

void run_checkpoint_with_retries(
    CheckpointHooks& hooks,
    const CheckpointRetryPolicy& policy,
    const std::string& checkpoint_text,
    const std::function<void(size_t attempt)>& action
);

}
}
```

SV keeps `checkpoint_reattempt_loop(...)` and `..._tutorial(...)` with their
current signatures as thin wrappers that build `SVCheckpointHooks` /
`SVTutorialCheckpointHooks`, so none of the ~102 call sites in 40 segment
files change.

### 3.2 Behaviours the move must keep (SV)

- Save on attempt 0 happens inside the `try`; if the save throws, later
  attempts do not save again.
- `i > max_attempts` fires the fatal error (102 attempts run). Fix later in
  its own behaviour-change PR, not in the move.
- Recoverable report exactly once at attempt 10.
- `wait_for_all_requests()` before the action and before the reset.
- `after_success` throwing (the overworld check) counts as a failed attempt.
- Each loop's own fatal message text.

### 3.3 Phase D PR sequence (after the pilot is in use; SV only, no behaviour change)

P1–P5 here are R1–R5 in §7.7.

| # | PR | What moves | Risk | Review |
|---|---|---|---|---|
| P1 | Shared select databases | 6 `make_*_database` → `make_index_database<T>`; same slugs/labels | low | `--color-moved` |
| P2 | Range math | `get_*_index` arithmetic → pure `to_global_range` + tests | low | tests |
| P3 | Shared stats | `AutoStoryStats` → `Pokemon/AutoStory`; alias in `SVT.h` | low (keys) | pure move |
| P4 | Retry core | hooks + `run_checkpoint_with_retries`; SV loops become adapters; fake-hook tests | medium | loop body moved, adapters new |
| P5 | Dev harness out of the program class | `test_checkpoints` / `test_code` → `PokemonSV_AutoStoryDevTests.cpp`; delete the dead 106-line comment block | low | `--color-moved` |
| P6 (opt.) | Templated segment/checkpoint base | aliases in SV; no segment file changes | low | small |

Tests enabled by P2/P4 (fake hooks recording steps as strings, registered
through a new `Pokemon::add_tests_AutoStory` in
`ComputerPrograms/UnitTestRunner.cpp`, or in `PokemonSV_Tests.cpp` first):
first-try success sequence; save only on attempt 0 (also when the save
throws); day skip only on retries; report exactly once at 10; SV fatal at 101
vs BDSP rethrow after 20; reset exception propagates; non-`OperationFailed`
exceptions are not caught; `after_success` failure retries; range math for
SV (10/21 offsets) and BDSP (1/6); slugs equal global indices.

Separate behaviour-change PRs after the move, each titled as such: fix the
off-by-one; the BDSP-style single `section_database` if P1 did not already
get there; registry-driven segments (a segment = a range of the checkpoint
registry, removing the duplicated order).

## 4. 2D movement: what exists

### 4.1 Primitives

| | FRLG (`main`) | BDSP #1717 |
|---|---|---|
| Grid walker | `WalkSegment{DpadPosition, steps}`, `walk_path` in `FRLG/Programs/Farming/PokemonFRLG_EliteFourFarmer.cpp:395-449`: decomp timings `FRAME_MS=16.74`, `TURN_FRAMES=8`, `STEP_FRAMES=16`; one continuous hold, direction switched mid last step (±8 frames tolerance) | `repeat_dpad` `BDSP/PokemonBDSP_AutoStoryTools.cpp:327-343`: one tap per tile, an extra tap when turning; 211 uses |
| Elsewhere | millisecond holds: `StarterRng.cpp:224-230,413-449`, `PickupFarmer.cpp:142-167`, `EvTrainer.cpp:206-277`, `BlindNavigation.cpp:414-477` (RNG timing budgets), 33 ms turn taps in `grass_spin` `Navigation.cpp:1571-1592` | joystick holds dominate (Seg03: 328 holds, 0 `repeat_dpad`); tile counts only in comments (`1600ms // 8`, `8100ms // 27++ (snow)`) |
| Facing | passed in by caller; not detected | `DpadState::last_dir` |
| Doors | `walk_through_door` (:451): path + 300 ms under `BlackScreenOverWatcher`; `check_room` floor colour | `BlackScreenOverWatcher` everywhere; `walk_until_doorway` `Segment_01.cpp:85-187` (black screen then battle menu = wild battle, else doorway) |
| Arrival | the transition only | transition, or landmark match (00–03) |
| Executor stubs | — | `Utils/PokemonBDSP_AutoStory_Navigation.cpp:19-52`: `walk_steps`, `walk_until_dialog`, `walk_until_battle` are TODO |

FRLG screen geometry: `GAME_BOX` (`FRLG/PokemonFRLG_Settings.cpp:110-115`)
is exactly 6.5× of 240×160 at 1080p; one tile = 104 px; the view is 15×10
tiles with the player in the centre tile.

### 4.2 Interruptions

- **FRLG:** no walking code watches for trainers, wild battles, NPC dialog or
  items (the E4 paths are safe only because the League has none).
  `exit_wild_battle` (`Navigation.cpp:576-660`) is a good, tested state
  machine for leaving a battle, but nothing feeds position back into a path.
- **BDSP:** trainer spotting is watched with `MarkDetector` ("!"), then
  battle, then `move_back_after_unexpected_battle(trainer_id)`, a per-segment
  if-chain of hard-coded pushes (Seg04:188, Seg05:185, Seg06:138, Seg07:203,
  Seg08:134, Seg09:66). `handle_unexpected_battle` (`Utils/..._Battle.cpp:73-100`)
  re-runs the whole action after a battle instead of resuming.
  `recalibrate_after_battle` and `handle_unexpected_battle` are never called
  by segments. Wild battles are mostly avoided with Repel; Repel expiry is a
  `ShortDialogWatcher` interrupt (Seg07:687-698).
- **Neither** reports "which step and tile was I on", handles item pickups or
  generic NPC dialog during a walk, or re-localizes other than by hard-coded
  pushes or a reset.

### 4.3 Landmark alignment (BDSP 00–03)

`match_landmarks` (`Utils/PokemonBDSP_OverworldMatchers.cpp:46-126`):
downscale to 960×540 grey (optional local-contrast or blue-fraction
features), `TM_CCOEFF_NORMED` per landmark in a search box, margin over the
best competitor outside an exclusion radius, multiple landmarks must agree
within ~1/64 × 1/54 of the screen; fine pass then a coarse pass that can
never declare "at target". Lighting variants per hour count as one landmark
(`match_lighting_pair` :309-340), with a lighting sweep test program.

Control loop (`align_to_route204_floaroma_turn`, `Segment_02.cpp:406-463`):
snapshot → if dialog or battle, return interrupted → match → 3 consecutive
at-target hits with 150 ms waits = done → else one-axis joystick nudge of 40
or 100 ms, 250 ms settle → "blocked" if dx moved < 1/384 over 3 moves, switch
axis → throw after 12 frames lost → give up after 60.

This is the strongest movement code in the PR. It also fits FRLG better than
BDSP: no lighting changes, exact pixels, and an offset in game-box fractions
converts directly to whole tiles (`round(dx·15)`, `round(dy·10)`), so FRLG
alignment becomes "measure → emit a `{dir, n}` path → re-measure" instead of
nudging. Sample only while standing still (the view scrolls during a step);
avoid animated tiles; Flash-dark caves are the exception.

## 5. 2D movement: shared design

**Superseded 2026-10-29** by [AutoStory-Navigation-Stack.md](AutoStory-Navigation-Stack.md): a
shared core (`nav/core`: grid types, a leg planner over a per-game step model, a leg walker
with per-game movers and interrupts, the screen machine) with two children, `frlg/brock-mvp`
and `bdsp/floaroma-mvp`. The sketch below is kept for its reasoning (§5.2 still holds).

### 5.1 Pieces

Console-free. It lives in `SerialPrograms/Source/Pokemon/Navigation/`
from T1 on (precedent: `Pokemon/Pokemon_AdvRng.h` is shared by FRLG and
RSE), with FRLG's adapter in the FRLG module. BDSP can adopt it later
without anything moving.

```cpp
namespace PokemonAutomation{
namespace Pokemon{

enum class TileDirection{ UP, DOWN, LEFT, RIGHT };

struct TileStep{
    TileDirection direction;
    uint16_t tiles;
    bool ends_in_warp = false;                      //  door/stairs: expect a fade at the end
    std::optional<uint16_t> trainer_stops_at;       //  sight line triggers after this many tiles
};
using TilePath = std::vector<TileStep>;

enum class WalkInterrupt{ NONE, WILD_BATTLE, TRAINER_BATTLE, DIALOG, ITEM, EARLY_WARP, NO_WARP, BLOCKED };

struct StepOutcome{
    WalkInterrupt reason = WalkInterrupt::NONE;
    std::chrono::milliseconds walked{0};            //  input start -> detection frame time
};

struct PathRunResult{
    bool completed = false;
    WalkInterrupt last_reason = WalkInterrupt::NONE;
    size_t step = 0;                                //  where it stopped
    uint16_t tiles_done_in_step = 0;
    bool tiles_exact = false;
    size_t interrupts = 0;
};

class TileMovementActions{
public:
    virtual ~TileMovementActions() = default;
    virtual StepOutcome walk(std::optional<TileDirection> facing, const TileStep& step) = 0;
    virtual uint16_t estimate_tiles(const TileStep& step, std::chrono::milliseconds walked) const = 0;
    virtual bool resolve(WalkInterrupt reason) = 0;                 //  battle/dialog handled, back in overworld
    virtual std::optional<uint16_t> relocalize(const TileStep& step) = 0;   //  landmark fix, if any
};

class InterruptPolicy{
public:
    enum class Decision{ RESUME, RESTART_PATH, ABORT };
    virtual ~InterruptPolicy() = default;
    virtual Decision on_interrupt(const PathRunResult& state) = 0;
};

PathRunResult run_tile_path(
    Logger& logger,
    TileMovementActions& actions,
    InterruptPolicy& policy,
    std::optional<TileDirection> facing,
    const TilePath& path,
    size_t max_interrupts = 10
);

//  Alignment: BDSP nudges the joystick, FRLG emits whole tiles.
struct LandmarkFix{ bool located = false, at_target = false, coarse = false; double dx = 0, dy = 0; };
class AlignActions{
public:
    virtual ~AlignActions() = default;
    virtual std::optional<LandmarkFix> observe() = 0;               //  nullopt = interrupted
    virtual void move_toward(double dx, double dy) = 0;
    virtual void settle() = 0;
};
struct AlignConfig{ size_t max_attempts = 60, confirmations = 3, max_missing = 12; double blocked_epsilon = 1.0/384; };
enum class AlignResult{ ALIGNED, INTERRUPTED, LOST, DID_NOT_CONVERGE };
AlignResult align_to_landmark(Logger& logger, AlignActions& actions, const AlignConfig& config);

}
}
```

Per game (adapters in the game module, so dependencies point
VideoGames → Pokemon): frame timings and speeds (walk, run, bike, surf,
snow), how a step is sent (FRLG one continuous d-pad hold; BDSP d-pad taps),
detectors (black screen box, battle start, dialog, "!" mark, item jingle),
landmark matchers and templates, field moves, battle resolution. Do **not**
share timing constants: two engines are not evidence for a common model.

### 5.2 Getting back on the path after an interrupt

Options for "how many tiles of the interrupted step were done":

- **A. Time estimate** `floor((t_detect − t_start − turn) / step)`, using the
  detecting frame's timestamp and starting the clock only after
  `wait_for_all_requests()`. ±1 tile from emulator/capture latency.
- **B. Path data.** A trainer's sight line triggers on a fixed tile, so the
  step can declare `trainer_stops_at`. Exact. This is what BDSP's hard-coded
  pushes encode, without a schema, and keyed by strings: `Segment_07.cpp:214`
  matches `"route_215_skier_edward"` but the only caller (`:773`) passes
  `"route_216_skier_edward"`, so the 8.1 s push never runs.
- **C. Landmark fix** (§4.3). Exact where templates exist.
- **D. Bonk anchor.** Walk into a known wall, counter or door until blocked
  or warped; overshoot is harmless. Already how Pokécenter entry works.

Recommendation: B when the step declares it; otherwise A, marked inexact, and
end with C or D before anything that needs an exact tile (doors beside walls,
interacting, the next trainer). Never chain two inexact estimates.

### 5.3 Tests (no console)

Following `ScriptedWildBattleExit` (`FRLG/PokemonFRLG_Tests.cpp:46-166`):
executor bookkeeping (step/tile indices, splitting the remainder after an
interrupt, early warp vs missing warp, `max_interrupts`, RESUME vs
RESTART_PATH vs ABORT); FRLG hold-duration maths (turn cost, mid-step switch)
if computed by a pure function; `estimate_tiles` boundaries; the alignment
control law fed scripted `LandmarkFix` sequences (converging, blocked, lost
13 frames, interrupted). Matchers themselves: screenshot tests in
CommandLineTests, one PR per detector (Mysticial's request on #1717).

### 5.4 Risks

- NSO GBA runs ~59.73 Hz inside a 60 Hz output; long holds drift. Split long
  paths at anchors.
- Input and capture latency hurt option A; dropped 33–40 ms taps (use ≥80 ms
  or continuous holds).
- BDSP joystick moves are not grid-locked: snap back to the grid (d-pad or a
  wall bump) after any joystick alignment.
- Variable speed tiles and forced movement (snow, bike, ledges = 2 tiles,
  spin tiles, currents), wandering NPCs blocking or pushing.
- Battle intro and door fade both look like a black screen: check for a
  battle menu after the fade (BDSP `walk_until_doorway` does; FRLG must).

## 6. BDSP #1717: findings (phase C)

**Revised 2026-10-25.** We do not own anything yet, so we don't ask the BDSP
team to restructure their WIP, adopt a shared core, or reorder their PRs.
The defects below become standalone review comments with no request
attached, drafted in
[AutoStory-1717-review-drafts.md](AutoStory-1717-review-drafts.md) and
re-checked at `65094d4cd`. The user decides whether and when to post them.
Design opinions stay in this internal doc until phase D, and come up
earlier only if someone asks.

They asked for feedback on "the SV-derived architecture, checkpoint and
recovery contracts, BDSP helper/detector boundaries, resource distribution,
and the order in which to extract focused PRs".

**Defects (drafted as comments 1–5):**

- `reload_backup_save` is an empty TODO (`BT.cpp:188-197`), but the retry
  loop calls it when `save` is false (`:245-246`) and then reruns the action
  without any reset.
- Checkpoint order is written twice (segment + registry) and has drifted:
  - Segment 08's `run_segment` stops at 0821 (`Segment_08.cpp:116`), while
    the registry goes on to 0826 (`BA.cpp:181-185`), so 0822–0826 (Sunyshore
    and the 8th gym) never run in segment mode.
  - 0717 is missing from the checkpoint list (`BA.cpp:155-156` goes 0716 →
    0718).
- `Segment_07.cpp:214` matches `"route_215_skier_edward"`, but the only
  caller (`:773`) passes `"route_216_skier_edward"`, so the 8.1 s push never
  runs.
- Badge texts swapped: Segment 04 (`:31`) says "Third Gym Badge acquired
  (Hearthome City)", Segment 05 (`:30`) "Fifth Gym Badge acquired (Veilstone
  City)"; Veilstone is the 3rd, Hearthome the 5th.

**Question (draft 6, optional):** `handle_battle` is copied into each of
segments 04–09, keyed by trainer-id strings, while `Battle.cpp`'s
`handle_expert_battle` (`:698`) and `handle_battle_with_outside_help`
(`:208`) have no callers.

**Held back (design opinions, for phase D or if asked):**

- A shared run-flow core (§3) instead of per-game copies; their
  `section_database` is cleaner than SV's six copies and would be the model.
- No start > end check, no recoverable-error report, no `GO_HOME_WHEN_DONE`,
  language fixed to English (`BA.cpp:701`), diagnostic options marked
  "remove before production" (`BA.h:42-46`). These are normal for a WIP
  and not worth a comment.
- Movement: path data with `trainer_stops_at` (§5.2) instead of
  per-segment `move_back_after_unexpected_battle` chains; the landmark loop
  (segments 00–03) as a shared alignment routine (§5.1).
- Changes to shared `DialogDetector` boxes affect every BDSP program and
  need screenshot tests; Mysticial has already asked for detectors first,
  one PR each.
- Templates live only in their Packages fork, and per-hour lighting variants
  multiply them.

## 7. FRLG AutoStory plan

### 7.0 Prior art: junari's FRLG script

From the [blog post](https://junari.hatenablog.com/entry/2024/08/02/234339)
(Poke-Controller MODIFIED, Python; we have not read its code):

- **Scope and time:** new game to the Network Machine, Silph Co., the
  ending or the Hall of Fame (selectable stop point); about 12–14 hours to
  the end.
- **Team:** the starter alone fights every trainer. Squirtle is fastest,
  then Charmander, then Bulbasaur. Moves come from TMs and tutors: Squirtle
  Mega Kick + Water Pulse; Charmander Mega Kick, Dig, Dragon Claw; Bulbasaur
  Return, Earthquake. No transfers from another save. This confirms the
  solo rule (§9) is workable and shrinks the requirements plan (§7.6) to HM
  users, TMs and tutors, and money.
- **Recovery:** on a lost battle or a loop running too long, reset and
  restart from the nearest save.
- **Options:** start/stop number, version, hardware, language, gender,
  player/rival names, starter, fossil, Dojo Pokémon, and a "Nugget farm"
  mode (lose to the Nugget Bridge Rocket repeatedly, 1.5–2 min each), the
  same trick as our `NuggetBridgeFarmer`.
- **Setup it requires:** delete the save from the title screen (Y + B + Up
  on Switch), 720p output, screen size 100%.
- **Images:** none shipped; it crops reference images at run time on the
  first run and reuses them.

Its terms forbid redistribution and claiming authorship, so it is a
reference for the route, team and options, not a source of code. Its demo
videos are useful for checking routes.

### 7.1 Reuse

Already on `main` (`FRLG/`):

- Detectors: dialogs, battle menu/dialogs/learn/level-up/out-of-PP, party
  and summary, start menu, bag, load menu (untested on a new game),
  selection arrows, faint watchers, fly map, E4 rooms, Hall of Fame.
- Navigation: `fly_from_kanto_map` (13 towns), `heal_at_pokecenter`,
  `enter_leave_pokecenter`, `use_teleport_from_overworld`,
  `open_party_menu_from_overworld`, `save_game_to_overworld`, `soft_reset`.
- Battle: tested `exit_wild_battle`; `spam_first_move`;
  `EliteFourFarmer::run_battle` (OCR the opponent, move plan per opponent,
  out-of-PP) as the model for a trainer battle handler.
- Story fragments: StarterRng's `walk_to_rival_battle`,
  `walk_to_route1_from_lab`, `auto_battle_rival`; SidHelper's
  `finish_intro_animations`.
- **Money:** `NuggetBridgeFarmer` already loops the Nugget Bridge event
  (loses the battle on purpose, ~72 s per Nugget by its "100 ≈ 2 hours"
  default, Nugget sells for 5,000, so ≈250k/hour). Wrap it as an optional
  "money top-up" checkpoint after Nugget Bridge (or any time later, by flying
  to Cerulean), with a target amount.
- `ItemDuplication` (Retro Mail glitch; needs Farfetch'd from the Vermilion
  trade): optional way to duplicate Rare Candies instead of grinding. Keep
  it out of the default route; offer it as a config.

### 7.2 Missing pieces (infrastructure, ~3–5k lines)

1. The run flow (§3), shared core in `Pokemon/AutoStory/`; FRLG hooks
   (save via start menu, reset via `soft_reset` + load menu), options,
   registries in the FRLG module.
2. Tile path (§5) in `Pokemon/Navigation/`, the FRLG adapter in the FRLG
   module, moving `walk_path` and the constants out of
   `EliteFourFarmer.cpp` (thewhitewolfking's code, not ours); add run
   (8 frames/tile), bike (6), surf (8) per decomp; ledge steps.
3. Overworld detector (overworld vs dialog vs battle vs menu without opening
   the start menu) and facing detection (player sprite in the centre tile).
4. Walk interrupts: trainer "!" or approach, wild battle (black screen +
   battle menu), item pickup jingle/dialog, NPC dialog; resume via §5.2.
5. Trainer battle handler: move choice (per-checkpoint plan first, type
   table later), "will you switch?" (avoid by Battle Style = Set), next
   Pokémon after a faint, move learn (skip or replace per config),
   evolution (allow/cancel per config), whiteout → fail the checkpoint.
6. New game: intro, naming keyboard (preset names avoid it), options menu
   (text Fast, animations Off, style Set), load menu on a fresh save. Built
   on the screen machine and step trace (§0.1), as the BDSP intro is in the
   lab: every input follows a screen read, no mash-for-N-seconds.
7. Field moves: teach HM from bag, use Cut/Surf/Strength from the party
   menu, Fly (exists), Bike on/off; puzzles below.
8. Shop and PC: buy list per Mart, deposit/withdraw for HM carriers.
9. Catch routine and the requirements planner (§7.6).

English only (decision 5), so text detectors and OCR need one language.

### 7.3 Route data

The decomp (`data/maps/<Map>/map.json`) gives every object event with x, y,
movement type, `trainer_type`, sight range and script, plus warps, coord
events and signs; `data/layouts/<L>/map.bin` gives metatiles and collision
(behaviours such as ledges, spin tiles and currents come from tileset
attributes). That is enough to:

- plan paths offline with a small planner over collision + ledges (prior
  art: `40Cakes/pokebot-gen3` `modules/map_path.py`, GPL-3.0, which plans on
  live RAM; we would run the same idea over the decomp files and **emit
  reviewed C++ `TilePath` literals**, not ship the planner);
- mark each trainer's sight line on the path so `trainer_stops_at` is
  computed, not guessed;
- simulate spin tiles (Rocket Hideout) and plan Strength boulder pushes
  (Seafoam optional; Victory Road mandatory).

Online maps (Bulbapedia route pages, PokeTools) list trainers but not exact
positions; the decomp is the source of truth.

### 7.4 Segments (draft)

| Seg | Content | Notes |
|---|---|---|
| 00 | New game → starter → lab rival | names, options, starter config |
| 01 | Parcel → Pokédex → Viridian Forest → Brock | Route 22 rival optional |
| 02 | Route 3 → Mt. Moon (fossil) → Misty → Nugget Bridge → Bill (S.S. Ticket) | money top-up available from here |
| 03 | S.S. Anne (Cut) → Surge; VS Seeker; Bike (opt.) | HM carrier for Cut |
| 04 | Route 9/10 → Rock Tunnel → Lavender → Celadon → Erika; Tea | Flash optional |
| 05 | Rocket Hideout (spin tiles, Lift Key, Silph Scope) → Pokémon Tower → Poké Flute | |
| 06 | Snorlax (Route 12 or 16) → Fuchsia → Koga → Safari (Surf, Strength) | Safari is a step-limited zone |
| 07 | Saffron: Silph Co. (Card Key, warp pads, rival, Giovanni) → Sabrina | Lapras optional |
| 08 | To Cinnabar (Route 21 by default) → Mansion (Secret Key) → Blaine | Seafoam optional; decline One Island |
| 09 | Giovanni (Viridian) → Route 22 rival → Victory Road (Strength boulders) | |
| 10 | Elite Four + Champion | reuse `EliteFourFarmer` pieces |
| 11 | Sevii post-game: Ruby (Mt. Ember), Sapphire (Dotted Hole), Network Machine | added 2026-10-29 (§9.2) |
| 12 | Mew | method to settle (§9.2); likely needs the naming typer |
| opt | Money top-up, leveling loop, gift Pokémon, fossils | config-driven, outside the main order |

Estimate ~90–110 checkpoints. Hardest: 00 (keyboard, intro), 05 (spin
tiles), 07 (warp pads), 09 (boulders), and leveling before each gym.

### 7.5 Config (in game order)

Player/rival names (presets by default); options (forced by the program);
starter; nickname prompts (always no); Magikarp (skip); Mt. Moon fossil
(Dome/Helix); Old Amber (skip); Bike (yes: shortens Cycling Road and
travel); Flash (skip); Eevee and stone (skip); Game Corner prizes (skip);
Snorlax route (12 or 16) and catch or defeat; Hitmonlee/Hitmonchan; Lapras
(yes: Surf carrier); Sevii trip (decline); optional legendaries (skip); HM
carriers (Cut/Surf/Strength: default to gifts and route catches); shop list
per Mart; move learning (keep starter's best moves per a table); money
target for the top-up; leveling targets per gym; Rare Candy duplication
(off).

Facts to design around (decomp): HM field use needs Cascade (Cut), Rainbow
(Strength), Soul (Surf); Flash/Fly/Rock Smash are optional for Kanto; Rock
Tunnel's darkness is cosmetic for movement; Saffron guards want Tea; the
Viridian Gym opens after badges 2–7; blacking out costs level × 4 × a
badge-based multiplier, not half the money.

### 7.6 Working backwards: the requirements plan

The team, catches and purchases follow from what the route and the chosen
options require, each with a deadline. Write this down as data, one row per
requirement, and derive the catch/gift/shop checkpoints from it:

| Requirement | Needed by | Providers (pick per version and options) |
|---|---|---|
| Cut user (Cascade badge) | Vermilion Gym tree (seg 03) | Oddish (Routes 24/25; FireRed exclusive, Bellsprout in LeafGreen), Paras (Mt. Moon), Farfetch'd (Vermilion in-game trade, option only) |
| Surf user (Soul badge) | Cinnabar (seg 08) | Lapras (Silph Co. 7F gift, seg 07), a Water starter, Seel/Psyduck etc. |
| Strength user (Rainbow badge) | Victory Road (seg 09) | Machop line, Snorlax, Lapras |
| Battle strength per gym | each gym and the E4 | the starter alone, levelled by every trainer on the way (proven by junari's script, §7.0); TMs and tutors (e.g. Water Pulse from Misty, Mega Kick tutor, Dig, Dragon Claw, Return, Earthquake); leveling loops or Rare Candies (duplication option) as a fallback |
| Money for each Mart list | per Mart | trainer prize money; Nugget Bridge top-up option |
| Retro Mail duplication (option) | when enabled | Farfetch'd from the Vermilion trade (needs the Pokémon it asks for), Retro Mail, the item to copy; see `ItemDuplication` |
| Nugget Bridge top-up (option) | after seg 02 | party able to lose on purpose and heal; `NuggetBridgeFarmer` preconditions |

Consequences:

- A **catch routine** is part of the infrastructure (weaken without
  fainting, throw until caught, handle box-full and nickname prompts), plus
  the encounter detector to know what appeared (`WildEncounterReader`
  exists).
- **HM teaching** and party order (who leads, who carries HMs) are steps
  derived from the table, not written by hand in each segment.
- A small **planner** checks a chosen config before the run: every
  requirement has a provider before its deadline, the money plan covers the
  Mart lists, version exclusives exist in the chosen version. A config that
  fails is a `UserSetupError` at start, not a failure hours in.
- The planner and the table are pure logic, so they get unit tests.

### 7.7 Earlier PR inventory (ordering superseded by the map-reuse review)

**2026-10-29:** T1 and T2 below become N1–N3 on `nav/core` and the FRLG child
`frlg/brock-mvp` ([AutoStory-Navigation-Stack.md](AutoStory-Navigation-Stack.md) §4). The
Elite Four farmer moves onto the walker later, not as part of the pilot. The rest of the table
stands.

Reviewers should know up front what is coming. Phase A is about 10 PRs,
touches only FRLG files, and produces something users can run at PR 5.
Most PRs are 150–500 changed lines, each does one thing, and each says what
to read first. Upstream allows two open PRs from us at a time, so they arrive
in order; the rest wait as drafts on the fork.

**Phase A: pilot, new game → Brock (new files and FRLG only)**

| # | ID | PR | Depends on | Kind | Size (lines) | What the reviewer reads |
|---|---|---|---|---|---|---|
| 1 | T1 | Tile-path executor + interrupt policy + tests, in `Pokemon/Navigation/` | — | new | ~500 (half tests) | result types, resume bookkeeping |
| 2 | T2 | FRLG adapter; Elite Four farmer uses it | T1 | move + new | ~300 | walker moved out of `EliteFourFarmer.cpp`; farmer unchanged on hardware |
| 3 | D4 | Screen machine + step trace (`CommonTools/`), then FRLG new-game screens and naming on it | — | new | ~400 + images | machine tests with fake screens; detector + screenshot tests |
| 4 | S0 | Program shell: options, FRLG hooks on the shared run flow (`Pokemon/AutoStory/`, + tests), registries | — | new | ~500 (a third of it tests) | retry behaviour against fake hooks; options |
| 5 | S1 | Segment 00: new game → starter → lab rival | S0, D4, T2 | new + path data | ~500 | **first runnable**; checkpoint list, paths as data; hardware runs |
| 6 | D1 | Overworld detector | — | new | ~150 + images | detector + screenshot tests |
| 7 | D3 | Trainer spotting / item pickup watchers | — | new | ~150 + images | same |
| 8 | B1 | Walk interrupts for FRLG (battle, dialog, item) | T2, D1, D3 | new | ~300 | resolve() paths; fakes |
| 9 | B2 | Trainer battle handler + tests | — | new | ~500 (half tests) | decisions with scripted fakes |
| 10 | S2 | Segment 01: parcel → Pokédex → Viridian Forest → Brock | S1, B1, B2 | new + path data | ~700 | **pilot complete**; hardware runs of each checkpoint |

The pilot does not need the facing detector (D2): path data knows the
facing after each step, as the E4 farmer does today. The requirements
planner (§7.6) is not needed either, because the pilot route has no HM,
shop or catch requirements. S1 reuses StarterRng's `walk_to_rival_battle` /
`auto_battle_rival` and SidHelper's `finish_intro_animations` where they
fit.

**Phase B: to the Champion (only if gate F3 passes)**

| ID | PR | Depends on | Kind | Size (lines) |
|---|---|---|---|---|
| D2 | Facing detector | D1 | new | ~150 + images |
| T3 | Landmark alignment loop + tests (FRLG; whole-tile output) | T1 | new | ~300 |
| B3 | Field moves: teach HM, Cut/Surf/Strength, Bike | D1 | new | ~300 |
| B4 | Mart purchases | — | new | ~250 |
| B5 | Catch routine | D1 | new | ~300 |
| S0b | Requirements plan + start-up check (§7.6) + tests | S0 | new | ~400 |
| S3–S11 | One PR per segment (02 … 10) | S2, B3–B5, S0b | new + path data | ~500–900 each |
| opt | Nugget Bridge top-up, Retro Mail duplication | S0b | new | small, one each |

**Phase C: BDSP findings (independent, any time).** Not PRs: the review
comment drafts in
[AutoStory-1717-review-drafts.md](AutoStory-1717-review-drafts.md), for the
user to post or not.

**Phase D: SV onto the shared code (after the pilot runs and people use
it).** Framing: "FRLG runs on the shared core; I'll migrate SV and offer it
to BDSP." Nothing of ours moves in phase D.

| ID | PR | Depends on | Kind | Size (lines) | What the reviewer reads |
|---|---|---|---|---|---|
| R1 | Shared start/end select databases (SV) | — | move | ~150 | `--color-moved`; same slugs and labels |
| R2 | Story range maths + tests | R1 | move + tests | ~200 | the pure function and its table test |
| R3 | SV `AutoStoryStats` onto the shared one | — | move | ~80 | stat keys unchanged |
| R4 | SV retry loops become adapters over the shared core | R3 | move + new | ~400 (half tests) | §3.2 list of kept behaviours |
| R5 | Dev harness out of the SV program class | — | move | ~300, mostly a deleted dead block | `--color-moved` |

R1–R5 touch SV AutoStory, which is someone else's merged program: pure
moves first, behaviour changes separately (§3.3). The SV AutoStory author
and Mysticial are the reviewers for these (§8). The BDSP team gets an offer,
not a request.

Notes:

- Segment PRs are mostly path data and short checkpoint functions. Paths
  are planned offline from the decomp; the planner stays outside the repo
  unless reviewers want it under `devtools/`.
- Optional features (Nugget Bridge top-up, Retro Mail duplication) come as
  separate small PRs after S0b, so they never hold up a segment.

**Milestones**

| M | PRs (count to here) | Users can run | Validation |
|---|---|---|---|
| F1 | T1, T2 (2) | the E4 farmer, on the new walker | unit tests; E4 farmer unchanged on hardware |
| F2 | D4, S0, S1 (5) | **new game → rival battle** | each checkpoint ×3 from its save |
| F3 (gate) | D1, D3, B1, B2, S2 (10) | **new game → Boulder Badge** | each checkpoint ×3 from its save; Switch hours logged per checkpoint |
| F4–F6 | phase B | further, one segment at a time | each checkpoint from its save, then full runs |
| F7 | phase D | SV unchanged; FRLG on the shared core | SV AutoStory smoke run of a few checkpoints |

**Gate F3 (public, in the proposal).** Continue past Brock only if every
pilot checkpoint (about 12) succeeds 3 times in a row from its own save,
**and** the measured cost is at most **3 Switch hours per checkpoint on
average**, counting both development and validation. If either target is
missed, stop at Brock: the pilot stays a working program, and we post the
measured cost. At 3 hours, the full game (~100 checkpoints) is about 300
hardware hours. Also record lines per checkpoint against the ~100–120 that
the 10–12k-line estimate implies. (The 3-hour budget was added in this
revision; the earlier gate named only the ×3 runs. The user should confirm
it before the proposal is posted.)

## 8. Who wrote the FRLG code we build on

From `git log` / `git blame` on `upstream/main` at `f9cf36f7a`
(2026-10-25). Paths are under `FRLG/`. They are the likely reviewers, and
one of them may sponsor the work.

| Author | What | Blame share | Last activity |
|---|---|---|---|
| thewhitewolfking | `Programs/Farming/PokemonFRLG_EliteFourFarmer.cpp/.h`, the tile walker T2 moves (#1536, #1544) | all 1,004 lines of the .cpp | 2026-10-08 (#1544); no other commits, no open PRs |
| theAstroGoth | `Programs/RngManipulation/PokemonFRLG_StarterRng.cpp` (the story fragments S1 reuses), most of `PokemonFRLG_Navigation.cpp` | 831 of 862 StarterRng lines; 686 Navigation lines; 88 FRLG commits, the most | 2026-10-21 (#1662); open BDSP starter RNG #1366 |
| Dalton (Dalton-V) | `Programs/Farming/PokemonFRLG_NuggetBridgeFarmer.cpp` (money top-up option), selection arrows and start menu (#1129), StarterRng input trim (#1679) | 213 of 235 NuggetBridge lines; 354 Navigation lines; 27 FRLG commits | 2026-10-24 (#1720, CONTRIBUTING testing additions); open #1614 FRLG box sorter |
| kichithewolf | started `PokemonFRLG_Navigation.cpp` (2025-01-29), party slot detection | 234 Navigation lines; 23 FRLG commits | 2026-09-20 (#1485) |
| Alexander Yee (Mysticial) | maintainer; moves and cleanups | 64 FRLG commits | 2026-10-25 (repo); FRLG 2026-10-15 |
| us (dayfine) | `WildBattleExitActions` seam and tests (#1663), evolution exit fix (#1684), both merged | 229 lines of `PokemonFRLG_Tests.cpp` | 2026-10-23 |

Reading: theAstroGoth is the most likely FRLG reviewer and sponsor (largest
and most recent FRLG author, and owns the code S1 reuses). Ask
thewhitewolfking to look at T2, since it moves their walker; they have been
quiet since 2026-10-08. Dalton is active and owns the Nugget Bridge code
behind the money option. For phase D, the SV AutoStory author is jw098 (117
commits under SV AutoStory; last SV AutoStory commit 2026-08-30, last repo
commit 2026-10-09).

Process notes that apply to the PRs: code-health limits apply to every new
file (functions ≤50 lines); the segment style is short checkpoint functions
calling path data and helpers, which also keeps reviews small. Who does what
among the agents is recorded outside this doc.

## 9. Decisions (user, 2026-10-25)

1. **Shared pieces:** offer to work on them rather than waiting to be
   asked. *Revised 2026-10-25 after review:* the offer comes in phase D,
   after the FRLG pilot runs and people use it. Until then, BDSP gets only
   the standalone bug comments (phase C).
2. **Scope:** main story to the Champion. No Sevii or post-game.
   *Revised 2026-10-29:* the run ends with catching Mew, so it includes the
   Sevii Islands post-game for the Ruby and Sapphire (Network Machine). Mew
   has no in-game encounter in FRLG, so how it is obtained (e.g. arbitrary
   code execution through box names, which needs the FRLG naming typer) is
   still to be settled and becomes its own segment.
3. **Shortcuts** (Nugget Bridge money, Retro Mail duplication): available as
   options. They need planning, e.g. catching the right Pokémon at the right
   time, so they feed into the requirements plan (§7.6).
4. **Team:** worked out backwards from what the route and options need
   (§7.6), not chosen up front.
5. **Language:** English only.

6. **Version:** FireRed (LeafGreen is already finished on the test
   console).
7. **Solo from a new game:** the program must not require a trade, a
   trade-back, or a levelled Pokémon brought in from another save. Optional
   trades the game itself offers (e.g. the Vermilion Farfetch'd) are
   allowed only behind an option.
