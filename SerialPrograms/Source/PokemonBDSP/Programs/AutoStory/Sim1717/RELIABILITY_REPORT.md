# PR #1717 (`[WIP] BDSP Autostory`) Reliability Assessment — Beginning → Floaroma

> [!IMPORTANT]
> **Executive Verdict:** Upstream PR #1717 (`d40920859`, by `Developer-Butters`) passes **10/10** checkpoints (`00.01` → `02.01`, including a full continuous chained run from `PlayerHouse2F` to `FloaromaTown` Center exit) under idealized nominal timing (`walk = 282 ms/tile`, `run = 140 ms/tile`). However, because most long overworld legs rely on open-loop millisecond analog-stick holds without wall resets or position recovery, **only 4/10 checkpoints survive ±3% speed drift**, **only 23/80 (28.8%) survive 1-tile scene-end pose shifts**, **3/10 survive +2.5s fade-in stalls**, and **realistic combined Monte Carlo survival (Beginning → Floaroma) is 0.05% on 1 attempt (~3.43% with 3-attempt checkpoint retries, or 0% if a console has ≥1% fixed speed bias)**. By contrast, our `lab/dev-bdsp-527` planner-based implementation passes **100% of all 5 `FaultSweep_Tests` classes** (`SCENE_END_POSE`, `BATTLE_OPENING`, `GRASS_BATTLES`, `SCENE_EARLY`, `SLOW_FADE_IN`) across the entire `00.00` → `01.11` span (`KNOWN_FAILING` empty).

---

## 1. Methodology & Wiring

We checked out `PokemonAutomation/Arduino-Source#1717` (`d40920859`, *"Improve BDSP AutoStory navigation and detection"*) and wired a native C++ offline simulation harness directly beneath PR #1717's actual C++ checkpoint functions (`00.01` through `02.01`), real BDSP visual detectors, real `ProControllerContext` command dispatch, and unmodified `escape_wild_battle` / `force_repel_expiry` / `clown_dialog_helper` / `walk_through_grass` / `mine_walk_to_endpoints` / `align_to_overworld_landmarks` loops.

### 1.1 Native C++ Offline Harness (`Sim1717` + `Pr1717SimRunner`)

| Component | File | Role |
|---|---|---|
| Hook interface | [Sim1717_Hooks.h](Sim1717_Hooks.h) / [Sim1717_Hooks.cpp](Sim1717_Hooks.cpp) | Thread-local hook pointer & virtual `steady_clock` (`sim1717_steady_now()`), zero effect when inactive |
| Synthetic frames | [Sim1717_Frames.h](Sim1717_Frames.h) / [Sim1717_Frames.cpp](Sim1717_Frames.cpp) | 15 cached 960×540 `ImageRGB32` frames verified **15/15 PASS** at startup against real BDSP detectors (`BlackScreenDetector`, `WhiteScreenDetector`, `ShortDialogDetector`, `BattleDialogDetector`, `BattleMenuDetector` + `selected_option()`, `MenuDetector`, `OverworldDetector`, `TownBannerDetector`, `PoketchButtonDetector`, `SelectionArrowFinder`) |
| World & map DB | [Sim1717_Map.h](Sim1717_Map.h) / [Sim1717_Map.cpp](Sim1717_Map.cpp) | Loads all **22 verified runtime JSON maps** (`PlayerHouse2F` → `FloaromaTown`) via `Common/Cpp/Filesystem/FileIO.h`, seamless border offsets, warps, trainer sightlines from `StoryMaps.cpp`/`StoryPlaces.cpp`, and geometric landmark definitions for all 9 Beginning→Floaroma matchers |
| Physics & fault engine | [Sim1717_World.h](Sim1717_World.h) / [Sim1717_World.cpp](Sim1717_World.cpp) | `Sim1717ProController`, `Sim1717VideoFeed`, synchronous virtual-time `wait_until`/`run_until`, continuous 2D/diagonal analog movement, Unity capsule collision (`PLAYER_HALF_BOX = 0.20`), off-grid single-door snags, 600ms ledge-hop animation lock, BDSP D-pad turn-vs-step mechanics, wandering NPCs, Repel 100-step counter, trainer approach movement, 600s hang watchdog, and all 11 fault knobs |
| CLI runner | [Pr1717Sim_Main.cpp](Pr1717Sim_Main.cpp) | Runs frame self-test, 10 isolated checkpoints, `--chained` continuous carry-over run, `--sweeps` (11 fault classes), and `--mc --mc-trials N` |

