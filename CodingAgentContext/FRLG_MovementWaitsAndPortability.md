# FRLG Overworld Movement Waits & Switch Portability

This document breaks down **why each overworld wait exists**, **how its duration is composed** (`Game Frames + Video/Capture Lag + Detector Hold + Margin`), and **how much of the `GbaEmulator` speedup can transfer back to Switch + Capture Card**.

---

## 1. Wait Composition Model

Every overworld wait in FRLG AutoStory is a sum of up to four components:

```text
Total Wait = T_game        (GBA/Switch engine frames for turn, walk, or flash)
           + T_video_lag   (HDMI/capture-card encode + USB decode latency; ~0ms on mGBA)
           + T_detector    (Minimum hold window required by watchers to avoid false positives)
           + T_margin      (Safety buffer for dropped frames / controller jitter)
```

- On **`Device::GbaEmulator` (mGBA)**: `T_video_lag ≈ 0ms`, `T_margin ≈ 1–2 frames (17–33ms)`, and inputs are frame-accurate.
- On **Switch + Capture Card**: `T_video_lag ≈ 60–150ms` (varies by card), USB/controller polling adds jitter, and frame drops can happen on busy scenes or `to_wall` holds.

---

## 2. Breakdown of Each Wait

| Wait / Step | Switch Value | Composition on Switch | `GbaEmulator` Value | Transferable to Switch? |
| :--- | :--- | :--- | :--- | :--- |
| **1. Post-wild-leg settle** (`AFTER_WILD_LEG_WATCH`) | `3500ms` | `~130ms` battle flash + `~100ms` video lag + **`~3200ms` capture-lag/wipe fallback** before `WallCheck::TAP` | `400ms` (`T_game` flash + recovery) | **Partially** (can likely drop to ~`800–1200ms` on Switch if `BattleFlashWatcher` is reliable, or skip `WallCheck::TAP` on `GbaEmulator`) |
| **2. Post-non-wild-leg settle** (`AFTER_LEG_WATCH`) | `500ms` | `250ms` `WhiteDialogWatcher` hold + `~100–150ms` video lag + `~100ms` margin | `0ms` (relies on `clear_open_message()` before next leg) | **Partially** (on Switch, can reduce to `~350–400ms` or `0ms` if `clear_open_message()` + `StoryInterrupts` catch dialogs before the next tap) |
| **3. Turn tap + settle** (`TURN_TAP` + `TURN_SETTLE` + `TURN_FACING_READS`) | `40ms` + `300ms` (+ up to `20 × 100ms` reads) | `133ms` (8-frame `PlayerTurnInPlace`) + `~100ms` video lag + `~70ms` margin; retries if `read_player_facing()` returns `nullopt` | `160ms` settle, `2` reads; **skipped** before `to_wall` holds (add `133ms` to wall hold) | **Yes (partially):** skipping `turn_to()` when `facing` is tracked/known transfers; `to_wall` turn-absorption transfers if `+133ms` is added to the hold |
| **4. Pre-step quiet screen** (`BETWEEN_STEPS_LOOK`) | `400ms` | `~100–150ms` video lag + `~250ms` settle window to ensure no dialog/fade is starting | `50ms–60ms` (3–4 frames) | **Partially** (needs `T_video_lag + margin` on Switch, ~`200–250ms`) |
| **5. Route planner wall detours & JSON backtraces** (`OPEN_STOP`, `OPEN_WILD_STOP`, `kind: "legs"` bounces) | `OPEN_STOP = 8`, `OPEN_WILD_STOP = 300`, hardcoded `west 24; east 2` | Extra steps + turns + `4.1s` wall checks added to avoid open-leg timing drift on Switch | `OPEN_STOP = 0`, `OPEN_WILD_STOP = 0`, fold safe opposite-direction `StoryLeg` pairs | **Partially:** safe `OPEN_STOP` reduction in non-wild rooms/cities transfers (since FRLG walks 1 tile/step on land); wild/cave wall-bounces are safer to keep on Switch |

---

## 3. Detailed Reasoning & Portability Assessment

### A. Post-Hold Settle (`AFTER_WILD_LEG_WATCH` = 3500ms, `AFTER_LEG_WATCH` = 500ms)
- **Where:**
  - `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalkWatches.h`
  - `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecoveryWalk.cpp`
  - `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.cpp`
