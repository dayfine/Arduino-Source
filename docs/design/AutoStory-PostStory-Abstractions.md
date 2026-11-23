# AutoStory: Post-Story Abstractions (Loops, Reusable FRLG Actions, and Scripted Battles)

Written 2026-11-22, based on building `PokemonFRLG:MailGlitchActivation` ([PR #781](https://github.com/dayfine/Arduino-Source/pull/781)) — the first non-main-story, multi-checkpoint FRLG program layered on top of the shared AutoStory engine (`Pokemon/AutoStory/` + `PokemonFRLG/Programs/AutoStory/`).

## 1. Motivation

`MailGlitchActivation` proved that the shared AutoStory stack already generalizes well beyond a linear Badge-to-Hall-of-Fame run: JSON-authored checkpoints (`mg-01`–`mg-05`), map-planned wall-anchored routes, `StoryExecutor`, save-card verification, and reload-on-failure retries all worked out of the box.

At the same time, writing the first *post-story / glitch-setup* program exposed **four places where the base engine lacks an abstraction**, forcing `MailGlitch` to either bypass the segment runner, duplicate menu/bag plumbing, or hand-roll parallel battle loops:

1. **No first-class looped checkpoint (`repeat_until`) in `Pokemon_StorySegments`.**
2. **Six general-purpose FRLG overworld/menu actions** (`party_swap`, `give_bag_item`, `spin_for_encounter`, `ingame_trade`, `buy_coins`, `redeem_prize_mon`) live inside `MailGlitch` even though any post-game, Dex, EV-training, RNG, or glitch program needs them.
3. **No shared low-level battle driver or declarative scripted-battle layer** (especially for switch-training, faint-and-flee HP setup, and double battles with ally targeting).
4. **Program wiring boilerplate** (`StoryGameHooks` construction and extra action registration) is copy-pasted from `PokemonFRLG_AutoStory.cpp`.

This document defines **three focused follow-up PRs (PR A, PR B, PR C)** to lift those abstractions into the base codebase without bloating PR #781.

---

## 2. Audit of PR #781: Reusable vs. Program-Specific

| Component in `Programs/MailGlitch/` | Current location | Classification | Target home |
|---|---|---|---|
| `run_grind_loop_checkpoint` / `run_mail_glitch_rows` | `MailGlitchActivation.cpp` | **Core engine gap** | `Pokemon/AutoStory/` (`StoryRepeat`, **PR A**) |
| `mail_glitch_hooks()` (`preflight`, `load_save`, `check_save_place`, `after_load`, `log`, `status`) | `MailGlitchActivation.cpp` | **Shared FRLG wiring** | `PokemonFRLG/Programs/AutoStory/` (**PR B**) |
| `party_swap` (species/slot swap, language-free `UP×3 → SWITCH`) | `MailGlitchActions.cpp` | **Reusable FRLG action** | `PokemonFRLG/Programs/AutoStory/` (**PR B**) |
| `give_bag_item` (Items pocket, Mail Easy Chat editor, Berry Pouch `No.XX`) | `MailGlitchActions.cpp` | **Reusable FRLG action** | `PokemonFRLG/Programs/AutoStory/` (**PR B**) |
| `open_bag_pocket_top` / `find_item_row_in_open_pocket` / `use_revive_on_slot` | `MailGlitchActions.cpp`, `MailGlitchBattle.cpp` | **Duplicate bag plumbing** | Shared `AutoStoryItemUse` helper (**PR B**) |
| `spin_for_encounter` (30ms turn-in-place with `read_facing()` sync) | `MailGlitchBattle.cpp` | **Reusable FRLG wild helper** | `PokemonFRLG/Overworld/` or `AutoStory` (**PR B**) |
| `ingame_trade`, `buy_coins`, `redeem_prize_mon` | `MailGlitchActions.cpp` | **Reusable FRLG actions** | `PokemonFRLG/Programs/AutoStory/` (**PR B**) |
| `wait_battle_menu_or_end`, `open_fight_and_pick_move`, `battle_switch_to_slot`, `select_party_member_in_battle`, `accept_move_into_slot`, double-battle targeting | `MailGlitchBattle.cpp` | **Shared FRLG battle driver** | `FRLGBattleDriver` (**PR C**) |
| `fight_switch_grind_battle`, `faint_holder_and_flee`, `execute_double_battle_setup_turns` + `sweep_double_battle` | `MailGlitchBattle.cpp` | **Declarative battle scripts/policies** | `BattleScript` / `BattlePolicy` (**PR C**) |
| `clear_purified_zone` (Tower 5F heal pad chime/dialog) | `MailGlitchBattle.cpp` | **Program-specific** *(or minor heal-pad helper)* | Stay in `MailGlitch` |
| `verify_mail_glitch` (`PartyHeldItemDetector` on `mr-mime`) | `MailGlitchActions.cpp` | **Program-specific** | Stay in `MailGlitch` |
| `mg-01..mg-05` JSON plans & `SweeperMovePool` Ghost-safe priority | `MailGlitch/PlanData/` | **Program-specific** | Stay in `MailGlitch` |

---

## 3. PR A — First-Class Looped Checkpoints (`StoryRepeat`) in `Pokemon_StorySegments`

### Problem

[`CheckpointSpec`](../../SerialPrograms/Source/Pokemon/AutoStory/Pokemon_StorySegments.h) currently supports two execution shapes:
- **Single-shot**: run once, retry from last save up to `policy.attempts`, optionally save.
- **`StoryVerify`** (`Pokemon_StoryVerify.h`): run multiple times *from the same start save* without saving between runs, to measure reliability.

Neither expresses an **idempotent progress loop**: *"start at a save point, execute one bounded batch of work (e.g. 10 wild battles), return to the save point to heal and save, and repeat until a completion condition holds — so any failure reloads the latest saved batch and loses at most one batch of progress."*

Because the engine lacks this primitive, `MailGlitchActivation.cpp` had to:
- bypass `run_segment(first..last)` with a bespoke `run_mail_glitch_rows()` + `run_grind_loop_checkpoint()` loop calling `run_segment({row.id, row.id})` one checkpoint at a time;
- pass loop termination and per-batch state through ad-hoc `RunState::flags` strings (`mail_glitch_grind_done:farfetchd`, `mail_glitch_grind_done:mr-mime`, `mail_glitch_grind_batch_used`).

### Proposed Design

Add `Pokemon/AutoStory/Types/Pokemon_StoryRepeat.h` (parallel to `Pokemon_StoryVerify.h`):

```cpp
namespace PokemonAutomation::Pokemon{

struct StoryRepeat{
    //  Hard cap on completed iterations before failing ("repeat_limit_exceeded").
    size_t max_iterations = 60;
    //  RunState::flags entries that must ALL be present for the loop to exit.
    std::vector<std::string> until_flags = {};
    //  RunState::flags entries cleared at the start of every iteration
    //  (e.g. per-batch guard flags that should not persist across batches).
    std::vector<std::string> reset_flags_each_iteration = {};

    bool operator==(const StoryRepeat&) const = default;
};

} // namespace PokemonAutomation::Pokemon
```

Extend `CheckpointSpec` in `Pokemon_StorySegments.h`:

```cpp
struct CheckpointSpec{
    std::string id;
    std::string title;
    bool saves = false;
    MoveOnlyFunction<Checkpoint()> build;
    std::optional<StoryVerify> verify = std::nullopt;
    std::optional<StoryRepeat> repeat = std::nullopt;
};
```

JSON schema addition under `"execution"` (validated by `StoryActionRegistry` / plan codec):

```json
"execution": {
  "menu": "full",
  "repeat": {
    "max_iterations": 60,
    "until_flags": ["mail_glitch_grind_done:farfetchd", "mail_glitch_grind_done:mr-mime"],
    "reset_flags_each_iteration": ["mail_glitch_grind_batch_used"]
  }
}
```

### Runtime Semantics in `run_segment()`

1. Reject rows that set both `verify` and `repeat` (caught at lint/validation time).
2. For a row with `repeat`:
   - For `iteration = 1 .. repeat->max_iterations`:
     - Erase every key in `repeat->reset_flags_each_iteration` from `state.flags`.
     - Run the checkpoint with normal retry-from-last-save semantics (`policy.attempts`) and save at the end of the iteration when `row.saves` is true and `policy.save_mode` allows it.
     - Log `[Repeat] <id>: iteration <k> passed (saved)` (or failure/reload via existing `[Checkpoint]` lines).
     - Check whether every flag in `repeat->until_flags` is present in `state.flags`. If so, log `[Repeat] <id>: done after <k> iteration(s)` and advance to the next checkpoint.
   - If the loop exhausts `max_iterations` without satisfying `until_flags`, stop the segment with `outcome_failed("repeat_limit_exceeded", id)`.
3. **Reload safety**: Because each completed iteration saves at the checkpoint's idempotent start pose, a mid-batch failure on iteration `k` reloads the save from iteration `k - 1` (losing `< 1` batch of work) and retries iteration `k` up to `policy.attempts`.

### Reusable Use Cases Beyond Mail Glitch
- Switch-training / level grinding loops (FRLG, BDSP, future games)
- Game Corner coin / Nugget Bridge / Amulet Coin money farming batches
- EV training batches with periodic Pokemon Center heals & saves
- Safari Zone or rare-encounter catching loops

### Acceptance Criteria (PR A)
- `StoryRepeat` + `run_segment()` support in `Pokemon/AutoStory/` (completely game-agnostic, Qt-free).
- JSON plan codec + `StoryActionRegistry` lint rules (reject `verify` + `repeat`, require non-empty `until_flags` and `max_iterations > 0`).
- Unit tests using the existing headless `Segment` test harness covering:
  - immediate exit after 1 iteration when flags are already met;
  - multi-iteration completion with per-iteration saves and `reset_flags_each_iteration`;
  - mid-iteration failure reloading the *previous iteration's* save and succeeding on attempt 2;
  - `repeat_limit_exceeded` failure when `max_iterations` is hit.
- Delete `run_grind_loop_checkpoint()` and `run_mail_glitch_rows()` from `MailGlitchActivation.cpp` once PR A lands.

---

## 4. PR B — Shared FRLG Overworld/Menu Actions & Program Wiring

### Problem

1. Six general FRLG actions (`party_swap`, `give_bag_item`, `ingame_trade`, `buy_coins`, `redeem_prize_mon`, and `spin_for_encounter`) live inside `Programs/MailGlitch/` and cannot be authored in `AutoStory` or other FRLG programs without copying code.
2. `use_revive_on_slot` (`MailGlitchBattle.cpp`) duplicates `open_bag_pocket_top` + `find_item_row_in_open_pocket` (`MailGlitchActions.cpp`).
3. `MailGlitchActivation.cpp` duplicates `StoryGameHooks` wiring (`mail_glitch_hooks()`) from `PokemonFRLG_AutoStory.cpp`.
4. Live mGBA testing of `spin_for_encounter()` showed an edge case (~4 occurrences across 33 batches) where `BattleFlashWatcher` missed a wild encounter that started between 24-tap spin batches, causing a false `grind_no_encounter` failure.

### Proposed Design

1. **Shared Bag Helpers** (extend `PokemonFRLG_AutoStoryItemUse.{h,cpp}` / `PokemonFRLG_AutoStoryPocketList.{h,cpp}`):
   - `open_bag_pocket_top(console, context, pocket)`
   - `find_item_row_in_open_pocket(console, context, language, item)`
   - `find_berry_row_in_pouch(console, context, item)` — language-free Berry Pouch reader using the fixed `No.XX` 2-digit index (`pokefirered/src/berry_pouch.c:686-697`), extensible to all 43 FRLG berries via a small slug→number table.
   - `use_bag_item_on_party_slot(env, pocket, item, party_slot)` — unifies Revive, Potions, status heals, and future out-of-battle item uses.
   - `give_bag_item_to_party_slot(env, item, party_slot)` — supports Items pocket, Mail (with Easy Chat 1-word write + confirm), and Berry Pouch.

2. **Shared FRLG Party & NPC Actions** (new `PokemonFRLG_AutoStoryPartyActions.{h,cpp}` or folded into `AutoStoryStepTable`):
   - `party_swap` (`species` + `target_slot`, or `slot_a` + `slot_b`) using the language-free `UP × 3 → SWITCH` wrap invariant (`pokefirered/src/party_menu.c:2963-2988`).
   - `give_bag_item` (`item`, `species` or `target_slot`).
   - `use_bag_item` (`item`, `species` or `target_slot`).
   - `ingame_trade` (`species`, optional `receive` for idempotent skip).
   - `buy_coins` (`coins`) and `redeem_prize_mon` (`slot`).

3. **Hardened `spin_for_encounter(env)`** (move to `PokemonFRLG_AutoStoryWild.{h,cpp}` or `Overworld/`):
   - Keep the 30ms clockwise turn-in-place with `read_facing()` re-sync per batch (never steps off the anchor tile).
   - **Fix the missed-flash edge case**: watch `{BattleFlashWatcher, BattleScreenWatcher, BattleMenuWatcher, WhiteDialogWatcher}` *and* check `battle_shows()` before each batch, so an encounter that triggers between batches is caught immediately instead of spinning against a battle screen until timeout.

4. **Shared Program Wiring Helper** (`PokemonFRLG_AutoStoryLaunch.{h,cpp}` or `AutoStoryEvents.{h,cpp}`):
   - Extract `make_frlg_story_hooks(env, story)` so both `AutoStory` and `MailGlitchActivation` (and any future FRLG story-engine program: ACE, Sevii Islands, Legendary Hunts) share identical preflight/load/check_save_place/after_load/log/status hooks.
   - Allow `StoryExecutor` to accept an optional table of program-local extra `StoryActionSpec` + dispatch callbacks, rather than hardcoding `run_mail_glitch_game_step` inside `AutoStoryEvents`.

### Acceptance Criteria (PR B)
- All 6 actions registered in FRLG's shared `StoryActionSpec` catalog with unit tests for arg validation, Berry number lookup, and party-slot/swap calculations.
- `spin_for_encounter()` checks `BattleScreenWatcher` + `BattleMenuWatcher` in addition to `BattleFlashWatcher`.
- `MailGlitchActions.cpp` shrinks to only Mail-Glitch-specific steps (`clear_purified_zone`, `verify_mail_glitch`, and program-local glue).
- Zero Qt includes; `/W4 /WX` clean; `code-health` = 0 failures.

---

## 5. PR C — Two-Layer Battle Abstraction (`FRLGBattleDriver` + Declarative `BattleScript`)

### Problem

FRLG currently has **two disconnected battle implementations**:
- [`PokemonFRLG_AutoStoryBattle.cpp`](../../SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattle.cpp): tightly coupled to single-battle AutoStory combat (`BattleKind::{WILD_RUN, WILD_FIGHT, TRAINER}` + optimal-KO `BattlePolicy`).
- [`PokemonFRLG_MailGlitchBattle.cpp`](../../SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/MailGlitch/PokemonFRLG_MailGlitchBattle.cpp): hand-rolls **three separate battle loops** (`fight_switch_grind_battle`, `faint_holder_and_flee`, and `execute_double_battle_setup_turns` + `sweep_double_battle`), all duplicating the same battle-menu state machine, party-switching choreography, move-learn replacement, and double-battle targeting — and none of them can be unit-tested without an emulator.

### Proposed Architecture

Split scripted/utility battle execution into **two cleanly separated layers**:

```
┌────────────────────────────────────────────────────────────────────┐
│  Layer 2: BattleScript / BattlePolicy  (pure logic, unit-testable) │
│   - ScriptedTurn sequence (single & double battles, ally target)   │
│   - SwitchTrainPolicy (T1 switch -> sweep + move-learn slot)       │
│   - FaintAndFleePolicy (attack until forced switch -> flee)        │
│   - SweepPolicy (SweeperMovePool, PP rotation, HP guard)           │
└───────────────────────────────┬────────────────────────────────────┘
                                │ calls atomic verbs on interface
┌───────────────────────────────▼────────────────────────────────────┐
│  Layer 1: BattleDriver interface                                   │
│   - ConsoleFRLGBattleDriver (real Switch / mGBA implementation)    │
│   - MockBattleDriver        (unit tests, deterministic replay)     │
└────────────────────────────────────────────────────────────────────┘
```

#### Layer 1: `BattleDriver` Interface (`PokemonFRLG_BattleDriver.h`)

```cpp
enum class BattleEvent{
    MENU,
    FORCED_SWITCH,
    OUT_OF_PP,
    MOVE_LEARN_PROMPT,
    OVERWORLD,
    TIMEOUT,
};

enum class BattleTarget{
    DEFAULT,    // single battle or spread move (no target cursor needed)
    ALLY,       // double battle: DPAD_RIGHT -> A (partner slot)
    FOE_LEFT,   // double battle: default top-left foe -> A
    FOE_RIGHT,  // double battle: top-right foe -> A
};

struct BattleScreenState{
    BattleEvent event = BattleEvent::TIMEOUT;
    PartyStatus party_status = {};  // populated on MENU
};

class BattleDriver{
public:
    virtual ~BattleDriver() = default;

    virtual BattleScreenState wait_event(Milliseconds timeout) = 0;
    virtual void use_move(size_t move_slot, BattleTarget target = BattleTarget::DEFAULT) = 0;
    virtual bool switch_to(size_t party_slot) = 0;
    virtual bool select_forced_switch(size_t party_slot) = 0;
    virtual void clear_out_of_pp_prompt() = 0;
    virtual void flee() = 0;
    virtual void handle_move_learn(std::optional<size_t> replace_slot) = 0;
    virtual void wait_battle_exit() = 0;
};
```

Key hardware/emulator invariants baked once into `ConsoleFRLGBattleDriver`:
- **Red-arrow-gated battle text advance**: advance dialog only on `AdvanceBattleDialogDetector` (or stuck-dialog timeout), never mashing `B` blindly into a `"Delete a move to make room for X?"` YES/NO prompt.
- **Deterministic battle party switching**: wait `650ms` for party menu settle, press `DPAD_DOWN × party_slot` from guaranteed Slot 0 (avoids `PartySlotDetector` sub-pixel cursor misreads on 240×160 mGBA captures), confirm `PartySelectionWatcher`, and verify `PartyMenuDetector` closes.
- **Double-battle targeting**: cleanly separates spread moves (`DEFAULT`) from single-target ally/foe selection (`ALLY` / `FOE_LEFT` / `FOE_RIGHT`).

#### Layer 2: `BattleScript` & Composable Policies (`PokemonFRLG_BattleScript.h`)

```cpp
struct BattleCommand{
    enum class Kind{ USE_MOVE, SWITCH, FLEE };
    Kind kind = Kind::USE_MOVE;
    size_t slot = 0;                       // move slot (0-3) or party slot (0-5)
    BattleTarget target = BattleTarget::DEFAULT;
};

struct ScriptedBattlePlan{
    //  Executed in order, one entry per MENU event (covers both single battles
    //  and the two commands per turn in double battles).
    std::vector<BattleCommand> opening_commands = {};
    //  What to do once opening_commands are exhausted.
    enum class Fallback{ SWEEP, FLEE, FAIL };
    Fallback fallback = Fallback::SWEEP;
    std::optional<size_t> forced_switch_slot = std::nullopt;
    std::optional<size_t> move_learn_replace_slot = std::nullopt;
};
```

How all three Mail Glitch battles express themselves cleanly on top of this:

1. **Twins Eli & Anne (`mg-05` double battle)** — purely data-driven `ScriptedBattlePlan`:
   - Command 1 (T1 Slot 1, `CH'DING`): `USE_MOVE(knock_off_slot, BattleTarget::ALLY)`
   - Command 2 (T1 Slot 2, `Rattata`): `SWITCH(party_slot=2)` (`MIMIEN`)
   - Command 3 (T2 Slot 1, `CH'DING`): `SWITCH(party_slot=3)` (`Blastoise`)
   - Command 4 (T2 Slot 2, `MIMIEN`): `USE_MOVE(recycle_slot, BattleTarget::DEFAULT)`
   - Fallback: `SWEEP` with `Blastoise` (`surf_slot`, `DEFAULT` if spread else `FOE_LEFT`), `forced_switch_slot = 3`.

2. **Switch-train (`mg-04b` `switch_grind`)**:
   - Opening: `[SWITCH(party_slot=1)]` (sweeper)
   - Fallback: `SWEEP` via `SweeperMovePool` (auto-rotates on `OUT_OF_PP`, records `sweeper_hurt` on yellow/red HP, fails `grind_sweeper_fainted` if `FORCED_SWITCH` arrives after sweeper is out)
   - `move_learn_replace_slot = cfg.learn_slot`

3. **Faint-and-flee (`mg-05` `prepare_berry_hp`)**:
   - Lead uses `USE_MOVE(0)` until `FORCED_SWITCH`, sends `sweeper` (`party_slot=1`), then `FLEE` on next `MENU`.

*(Note: AutoStory's full trainer/League `fight_battle()` stays untouched in PR C; it can optionally adopt `ConsoleFRLGBattleDriver` later once PR C is proven.)*

### Acceptance Criteria (PR C)
- `BattleDriver` interface + `ConsoleFRLGBattleDriver` + `ScriptedBattleRunner` implemented Qt-free under `PokemonFRLG/Programs/AutoStory/` (or `PokemonFRLG/Battle/`).
- Unit tests with `MockBattleDriver` verifying:
  - double-battle 4-command setup + sweep (both spread move `Surf` and single-target fallback);
  - switch-grind T1 switch, `OUT_OF_PP` move rotation, move-learn slot overwrite, and `SWEEPER_FAINTED` detection;
  - faint-and-flee sequence (attack until forced switch → send sweeper → flee).
- `PokemonFRLG_MailGlitchBattle.cpp` delegates all 3 battle flows to `ScriptedBattleRunner` + `ConsoleFRLGBattleDriver`.

---

## 6. Sequencing & Risks

| Order | PR | Depends on | Scope / Risk |
|---|---|---|---|
| 0 | Finish live mGBA validation of PR #781 (`mg-04b` → `mg-04c` → `mg-05`) | — | Validates double-battle targeting & `verify_mail_glitch` on real game logic before freezing PR C's `BattleTarget` semantics. |
| 1 | **PR A**: `StoryRepeat` in `Pokemon/AutoStory/` | None (base `frlg/lean` or shared core seam) | Small (~150 LOC + tests), zero console risk, immediately portable to BDSP & all Story Engine games. |
| 2 | **PR B**: Shared FRLG actions, bag helpers, hardened `spin_for_encounter`, `make_frlg_story_hooks` | PR #781 learnings | Medium (~400 LOC move + cleanup); low risk because every action is already live-tested on mGBA. |
| 3 | **PR C**: `BattleDriver` + `ScriptedBattleRunner` | PR #781 `mg-05` double-battle verified | Medium (~450 LOC + mock-driver tests); folds in any real double-battle edge cases observed in `mg-05`. |

### Open Questions to Confirm After `mg-05` Passes
1. **Double-battle target cursor initial position** in FRLG when `CH'DING` (Slot 1) selects `Knock Off`: verify whether `DPAD_RIGHT` reliably highlights Ally Slot 2 (`Rattata`) or if target cursor starts on Foe Left and requires `DPAD_DOWN` + `DPAD_RIGHT`. Update `ConsoleFRLGBattleDriver::use_move(..., BattleTarget::ALLY)` accordingly before PR C.
2. **Sweeper HP threshold during `switch_grind`**: live mGBA `mg-04b` runs showed occasional early batch exits (at 1–6 battles instead of 10) triggered by `note_sweeper_hp` reading a yellow/red HP pixel during battle-menu animation transitions. PR C's `ConsoleFRLGBattleDriver` should require `PartyStatus` to settle (or 2 consecutive reads) before setting `sweeper_hurt`.
3. **Should `StoryRepeat` target `autostory/story-policies-seam` (or the latest shared core seam) directly instead of `frlg/lean`?** Landing PR A on the shared core seam lets BDSP and FRLG adopt it cleanly without duplicate merges.