### 1.2 Minimal Hooks & Build Note
- **AppleClang / C++23 Build Bug in PR #1717:** [PokemonBDSP_AutoStory_Segment_01.cpp](../StorySegments/PokemonBDSP_AutoStory_Segment_01.cpp#L739) called `correlation.at<float>(...)` inside a function template without the required `template` disambiguator (`correlation.template at<float>(...)`), failing compilation on AppleClang 21.
- **Non-navigation routines hooked (with strict spatial/state precondition checks):** `set_time_fast`, `verify_tutorial_settings`, `select_starter`, `fight_starly`, `fight_trainer_*`, `fight_using_move_slot`, `catch_1_pokemon`, `battle_1_pokemon`, `use_potion_first_pokemon`, `buy_items`, `activate_repel`, `use_escape_rope`, `heal_pokemon`, `use_HM`, and `checkpoint_save` (zero changes to PR #1717's navigation logic or timings).
- **Modeling Disclosures (Fairness Caveats):**
  - **Best-case landmark/mine vision:** Real template matching under varying Switch brightness/shadows/NPC occlusion can produce false positives or larger error than our clean geometric matcher. `LANDMARK_DROP (25%)` and clean landmark accuracy represent an *upper bound* on PR #1717's visual reliability.
  - **Non-navigation routines assumed 100% reliable:** Menu OCR, bag navigation, shopping, Pokémon Center healing, battle move OCR, and clock manipulation are mocked once spatial/state preconditions pass — so real console reliability will be strictly **lower** than the navigation-only reliability measured here.
  - **Bug Catcher Brandon (`Route204North`):** Because `StoryMaps.cpp` notes Brandon's exact tile is uncertain between DP `(20, 12)` and Platinum `(22, 13)` and he paces, we placed his post-battle position north of the player (`(23, 11)`) so `02.01`'s westward hold passes cleanly. Note that if Brandon ever catches the player from the west at `(22, 12)` while the player is against the east cliff `(23, 12)`, Butters' pure-west hold (`northbound_move(-1, 0, 4200ms)`) has no south-detour recovery and gets trapped behind him.

---

## 2. Clean Nominal & Chained Baseline

Calibrated isotropic BDSP world speeds (`walk = 282 ms/tile`, `run = 140 ms/tile`, matching BDSP's 2× Running Shoes speed ratio and independently constrained by `00.01`, `00.04`, `01.02`, and `02.01`'s long runs):

| CP | Segment Description | Isolated Clean | End Position | Virtual Time | Warps | Fades | Banners | Trainers | Clean `fadeMoves` |
|---|---|---|---|---:|---:|---:|---:|---:|---:|
| `00.01` | Bedroom → Verity Lakefront (before starter) | **PASS** | `VerityLakefront (16.21, 14.98)` | 94.2s | 6 | 6 | 2 | 0 | **57** |
| `00.02` | Verity Lakefront → Starter & Starly → Lakefront | **PASS** | `VerityLakefront (17.00, 14.00)` | 12.7s | 0 | 1 | 0 | 0 | 0 |
| `00.03` | Lakefront → Running Shoes → Potions → Rowan's Lab | **PASS** | `RowansLab (7.00, 6.00)` | 151.3s | 1 | 2 | 3 | 0 | **3** |
| `00.04` | Rowan's Lab → Dawn Tour → Mom Parcel → Route 202 Catch | **PASS** | `Route202 (20.00, 26.00)` | 174.5s | 2 | 4 | 5 | 0 | **6** |
| `00.05` | Route 202 Catch → Trainers → Jubilife Dawn | **PASS** | `JubilifeCity (46.00, 60.00)` | 152.1s | 0 | 0 | 1 | 3 | 0 |
| `00.06` | Jubilife Dawn → School → 3 Clowns → Pokétch → Center PC | **PASS** | `JubilifeCenter (11.00, 3.70)` | 152.1s | 3 | 3 | 0 | 0 | **9** |
| `01.01` | Jubilife PC → Route 203 (Barry 1) → Oreburgh Gate → City | **PASS** | `OreburghCity (2.00, 13.00)` | 209.2s | 3 | 3 | 1 | 4 | **8** |
| `01.02` | Oreburgh Tour → Mine (Escape Rope + Roark) → Gym Entry | **PASS** | `OreburghGym (5.00, 24.00)` | 214.2s | 3 | 3 | 0 | 0 | **7** |
| `01.03` | Oreburgh Gym Puzzle → Roark → Coal Badge → Exit Gym | **PASS** | `OreburghCity (26.00, 21.00)` | 44.5s | 1 | 1 | 0 | 1 | 0 |
| `02.01` | Gym Exit → Mart → Gate → R203 → Galactic → R204 → Ravaged Path → Floaroma | **PASS** | `FloaromaTown (16.00, 27.00)` | 220.4s | 8 | 8 | 3 | 4 | **24** |
| **Chained** | **Continuous `00.01` → `02.01` carrying pose & world state** | **PASS (10/10)** | `FloaromaTown (16.00, 27.00)` | **1425.2s** | 27 | 31 | 15 | 12 | 114 |

> [!NOTE]
> Even in **clean nominal runs**, 7 of 10 checkpoints emit movement inputs while the screen is still black/white (`fadeMoves > 0`), showing missing post-fade settle waits before open-loop movement resumes.

---

## 3. Full Fault-Sweep Matrix (10 Checkpoints × 11 Fault Classes)

All 6 `dev-bdsp` `FaultSweep_Tests` / `SimFailureCases_Tests` fault classes plus 5 analog-stick / vision sweeps:

| CP | GRASS+CAVE (every 8) | SLOW_FADE_IN (+2.5s) | BATTLE_OPEN (+2.5s) | SCENE_EARLY (+1 tile) | MISSED_FADE (1 each) | MISSED_BANNER (1 each) | SCENE_END_POSE (8dir) | WANDERERS (10 seeds) | SPEED_DRIFT (-10/-5/-3/+3/+5/+10%) | LOST_DPAD_TAP (5%, /20) | LANDMARK_DROP (25%, /20) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `00.01` | PASS | **FAIL** (`118`) | PASS | PASS | **2/6** | 2/2 | **0/8** | 10/10 | `F / F / F / F / F / F` | 20/20 | 20/20 |
| `00.02` | PASS | PASS (`0`) | PASS | PASS | **0/1** | — | 8/8 | 10/10 | `OK/OK/OK/OK/OK/OK` | 20/20 | 20/20 |
| `00.03` | PASS | **FAIL** (`3`) | PASS | PASS | **0/2** | 3/3 | **3/8** | 10/10 | `OK/OK/OK/OK/OK/OK` | **9/20** | 20/20 |
| `00.04` | PASS | **FAIL** (`6`) | PASS | PASS | **0/4** | 5/5 | **1/8** | 10/10 | `F / F / F / F / F / F` | **9/20** | 20/20 |
| `00.05` | PASS | PASS (`0`) | PASS | PASS | — | 1/1 | **4/8** | 10/10 | `OK/OK/OK/OK/OK/ F` | 20/20 | 20/20 |
| `00.06` | PASS | **FAIL** (`9`) | PASS | PASS | **0/3** | — | **0/8** | **4/10** | `F / F / OK / F / F / F` | **10/20** | 20/20 |
| `01.01` | PASS | **FAIL** (`39`, crash) | PASS | PASS | **0/3** | 1/1 | **2/8** *(hang!)* | 10/10 | `F / F / OK / OK / OK / F` | 20/20 | 20/20 |
| `01.02` | **FAIL** (mine) | **FAIL** (`7`) | PASS | PASS | **0/3** | — | **2/8** | **8/10** | `F / OK / OK / F / F / F` | 20/20 | **0/20** |
| `01.03` | PASS | PASS (`0`) | PASS | PASS | **0/1** | — | **2/8** | 10/10 | `OK/OK/OK/OK/OK/OK` | **1/20** | 20/20 |
| `02.01` | PASS | **FAIL** (`24`) | PASS | PASS | **0/8** | **2/3** | **1/8** | **1/10** | `F / F / F / OK / F / F` | **18/20** | 20/20 |
| **Total** | **9/10** | **3/10** | **10/10** | **10/10** | **2/31 (6.5%)** | **14/15 (93.3%)** | **23/80 (28.8%)** | **83/100 (83%)** | **4/10 survive ±3%** | **147/200 (73.5%)** | **180/200 (90%)** |

### Fine-Grained Speed-Drift Margins (±1% / ±2%)

| CP | `-2%` (`0.98`) | `-1%` (`0.99`) | `0%` (`1.00`) | `+1%` (`1.01`) | `+2%` (`1.02`) | Tightest Unanchored Bottleneck |
|---|---|---|---|---|---|---|
| `00.01` | PASS | PASS | PASS | PASS | **FAIL** | `right 2300ms` from Barry's door to 4-tile Route 201 mouth (`x=17.32`) |
| `00.04` | **FAIL** | **FAIL** | PASS | **FAIL** | **FAIL** | **71-tile** Sandgem→Twinleaf run to 1-tile house door `(20, 21)`: `±1%` = `±0.71` tiles (`x=20.80` / `19.39`) > `±0.42` door width! |
| `00.06` | PASS | PASS | PASS | PASS | **FAIL** | 45-tile Clown 1 → Clown 2 → Clown 3 timed run (`x=35.00` misses Clown 2) |
| `01.02` | PASS | PASS | PASS | **FAIL** | **FAIL** | 24-tile Oreburgh run into 2-tile gap between sign `(45, 56)` & rock `(48, 50)`; mine pulse phasing |
| `02.01` | PASS | **FAIL** | PASS | PASS | PASS | 56-tile `8000ms` north run to Lass Sarah (fails `≤ -3%`) + Ravaged Path locator phase at `-1%` |
| `00.02, 00.03, 00.05, 01.01, 01.03` | PASS | PASS | PASS | PASS | PASS | Wall-aligned or pure D-pad / bounded search |

---

## 4. Realistic Combined Monte Carlo (100 Trials / Checkpoint)

Combined console variance model: **±2% uniform speed drift** (`U[0.98, 1.02]`), **20ms Gaussian stick-hold jitter**, **10% grass / 6% cave wild encounter rate**, **seeded wandering NPCs**, and **5% single-frame landmark dropout** (0% D-pad drop, 0% missed fades, 0% slow fades — i.e., *mild* console noise).

| CP | 1-Attempt Pass | 3-Attempt Retry (`1-(1-p)³`) | Top Failure Reason (out of 100 trials) |
|---|---:|---:|---|
| `00.01` | **82/100 (82.0%)** | 99.4% | `14×`: `Phase 9 (Barry Route 201): dialogue not detected within timeout` |
| `00.02` | **100/100 (100.0%)** | 100.0% | — |
| `00.03` | **98/100 (98.0%)** | 100.0% | `2×`: `Phase 11 (dawn conversation): dialogue not detected within timeout` |
| `00.04` | **15/100 (15.0%)** | **38.6%** | `68×`: `Twinleaf home entrance for parcel: Black transition not detected` |
| `00.05` | **94/100 (94.0%)** | 100.0% | `5×`: `Phase 21 (dawn Jubilife dialog): dialogue not detected within timeout` |
| `00.06` | **22/100 (22.0%)** | **52.5%** | `78×`: `[AutoStory] Clown helper timed out finding dialog` |
| `01.01` | **72/100 (72.0%)** | 97.8% | `23×`: `Dallas fight dialog): dialogue not detected within timeout` |
| `01.02` | **15/100 (15.0%)** | **38.6%** | `36×`: `Mine route failed during: Reach 14 and collect item` (+ Oreburgh sign/rock misses) |
| `01.03` | **100/100 (100.0%)** | 100.0% | — (100% pure D-pad step counts!) |
| `02.01` | **18/100 (18.0%)** | **44.9%** | `27×`: `Route 204 female trainer: No dialogue/battle after bounded down/up recovery` (+ Oreburgh wanderers) |
| **Cumulative (`00.01` → `02.01`)** | **0.05%** | **3.43%** | *Note: 3-attempt math assumes independent speed draws; fixed ≥1% console speed bias yields **0%** even with retries.* |

---

## 5. Top Structural Failure Modes in PR #1717

### 5.1 Critical Infinite-Loop Hang in `oreburgh_gate_nav` (`01.01`)
In [PokemonBDSP_AutoStory_Segment_01.cpp](../StorySegments/PokemonBDSP_AutoStory_Segment_01.cpp#L300-L308), both trainer loops are completely unbounded:
```cpp
while (!try_detect_dialogue(stream, context, "Curtis fight", 1, 3000ms)){
    walk_through_grass(stream, context, 1, 0, 1, "right through gate");
}
```
If a post-scene shift (`SCENE_END_POSE`) or premature cave exit puts the player off Curtis's or Diana's sightline, this loop **hangs indefinitely** until manually aborted (caught by our 600s virtual-time watchdog).

### 5.2 Open-Loop Analog Stick Runs Without Wall Anchoring (`00.04`, `00.01`, `01.02`, `02.01`)
- **`00.04` Sandgem → Twinleaf Home ([Segment_00.cpp](../StorySegments/PokemonBDSP_AutoStory_Segment_00.cpp#L435-L444)):** 71 open tiles west into a 1-tile door `(20, 21)` with zero X-axis wall hits or visual alignment. Mathematically breaks at **`±0.6%` speed error** (15% MC pass rate).
- **`02.01` Jubilife Center → Lass Sarah ([Segment_02.cpp](../StorySegments/PokemonBDSP_AutoStory_Segment_02.cpp#L862-L875)):** 56 tiles north on a single `8000ms` timer (`8000 / 56 = 142.8 ms/tile`, zero headroom for >2% slower speed) — top failure in `02.01` MC (`27×`).
- **`01.02` Oreburgh City → Mine ([Segment_01.cpp](../StorySegments/PokemonBDSP_AutoStory_Segment_01.cpp)):** 24-tile open east run (`3124ms`) threading a 2-tile gap between the Mine sign `(45, 56)` and rock wall `(48, 50)`; breaks at `+2%` speed or Oreburgh wanderer collisions.

### 5.3 Mine Picture Stepper Steps *Past* Targets on a Single Dropped Frame (`01.02`: 0/20 `LANDMARK_DROP`)
In [PokemonBDSP_AutoStory_Segment_01.cpp](../StorySegments/PokemonBDSP_AutoStory_Segment_01.cpp#L915-L945), `mine_walk_to_endpoints` requires 3 consecutive matching frames (`matching_frames >= 3`), **but any non-matching frame immediately resets `matching_frames = 0` AND issues another forward joystick pulse** instead of re-sampling stationary frames. Under 25% frame dropout, this causes **0/20 pass** (the player walks straight past the target tile). Additionally, `movements++` counts pulses interrupted by wild encounters, causing `GRASS+CAVE (every 8)` to exhaust `max_movements` (`FAIL`).
*Contrast:* PR #1717's newer overworld locators (`align_to_overworld_landmarks`, `align_and_enter_jubilife_center`, `align_and_talk_to_poketch_man`) wait stationary on misses and passed **20/20** on `LANDMARK_DROP (25%)`.

### 5.4 Wandering NPC Collisions With No Sidestep Recovery (`00.06`: 4/10, `02.01`: 1/10, `01.02`: 8/10)
- **`00.06` Pokétch Clowns ([Segment_00_tools.cpp](../Utils/PokemonBDSP_AutoStory_Segment_00_tools.cpp#L76-L104)):** `clown_dialog_helper` walks in a single fixed direction (`DPAD_RIGHT`/`UP`/`LEFT`). When `clown_mart` (`[54..56, 32..34]`) wanders 1 row off `y = 33`, the helper walks past the clown into the wall and times out (`78/100` MC failures).
- **`02.01` Oreburgh City ([Segment_02.cpp](../StorySegments/PokemonBDSP_AutoStory_Segment_02.cpp#L749-L753)):** Blind west run from Oreburgh Mart `(29, 11)` to Barry `(6, 13)` passes through `guitarist_20`'s wander corridor `(22..24, 10..16)`, blocking the run on **9/10 wanderer seeds**.

### 5.5 Missing Post-Warp Settle & Single-Signal Fade Dependency (`SLOW_FADE_IN`: 3/10, `MISSED_FADE`: 2/31)
- 7/10 checkpoints start sending joystick holds before warp fade-ins complete (worst in `00.01`: 57 fade-in moves clean, 118 under `+2.5s`; `01.01` hard-crashes with `"HM Rock Smash Dialog: dialogue not detected"` under `+2.5s`).
- 29 of 31 warp transitions (`93.5%`) rely exclusively on a single black/white screen watcher with no banner or position fallback; suppressing any single fade crashes the checkpoint.

### 5.6 Unrecovered Single D-Pad Drop (`01.03`: 1/20, `00.03`: 9/20, `00.04`: 9/20, `00.06`: 10/20)
- `01.03` (`gym_Puzzle_01`) is **100% immune** to speed drift and wanderers because it uses pure counted D-pad steps — **but a single 5% D-pad drop anywhere across its ~56 exact steps causes 19/20 failures (5% pass)** because there is no wall-reset or position check between Roark and the exit door.

### 5.7 Carry-Over Risk Between `01.01` and `02.01` on `Route203`
`01.01` crosses `Route203` along `y = 18`, 1 tile south of `lass_2` `(43, 17)` (facing west, range 5). When `02.01` returns west from Oreburgh Gate and hops the ledge at `(40, 15) → (40, 16)`, any slightly shorter ledge lock or extra south step lands on `(40, 17)` inside undefeated `lass_2`'s sightline, crashing `force_repel_expiry` (`"Unexpected dialogue; renewal choice not confirmed"`).

---

## 6. Side-by-Side Comparison: PR #1717 vs. Our `dev-bdsp` (`lab/dev-bdsp-527`)

| Reliability Dimension (Beginning → Floaroma) | PR #1717 (`Developer-Butters`, `d40920859`) | Our `dev-bdsp` (`lab/dev-bdsp-527`) |
|---|---|---|
| **Architecture** | Recorded millisecond analog-stick holds + targeted template-match locators (`00.06`, `01.02`, `02.01`) | Map-based `RoutePlanner` (wall-anchored legs, `CountedWallLeg`, automatic replanning & recovery) |
| **Clean Nominal (`00.01` → Floaroma)** | **10/10 PASS** (isolated & chained) | **PASS** (`00.00` → `01.11`) |
| **`GRASS_BATTLES` / `CAVE_BATTLES` (every 8)** | **9/10** (`01.02` mine fails) | **100% PASS** (`KNOWN_FAILING` empty) |
| **`SLOW_FADE_IN` (+2.5s)** | **3/10** (`7/10` send fade-in moves; `01.01` crashes) | **100% PASS** (`KNOWN_FAILING` empty) |
| **`BATTLE_OPENING` (+2.5s)** | **10/10** | **100% PASS** (`KNOWN_FAILING` empty) |
| **`SCENE_EARLY` (+1 tile)** | **10/10** | **100% PASS** (`KNOWN_FAILING` empty) |
| **`SCENE_END_POSE` (1-tile shifts)** | **23/80 (28.8%)** (`01.01` hangs indefinitely) | **100% PASS** (`KNOWN_FAILING` empty) |
| **`MISSED_FADE` / `MISSED_BANNER`** | **2/31 (6.5%)** fades / **14/15 (93.3%)** banners | Fails only on the 10 documented `ONE_SIGNAL_STEPS` (all two-signal warps recover) |
| **`WANDERERS`** | **83/100** (`00.06`: 4/10, `01.02`: 8/10, `02.01`: 1/10) | Passes all except 1 known `01.08` Route 204 wanderer (`(12..14, 2)`) |
| **`SPEED_DRIFT` (±3%)** | **4/10** (`00.04` breaks at `±1%`!) | Architecturally immune on wall-anchored legs (`open_legs = STEP_TAPS`) |
| **Realistic Combined MC (1-attempt / 3-attempt)** | **0.05% / 3.43%** (0% under ≥1% fixed bias) | — *(D-pad/wall-anchored architecture has no open-loop analog drift)* |

---

## 7. Where PR #1717 Shines & Actionable Recommendations

PR #1717 demonstrates several genuinely strong engineering patterns:
1. **Overworld multi-landmark locators (`00.06`, `02.01`):** Passed **20/20** under 25% frame dropout because they hold stationary on missed frames and require 3 consistent confirmations before acting.
2. **Battle opening & early-scene watchers:** Passed **10/10** on both `BATTLE_OPENING (+2.5s)` and `SCENE_EARLY (+1 tile)`.
3. **Pure D-pad Gym Puzzle (`01.03`) and Starter/Starly (`00.02`):** Passed **100/100** in Monte Carlo.

Highest-ROI hardening fixes if keeping PR #1717's architecture:
1. **Eliminate the 71-tile unanchored `00.04` Twinleaf run** (wall-align on Twinleaf's west or house fence before walking east to `(20, 21)` — single biggest MC win: `15% → 90%+`).
2. **Fix `mine_walk_to_endpoints` (`01.02`)** to wait stationary on missed frames (matching `align_to_overworld_landmarks`) and stop counting battle-interrupted pulses toward `max_movements`.
3. **Bound the `while (!try_detect_dialogue(...))` loops in `oreburgh_gate_nav` (`01.01`)** to prevent infinite hangs.
4. **Add 2D search to `clown_dialog_helper` (`00.06`)** and a wall-anchored/extended approach to Lass Sarah (`02.01`) + wanderer retry in Oreburgh City (`02.01`).
5. **Wait for fade-in completion** (`BlackScreenWatcher::GONE`) on the 7 checkpoints with `fadeMoves > 0` (especially `00.01` and `01.01`), and fix the Clang `template` keyword bug at `Segment_01.cpp:739`.

---

## 8. Reproducing These Results

```bash
# Build native simulation runner
cmake --build build --target Pr1717SimRunner -j 16

# Run 10/10 clean + chained Beginning -> Floaroma
./build/Pr1717SimRunner --maps <path-to-pokesim>/data/games/bdsp/maps/runtime --chained

# Run full 11-class fault sweep matrix
./build/Pr1717SimRunner --maps <path-to-pokesim>/data/games/bdsp/maps/runtime --sweeps

# Run 100-trial realistic Monte Carlo
./build/Pr1717SimRunner --maps <path-to-pokesim>/data/games/bdsp/maps/runtime --mc --mc-trials 100
```