- **Why it's `3500ms` on wild legs:** On Switch, capture-card latency or dropped frames can cause `BattleFlashWatcher` to miss the ~130ms encounter flash. Waiting `3500ms` ensures the screen wipe / `"Wild ... appeared!"` dialog is caught *before* `CheckedSteps::walk()` issues the `WallCheck::TAP` (`LEG_END_TAP`), which would otherwise tap into the battle or misread movement.
- **Why `GbaEmulator` only needs `400ms` (wild) / `0ms` (non-wild):** Zero capture-card lag means `BattleFlashWatcher` sees the flash synchronously, and `WhiteDialogDetector` is a `StaticScreenDetector` checked by `clear_open_message()` before the next leg.
- **Switch transferability:** **Low-to-Medium.** You cannot drop `AFTER_WILD_LEG_WATCH` to `400ms` on Switch without risking missed flashes, though `3500ms` is likely over-conservative on low-latency capture cards (could be tuned to ~`1000–1500ms`). On non-wild legs, `500ms` can likely trim to ~`350ms` (`250ms` `WhiteDialogWatcher` hold + `100ms` video lag).

### B. Turn Tap & Facing Verification (`TURN_TAP` 40ms + `TURN_SETTLE` 300ms + up to 20×100ms reads)
- **Where:**
  - `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementTurnPresses.h`
  - `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldFacing.cpp`
  - `SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.cpp`
- **Why it's slow:** Whenever `from.facing != step.direction` (including `std::nullopt` in dark caves or after route stops), `turn_to()` taps for `40ms`, waits `300ms`, and polls `read_player_facing()` up to 20 times (`100ms` each) if the sprite isn't recognized.
- **Switch transferability:** **High (for structural fixes), Low (for raw timing).**
  - *Transfers cleanly to Switch:* Propagating `end_facing` so `from.facing` isn't unnecessarily `std::nullopt`, and absorbing the 8-frame (`133ms`) turn-in-place directly into `to_wall` holds (`turns_first() = false` when `step.to_wall && !step.warps && !step.surfing`).
  - *Emulator-only:* Cutting `TURN_SETTLE` from `300ms` -> `160ms` and `TURN_FACING_READS` from `20` -> `2` (Switch needs the extra ~`140ms` for video lag + dropped-tap detection).

### C. Pre-Event Quiet Check (`BETWEEN_STEPS_LOOK` = 400ms)
- **Where:**
  - `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryStepInterrupts.h`
  - `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryStepInterrupts.cpp` (`ConsoleScreenLook::look()`, `wait_for_quiet_overworld()`)
- **Why it's `400ms`:** Gives `ScreenEventWatcher` time to observe any delayed fade/dialog arriving over HDMI before starting the next story event.
- **Switch transferability:** **Medium.** On Switch you still need `T_video_lag + margin` (~`200–250ms`), whereas `GbaEmulator` only needs `50–60ms` (3–4 frames).

### D. Planner `to_wall` Bias & Hardcoded `StoryLeg` Bounces
- **Where:**
  - `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.cpp` (`precise_story_route_costs()`, `walk_legs()`)
- **Why it happened:** `OPEN_STOP = 8` and `OPEN_WILD_STOP = 300` make the planner detour to walls; meanwhile `kind: "legs"` events (like `mt_moon_1f`'s `west 24; east 2`) bypass `plan_story_route()` altogether.
- **Switch transferability:** **Medium-High for non-wild rooms/cities, Low for wild caves/grass.**
  - As noted in `PokemonFRLG_AutoStoryPacedRoute.h`, FRLG walks 1 tile per step on land with low drift, so reducing `OPEN_STOP` in indoor rooms/cities (e.g. `OPEN_STOP = 0` or `2` when `!wild`) is mostly transferable to Switch.
  - Folding `west 24; east 2` -> `west 22` in wild caves (`MtMoon_1F`) is **best kept `GbaEmulator`-only** (or only when `OpenLegCheck::TRUST`), because an unexpected wild battle mid-leg on Switch loses exact tile position unless the leg ends at a wall.

---

## 4. Net Expected Savings

- **Per `to_wall` wild leg (e.g. Mt. Moon):** saves **~3.5s** (`3100ms` settle + `~340ms` turn/pre-step), plus **~4s+** per eliminated wall-bounce pair.
- **Per `open` leg (cities/rooms/routes):** saves **~0.85s–1.0s** (`500ms` settle + `140–340ms` turn/pre-step).
- **Estimated Switch-portable fraction:** **~25–35%** of the total speedup (primarily via `to_wall` turn absorption, better `facing` propagation, and lower `OPEN_STOP` in non-wild areas), while the remaining **~65–75%** comes from eliminating capture-card video-lag buffers (`AFTER_WILD_LEG_WATCH`, `AFTER_LEG_WATCH`, `BETWEEN_STEPS_LOOK`, `TURN_SETTLE`).
