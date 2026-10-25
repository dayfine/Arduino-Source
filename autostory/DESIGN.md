# AutoStory: shared run flow, 2D movement, and an FRLG AutoStory

Status: draft for the user, 2026-10-25. Nothing here has been posted upstream.
Sources: SV AutoStory on `main`; BDSP WIP
[PokemonAutomation/Arduino-Source#1717](https://github.com/PokemonAutomation/Arduino-Source/pull/1717)
at `65094d4cd`; SwSh WIP PokemonAutomation/Arduino-Source#1207 at `32d506c27`;
FRLG code on `main`; the [pret/pokefirered](https://github.com/pret/pokefirered)
decompilation; the [52poke walkthrough](https://wiki.52poke.com/wiki/%E9%99%84%E5%BD%95:%E5%AF%B6%E5%8F%AF%E5%A4%A2_%E7%81%AB%E7%B4%85%EF%BC%8F%E8%91%89%E7%B6%A0%E6%B5%81%E7%A8%8B).
Paths: `SV/` = `SerialPrograms/Source/VideoGames/PokemonSV/Programs/AutoStory/`,
`BDSP/` = `SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/` (PR ref),
`FRLG/` = `SerialPrograms/Source/VideoGames/PokemonFRLG/`.

## 0. Summary

- **Run flow.** BDSP copied SV's framework almost name for name (segment and
  checkpoint interfaces, registries, start/end selects, save-then-retry loop,
  stats). The two copies have already drifted: retry limits (SV 100 with an
  off-by-one, BDSP 20), error reporting (SV only), day skip (SV only), start >
  end check (SV only). SwSh #1207 does not use the framework at all. A small
  game-agnostic core (retry loop behind a hooks interface, index/range math,
  stats, select databases) can be extracted from SV as pure moves in five
  small PRs, then adopted by BDSP and FRLG. Each game keeps its own options,
  save/reset functions and checkpoints.
- **2D movement.** FRLG and BDSP both move on a tile grid, and both
  codebases reinvented the same three things badly or partially: a path as
  `{direction, tiles}` data, walking under interrupt watchers, and getting
  back on the path after an interrupt. BDSP's best idea is closed-loop
  landmark alignment (segments 00–03); its later segments are blind scripts
  with per-trainer hard-coded "move back" pushes, one of which never runs
  (§5.2). A shared, console-free core (path executor with resume bookkeeping,
  interrupt policy, alignment control law) plus per-game adapters (timings,
  detectors, matchers) fits both. Timing constants stay per game.
- **FRLG AutoStory** is smaller than SV or BDSP because GBA movement is
  frame-exact, the camera is fixed with the player centred, Kanto has no
  lighting changes, and the decomp gives exact trainer positions and sight
  ranges. Estimate: ~3–5k lines of infrastructure, ~90–110 checkpoints at
  40–80 lines each, ~10–12k lines total (SV 22.4k, BDSP 32.7k). The
  bottleneck is Switch time to validate each checkpoint, not code.
- **Order:** (1) run-flow prework from SV, (2) shared tile movement with FRLG
  as the first adopter (moving `EliteFourFarmer`'s walker out), (3) FRLG
  pilot Pallet → Pewter to measure lines and hours per checkpoint, (4) the
  rest of FRLG by segment. Offer the shared pieces to the BDSP team; their
  detector-first extraction plan (Mysticial's request) is independent of it.

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
| `save=false` path | — | `reload_backup_save` is an empty TODO `BT.cpp:188-197`, no callers | dead |
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
  went wrong twice (§5.2).
- Code-health limits (fork #41: functions ≤50 lines, nesting ≤4, ≤7 params)
  are exceeded by: SV constructor ~397 lines, `test_checkpoints` 180 lines /
  8 params / nesting 5, `test_code` 96, `on_config_value_changed` 87,
  `program` 82, both retry loops (64 and 57 lines, 8 and 7 params), and every
  `checkpoint_NN` (8 params). BDSP: constructor 282 (four copies of the
  14-move list), `program` 74, `save_game` 71, retry loop 51 / 8 params. Pure
  moves keep these as legacy; anything new should not add more.

## 3. Run flow: shared core and prework

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

### 3.3 Prework PR sequence (SV only, no behaviour change)

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

### 5.1 Pieces

Shared, console-free, in `SerialPrograms/Source/Pokemon/Navigation/`
(precedent: `Pokemon/Pokemon_AdvRng.h` is shared by FRLG and RSE):

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

## 6. Feedback for the BDSP team (draft; the user decides whether to post)

They asked for feedback on "the SV-derived architecture, checkpoint and
recovery contracts, BDSP helper/detector boundaries, resource distribution,
and the order in which to extract focused PRs".

**Architecture.** Following SV is right, and their `section_database` is
cleaner than SV's six copies. Rather than a third copy of the framework
(SwSh #1207 will need one too), propose the shared core in §3 and adopt it
when it lands; their loop becomes a hooks object with policy `{20, ..., never
report}` and their stats/base classes/database code are deleted.

**Checkpoint and recovery contracts.**
- `reload_backup_save` is an empty TODO but the `save=false` path calls it
  and then re-runs the action without any reset (`BT.cpp:188-197`); remove
  the path or implement it.
- No start > end check, no recoverable-error report, no `GO_HOME_WHEN_DONE`,
  language fixed to English (`BA.cpp:701`): adopt SV's behaviour here.
- Diagnostic options marked "remove before production" (`BA.h:42-46`) and
  `CheckpointRetryScope`: gate behind developer mode or drop.
- Checkpoint order is written twice (segment + registry) and has drifted:
  - Segment 08's `run_segment` stops at 0821 (`Segment_08.cpp:~116`), so
    0822–0826 (Sunyshore and the 8th gym) never run in segment mode.
  - 0717 is missing from the advanced list (`BA.cpp:155-156` goes 0716 →
    0718).
  - Deriving segments from registry ranges removes this class of bug.
- Badge texts swapped: Segment 04 says "Third badge (Hearthome)", Segment 05
  "Fifth badge (Veilstone)"; Veilstone is the 3rd, Hearthome the 5th.

**Helper/detector boundaries.**
- Movement: replace per-segment `move_back_after_unexpected_battle` chains
  with path data (`trainer_stops_at`, §5.2). Includes the dead skier push
  above.
- The landmark loop (segments 00–03) deserves to become the shared alignment
  routine (§5.1); later segments would gain from it most.
- `handle_battle` is copied into each of segments 04–09, keyed by trainer-id
  strings, while `Battle.cpp`'s `handle_expert_battle` and
  `handle_battle_with_outside_help` have no callers; pick one.
- `Navigation.cpp` stubs and `Dialog.cpp` (already agreed to delete).
- Changes to shared `DialogDetector` boxes affect every BDSP program; those
  need screenshot tests before anything else depends on them.

**Resources.** Templates live only in their Packages fork; per-hour variants
multiply them. Document which landmarks need lighting sets and keep the
lighting sweep program as a dev tool.

**Extraction order.** Agree with Mysticial: detectors first, one PR each
with CommandLineTests images. Then navigation primitives (shared tile path
if it exists by then), then the battle handler, then segments 00–01.

Tone: the PR is explicitly WIP and asks for early feedback; keep it to a few
concrete items plus the shared-core offer.

## 7. FRLG AutoStory plan

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

1. Shared run flow (§3) adopted in FRLG: FRLG hooks (save via start menu,
   reset via `soft_reset` + load menu), options, registries.
2. Shared tile path (§5) with the FRLG adapter, moving `walk_path` and the
   constants out of `EliteFourFarmer.cpp`; add run (8 frames/tile), bike (6),
   surf (8) per decomp; ledge steps.
3. Overworld detector (overworld vs dialog vs battle vs menu without opening
   the start menu) and facing detection (player sprite in the centre tile).
4. Walk interrupts: trainer "!" or approach, wild battle (black screen +
   battle menu), item pickup jingle/dialog, NPC dialog; resume via §5.2.
5. Trainer battle handler: move choice (per-checkpoint plan first, type
   table later), "will you switch?" (avoid by Battle Style = Set), next
   Pokémon after a faint, move learn (skip or replace per config),
   evolution (allow/cancel per config), whiteout → fail the checkpoint.
6. New game: intro, naming keyboard (preset names avoid it), options menu
   (text Fast, animations Off, style Set), load menu on a fresh save.
7. Field moves: teach HM from bag, use Cut/Surf/Strength from the party
   menu, Fly (exists), Bike on/off; puzzles below.
8. Shop and PC: buy list per Mart, deposit/withdraw for HM carriers.

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

### 7.6 Milestones

| M | Deliverable | Rough size | Validation |
|---|---|---|---|
| F0 | Run-flow prework P1–P5 (SV) | ~1k moved, ~400 new incl. tests | SV AutoStory smoke run of a few checkpoints |
| F1 | Shared tile path + FRLG adapter; E4 farmer switched to it | ~800 | unit tests; E4 farmer run unchanged |
| F2 | FRLG overworld/facing detectors, walk interrupts, trainer battle handler | ~1.5k | screenshot tests; scripted fakes |
| F3 | FRLG AutoStory program shell (options, hooks, registries) + segment 00–01 pilot | ~1.5k | live: new game → Boulder Badge, each checkpoint ×3 from its save |
| F4 | Route-planning tool (offline) and segments 02–04 | ~2k | live per checkpoint |
| F5 | Segments 05–07 (puzzles) | ~2k | live |
| F6 | Segments 08–10, optional segments, money top-up | ~2k | full run |

Decide after F3 using measured lines and Switch hours per checkpoint.

## 8. Process

- All new work as fork drafts on `dayfine/Arduino-Source`; per AGENTS.md,
  Codex owns new feature branches until the first fork PR and does the live
  validation; Claude prepares PRs. Nothing goes upstream without the user.
- F0 touches SV AutoStory, which is someone else's merged program: pure moves
  first, behaviour changes separately, `--color-moved` reviewable.
- Code-health limits apply to every new file (functions ≤50 lines); the
  segment style should be short checkpoint functions calling path data and
  helpers, which also keeps reviews small.

## 9. Open questions for the user

1. Offer the shared run flow and tile path to the BDSP team before or after
   F0/F1 exist? (Before = less churn for them; after = a concrete PR to
   point at.)
2. FRLG scope: main story to the Champion only, or include Sevii/post-game?
3. Allow glitch shortcuts (Nugget Bridge farm by default for money; Retro
   Mail Rare Candy duplication as opt-in)?
4. Team plan: starter solo with leveling loops, or catch HM carriers and a
   small team? The first is simpler to automate; the second needs fewer
   grinding hours.
5. Which language(s) to support first (detectors with text are per
   language)?
