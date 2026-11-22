# BDSP & FRLG AutoStory: Reusability Audit of Remaining Game-Specific Code

> [!NOTE]
> Analysis based on `origin/bdsp/core-adopt` (PR #772) and `origin/frlg/core-adopt` (PR #779) diffed against the top of the shared seam stack `origin/autostory/navigation-sim-seam` (PR #771), alongside `origin/bdsp/lean` (PR #753) and `origin/frlg/lean` (PR #652).
>
> **Single source of truth for live seam PR status & core decisions (`S1`–`S17`, `E1`–`E10`):** [`autostory/SHARED_CORE.md`](https://github.com/dayfine/Arduino-Source/blob/agents/autostory/SHARED_CORE.md) on the `agents` branch.
> **Navigation seam layout & rules:** [`SerialPrograms/Source/Pokemon/Navigation/README.md`](https://github.com/dayfine/Arduino-Source/blob/autostory/navigation-sim-seam/SerialPrograms/Source/Pokemon/Navigation/README.md) (PR #771).
> **GitHub milestone tracker:** [#144](https://github.com/dayfine/Arduino-Source/issues/144).

---

## 0. Landing Priority: Finish the Existing Seam Stack First

Before starting any new extraction PR from Sections 2 and 4 below, **prioritize landing the existing 11-PR stack** (`#572 → #667 → #648 → #653 → #741 → #762 → #764 → #768 → #769 → #770 → #771`) plus `#772`/`#779` (`core-adopt`):

- **Already built & mostly green:** 9 of the 11 seam PRs (`#572`, `#667`, `#648`, `#653`, `#762`, `#764`, `#768`, `#769`, `#770`) are `CLEAN`; only `#741` and `#771` have a single Windows CI failure, and `#779` has Code Health + Ubuntu failures (`#772` has 0 failing checks).
- **Immediate win (~41k lines / ~340 files):** Landing `#572`–`#771` strips the duplicated shared infrastructure out of both `bdsp/lean` (`#753`) and `frlg/lean` (`#652`) and puts the Phase 1 target primitives (`#762` Maps, `#764` WallRecovery/RouteScorer, `#769` Trace/Replay, `#771` Sim harness) on `main`.
- **Avoids rebase churn:** Starting Phase 1–4 extractions before `#771` merges would force continuous rebasing across 11+ stacked PRs.
- **Parallelizable:** `#728` (`ns/system-keyboard`, based on `main`) is independent and can land in parallel once its CI check is fixed.

---

## 1. Where Things Stand After `core-adopt`

Landing the shared core & navigation seams (`#648` → `#653` → `#667` → `#741` → `#762`–`#771`) removes **~41k lines / ~340 files** of shared infrastructure from both lean branches. However, **even on `bdsp/core-adopt` and `frlg/core-adopt`**, massive game-local stacks remain:

| Branch (diff vs `#771`) | Directory | Files | Lines |
|---|---|---:|---:|
| `bdsp/core-adopt` (`#772`) | `PokemonBDSP/Programs/AutoStory/` | 232 | +43,207 |
| `frlg/core-adopt` (`#779`) | `PokemonFRLG/Programs/AutoStory/` | 450 | +96,681 |
| `frlg/core-adopt` (`#779`) | `PokemonFRLG/Overworld/` | 97 | +22,185 |
| **Combined remaining** | | **779** | **+162,073** |

Much of this remaining code falls into **six reusable domains** that are either (a) still duplicated across BDSP and FRLG despite existing seams, or (b) built out in FRLG for a full playthrough and directly needed by BDSP (Gyms 2–8) and future 2D/grid games (Emerald, HGSS, etc.).

---

## 2. Reusable Extraction Candidates (Ranked by Leverage)

| # | Candidate Seam | BDSP (`#772`) | FRLG (`#779`) | Combined | Duplicated Today? |
|---:|---|---:|---:|---:|---|
| 1 | **Belief-State Route Executor, Stop-Candidate Narrowing & Re-anchoring** | 24 files (`5,899 L`) | 40 files (`10,665 L`) | **64 files (`16,564 L`)** | **Yes (heavy)** |
| 2 | **Offline Grid / Story Simulation Engine & Fault Sweeps** | 25 files (`7,174 L`) | 19 files (`7,750 L`) | **44 files (`14,924 L`)** | **Yes (heavy)** |
| 3 | **Story Step Dispatch, Replay IO, Embedded Plan Registry & Resource Check** | 26 files (`4,983 L`) | 29 files (`5,015 L`) | **55 files (`9,998 L`)** | **Yes (moderate–heavy)** |
| 4 | **Map World Stitching & Runtime Map-Pack Loader** | 17 files (`2,792 L`) | 7 files (`1,904 L`) | **24 files (`4,696 L`)** | **Yes (direct seam gap)** |
| 5 | **Bag Scrolling List, Healing Math, Mart Planning & Party Roster Cache** | 13 files (`2,962 L`) | 44 files (`12,699 L`) | **57 files (`15,661 L`)** | **Partial (FRLG ahead)** |
| 6 | **Turn-Based Battle Policy, Planned-Fight Engine & Move-Learn Policy** | 14 files (`2,590 L`) | 70 files (`17,247 L`) | **84 files (`19,837 L`)** | **Partial (FRLG ahead)** |
| | **Total in reusable domains** | **119 files (`26,400 L`)** | **209 files (`55,280 L`)** | **328 files (`81,680 L`)** | |

---

### Candidate 1: Belief-State Route Executor, Candidate Narrowing & Re-anchoring (`16.6k L`)

**What's duplicated:**
- **BDSP:** `Utils/PokemonBDSP_AutoStoryPlannedRoute.cpp` (`989 L`), `Utils/PokemonBDSP_AutoStoryRouteChoice.cpp` (`391 L`), `Utils/PokemonBDSP_AutoStoryRouteWalk.cpp` (`333 L`), `Data/PokemonBDSP_AutoStoryRoutePlanner.cpp` (`337 L`), `Data/PokemonBDSP_AutoStoryWallRecovery.cpp` (`165 L`), `Utils/PokemonBDSP_AutoStoryWalk.cpp` (`993 L`) + tests.
- **FRLG:** `Overworld/PokemonFRLG_OverworldRecovery.cpp` (`1,036 L`), `Overworld/PokemonFRLG_OverworldRecoveryWalk.cpp` (`913 L`), `Overworld/PokemonFRLG_OverworldReanchor.cpp` (`688 L`), `Overworld/PokemonFRLG_OverworldRoute.cpp` (`989 L`), `Overworld/PokemonFRLG_OverworldRobustRoute.cpp` (`205 L`), `Overworld/PokemonFRLG_OverworldWalk.cpp` (`993 L`) + tests.

**Why it's still huge after `#764`/`#741`:**
- The shared seams extracted the **inner primitives** (`Pokemon_LegPlanner`, `Pokemon_RobustLegPlanner`, `Pokemon_WallRecovery`, `Pokemon_WanderRecovery`, `Pokemon_LegWalker`), and BDSP's `AutoStoryWallRecovery.cpp` doesn't even include `Pokemon_WallRecovery.h` yet (`0` shared includes).
- Both games still independently implement the **outer closed-loop belief-state route runner**:
  1. Walk a sequence of planned legs while maintaining the candidate tile set `S` (belief state).
  2. When an interrupt occurs (wild battle, trainer sight, dialog, unexpected stop), narrow `S` from elapsed hold duration + trainer sight intersections (`stop_candidates`).
  3. If `|S| == 1`, replan directly to the goal; if `|S| > 1`, search 0–3 wall holds (`plan_reanchor`) that collapse every candidate in `S` to a single anchored tile, then walk back onto the route; if no unambiguous anchor exists, fail safely without guessing.
  4. Handle open-leg shortfall checks before wall holds / warps / spinners and route choice scoring (`RouteChoice` / `RouteCosts`).

**Shared seam design:**
- Extract a shared `Pokemon::BeliefRouteRunner` (in `Pokemon/Navigation/Grid/Walker/` or `Planning/`) parameterized by a small `GameWalkAdapter` (step timing, how to hold a wall leg vs tap an open leg, how to clear interrupts/trainers, and optional facing reader).
- Unify `RouteCosts` / `PlanCosts` and migrate BDSP's `AutoStoryWallRecovery` & `RoutePlanner` onto `Pokemon_WallRecovery` + `Pokemon_ConnectedMaps`.

---

### Candidate 2: Offline Grid / Story Simulation Engine & Fault-Sweep Harness (`14.9k L`)

**What's duplicated:**
- **BDSP:** `Simulation/PokemonBDSP_AutoStoryGridSim.cpp` (`614 L`), `Simulation/PokemonBDSP_AutoStorySim.cpp` (`918 L`), `Simulation/PokemonBDSP_AutoStorySimScene.cpp` (`147 L`), `Simulation/PokemonBDSP_AutoStorySimTriggers.cpp` (`118 L`), plus ~5.4k lines of sim & fault-sweep tests.
- **FRLG:** `Overworld/PokemonFRLG_OverworldSim.cpp` (`987 L`), `Overworld/PokemonFRLG_OverworldSimField.cpp` (`497 L`), `Overworld/PokemonFRLG_OverworldSimTaps.cpp` (`335 L`), `Programs/AutoStory/PokemonFRLG_AutoStoryRowSim.cpp` (`918 L`), `PokemonFRLG_AutoStoryPaceSim.cpp` (`61 L`), plus ~4.9k lines of sim & fault-sweep tests.

**What's actually common:**
- Both build an offline overworld simulator that advances a player across `MapData` / `ConnectedMaps` with:
  - Wall/NPC/sign collision, ledges, warps (with fade durations), trainer sight triggers, and scripted/seeded wild-encounter injection.
  - Delayed screen-event emission so the real `ScreenEvents` watchers observe (or miss!) frames exactly as on hardware.
  - A story-row simulator (`AutoStorySim` 918L vs `AutoStoryRowSim` 918L) that runs authored checkpoints on `Pokemon::run_checkpoints()` with stubbed/simulated IO and runs seeded fault sweeps (`AutoStoryFaultSweep_Tests`).
- **What actually differs:** only the **kinematics model** (BDSP's sub-tile thousandths-of-a-tile analog stick drift vs FRLG's whole-tile 16-frame D-pad/bike/spinner cadence) and game-specific battle/scene stubs.

**Shared seam design:**
- Extract `Pokemon::GridWorldSim` (walls, warps, trainer sight, wild encounter scheduling, screen-event timeline, trace recording) with a pluggable `MovementPhysics` policy (grid-locked vs sub-tile analog).
- Extract a shared `Pokemon::StoryRowSim` harness that drives `Pokemon::run_checkpoints()` + `Pokemon_SimSweep` over any game's `StoryBackend`.

---

### Candidate 3: Story Table, Step Dispatch, Replay IO, Embedded Plans & Resource Checks (`10.0k L`)

**What's duplicated:**
- **BDSP:** `PokemonBDSP_AutoStoryFiles.cpp` (`366 L`), `PokemonBDSP_AutoStoryExecutor.cpp` (`364 L`), `PokemonBDSP_AutoStoryStepIO.cpp` (`409 L`), `PokemonBDSP_AutoStoryWalkSteps.cpp` (`855 L`), `PokemonBDSP_AutoStoryTable.cpp` (`74 L`), `PokemonBDSP_AutoStoryResources.cpp` (`64 L`), `Utils/PokemonBDSP_AutoStoryLegTraceIO.cpp` (`123 L`), `Actions/PokemonBDSP_AutoStoryStepTrace.cpp` (`108 L`), `PokemonBDSP_AutoStoryInterrupts.cpp` (`73 L`).
- **FRLG:** `PokemonFRLG_StoryReplay_Executor.cpp` (`522 L`), `PokemonFRLG_StoryReplay_IO.cpp` (`435 L`), `PokemonFRLG_AutoStoryStepTypes.cpp` (`615 L`), `PokemonFRLG_AutoStoryStepTable.cpp` (`108 L`), `PokemonFRLG_AutoStoryTable.cpp` (`425 L`), `PokemonFRLG_AutoStoryResources.cpp` (`88 L`), `PokemonFRLG_AutoStoryFailureClasses.cpp` (`178 L`), `PokemonFRLG_AutoStoryStepInterrupts.cpp` (`216 L`), `Overworld/PokemonFRLG_OverworldLegTrace.cpp` (`80 L`), `Data/JsonWriter.cpp` (`107 L`).

**What's actually common:**
- **Embedded checkpoint JSON + dev-file override loader (`AutoStoryFiles` vs `StoryReplay_IO`):** both load embedded JSON plans, apply developer override files, filter `dev_only` rows by `STATIC_GLOBALS.DEVELOPER_MODE`, and validate checkpoint tables.
- **Generic grid-step execution (`AutoStoryWalkSteps` + `AutoStoryExecutor` vs `StoryReplay_Executor` + `AutoStoryStepTypes`):** standard actions (`walk`, `route`, `warp`, `face`, `talk`, `scene`, `answer`, `battle`, `heal`, `save`, `use_item`) are parsed and dispatched separately in each game instead of extending `Pokemon_GridStoryActions`.
- **Resource existence verifier (`AutoStoryResources` + `_Tests`):** both games have identical resource-path manifest checks verifying every file under `RESOURCE_PATH()` exists at test time.
- **Failure classification & leg-trace JSON IO:** `PokemonFRLG_AutoStoryFailureClasses.cpp` maps generic step failures (`bad_args`, `bad_target`, `bad_legs`, `unknown_item`, etc.) that belong in `Pokemon_StoryFailureClasses`; `LegTraceIO` / `OverworldLegTrace` both wrap `Pokemon_LegTrace` with identical JSON/file IO.
- **Top-level Program Shell (`PokemonBDSP_AutoStory.cpp` 432L vs `PokemonFRLG_AutoStory.cpp` 958L):** range options, preflight, stats tracking, notifications, and retry loop wiring share ~60% boilerplate.

---

### Candidate 4: Map World Stitching & Runtime Map-Pack Loader (`4.7k L`)

**What's duplicated:**
- **BDSP:** `Data/PokemonBDSP_AutoStoryWorld.cpp` (`367 L`), `Data/PokemonBDSP_AutoStoryStoryMaps.cpp` (`306 L`), `Data/PokemonBDSP_AutoStoryMaps.cpp` (`77 L`), `PokemonBDSP_AutoStoryPlaces.cpp` (`313 L`), `Data/PokemonBDSP_AutoStoryCoordEvents.cpp` (`119 L`) — **0 includes** of `Pokemon_MapData.h` / `Pokemon_ConnectedMaps.h` in `AutoStoryWorld` and `AutoStoryStoryMaps`.
- **FRLG:** `Overworld/PokemonFRLG_OverworldMap.cpp` (`375 L`), `Overworld/PokemonFRLG_OverworldMaps.cpp` (`334 L`) — **0 shared includes**.

**Why this is a low-hanging win:**
- `Pokemon_MapData.h`, `Pokemon_RuntimeMap.h`, `Pokemon_ConnectedMaps.h`, and `Pokemon_MapGates.h` **already exist** in `#762` (`autostory/navigation-map-seam`), yet both games still keep parallel map-loading wrappers, `in_bounds` / `tile_at` / `elevation_at` queries, hardcoded map-name arrays (`MAP_NAMES[]`), and multi-map world stitching (`StoryWorld` vs `OverworldMaps`).
- Finishing the migration onto `Pokemon::ConnectedMaps` + a shared `Pokemon::MapPackLoader` (reading `runtime-manifest.json` / resource dir by game name) eliminates ~2–3k lines immediately with minimal design risk.

---

### Candidate 5: Bag Scrolling, Healing Math, Mart Planning & Party Roster Cache (`15.7k L`)

**Where it lives today:**
- **FRLG (full game):** `AutoStoryPocketList.cpp` (`141 L`), `AutoStoryItemsPocket.cpp` (`145 L`), `AutoStoryItemsPocketChange.cpp` (`105 L`), `AutoStoryItemUse.cpp` (`455 L`), `AutoStoryHealing.cpp` (`488 L`), `AutoStoryMart.cpp` (`565 L`), `AutoStoryMartSell.cpp` (`130 L`), `AutoStoryParty.cpp` (`546 L`), `AutoStoryPartyRoster.cpp` (`208 L`), `AutoStoryPcStorage.cpp` (`516 L`), `AutoStoryTeach.cpp` (`391 L`), `AutoStoryFieldMoveUsers.cpp` (`138 L`).
- **BDSP (1 gym so far):** `Actions/PokemonBDSP_AutoStoryBag.cpp` (`412 L`), `Actions/PokemonBDSP_AutoStoryCenter.cpp` (`519 L`), `Actions/PokemonBDSP_AutoStoryMenuActions.cpp` (`356 L`), `Actions/PokemonBDSP_AutoStorySettings.cpp` (`650 L`).

**What is genuinely game-independent pure logic:**
1. **Scrolling list reader (`PocketList`):** scrolling a fixed-viewport list (`N` visible rows, cursor stops at row `K`, list scrolls underneath until `CANCEL` or bottom repeat) is identical across GBA/DS/Switch Bag, Mart, and TM menus — only `read_shown(row)` and `press_down()` are game-specific.
2. **Inventory & healing solver (`AutoStoryHealing` + `AutoStorySupplies`):** given current HP/max HP/PP and available healing items (`Potion`=20, `Super Potion`=50, `Hyper Potion`=200, `Full Restore`, `Elixir`), choosing the minimal-waste item sequence is 100% game-independent across all generations.
3. **Mart shopping planner (`AutoStoryMart` / `AutoStoryMartSell`):** computing budget, items to sell to reach target cash, and quantities to buy is pure arithmetic independent of UI.
4. **Party roster cache (`PartyCache`):** caching `(slot -> species, field_moves)` across a checkpoint and invalidating on PC/catch/swap works identically in BDSP, FRLG, Emerald, etc.
5. **Move-teaching priority policy (`AutoStoryTeach`):** deciding whether to learn a new move and which existing move slot to replace is pure policy; only `AutoStoryTeachScreens` is game-specific UI.

---

### Candidate 6: Turn-Based Battle Policy, Planned-Fight Engine & Risk Model (`19.8k L`)

**Where it lives today:**
- **FRLG:** `AutoStoryBattlePolicy.cpp` (`843 L`), `AutoStoryBattleDecisions.cpp` (`739 L`), `AutoStoryBattleTrack.cpp` (`329 L`), `AutoStoryPlannedBattle.cpp` (`418 L`), `AutoStoryPlannedPolicy.cpp` (`492 L`), `AutoStoryPlannedRisk.cpp` (`79 L`), `AutoStoryBattleData.cpp` (`287 L`), `AutoStoryWildBattle*`, plus `AutoStoryGen3Damage.cpp` (`81 L`).
- **BDSP:** `Actions/PokemonBDSP_AutoStoryBattle.cpp` (`401 L`), `Actions/PokemonBDSP_AutoStoryBattleMoves.cpp` (`372 L`), `Actions/PokemonBDSP_AutoStoryWildBattle.cpp` (`478 L`), `Utils/PokemonBDSP_AutoStoryWildBattlePolicy.cpp` (`42 L`).

**What is genuinely reusable:**
- **Turn-state tracker (`BattleTrack`):** tracking foe HP %, stat stages (`-6..+6`), status, Mist/Safeguard/weather turn counters, and item spend budgets across a fight or multi-fight chain (e.g. Elite Four).
- **Planned-fight policy interpreter (`PlannedPolicy` + `PlannedRisk`):** evaluating data-driven fight plans (`foe_threats`, setup thresholds, heal-before-KO risk check, PP fallback) — note that `AutoStoryGen3Damage.cpp` is only **81 lines**, whereas the policy/decision/tracking machinery around it is **>3,000 lines of prod code (+ >5,000 lines of tests)**!
- **Wild-battle policy (`WildBattlePolicy`):** run vs fight vs catch-target decision based on species/level/grinding goal.

**What stays game-specific:**
- Generation-specific damage formula (`Gen3Damage` 81L vs Gen 4+ physical/special split), badge boosts, and battle-menu UI readers/controllers (`BattleMenuReader`, `BattleBag` UI, `BattleSwitch` UI).

---

## 3. What Should Stay Truly Game-Specific (Do Not Extract)

| Category | Examples | Why it stays game-specific |
|---|---|---|
| **Visual Detectors & OCR** | `Detect/*` in BDSP (`1.7k L`), `Inference/*` + `PlayerFacingReader` + `PokemonCenterFloorReader` in FRLG (`4.6k L`) | Tightly coupled to pixel coordinates, palettes, fonts, and sprites of each game. |
| **Story Events & Puzzles** | BDSP `EventsJubilife/Lake/Oreburgh/Route204/Sandgem`, `AutoStoryNewGame`, `AutoStorySettings`; FRLG `AutoStoryHideout`, `TrashCans`, `Boulders`, `Slope`, `HallOfFame`, `ViridianMart` | One-off scripted cutscenes, gym puzzles, and game-specific UI flows. |
| **Game-Specific Movement Quirks** | BDSP analog-stick drift & lit-doorway detection; FRLG bike taps, Surf dialog, spinner pads, stairs | Hardware/engine mechanics unique to GBA vs Unity/Switch BDSP. |
| **Authored Plans & Static Data** | `StoryData/*.plan.json`, `LeagueFightData`, `LeaguePlanData`, `BattleData` | Content data per game (though the *schema* and *loader* are shared). |
| **RNG Manipulation** | `PokemonBDSP_AutoStoryStarterRng`, `PokemonFRLG_AutoStoryRngLoop/Options` | Tightly coupled to each game's RNG algorithm & boot timing (only the handoff contract `Pokemon_StoryRngHandoff` is shared). |

---

## 4. Recommended Extraction Roadmap

If the goal is to shrink both lean branches cleanly **before** adding BDSP Gyms 2–8 or Emerald/SV AutoStory:

1. **Phase 1 — Finish Adopting Already-Built Seams (Fastest win, lowest risk)**
   - Migrate BDSP `AutoStoryWorld` / `AutoStoryStoryMaps` / `AutoStoryWallRecovery` and FRLG `OverworldMap` / `OverworldMaps` onto `#762` (`Pokemon_MapData`, `Pokemon_ConnectedMaps`) + `#764` (`Pokemon_WallRecovery`), plus a shared `MapPackLoader`.
   - Unify `AutoStoryResources`, `AutoStoryFailureClasses`, and `LegTraceIO` into `Pokemon/AutoStory/`.
   - *Est. savings:* **~5–7k lines** of pure duplication removed.

2. **Phase 2 — Extract Shared `BeliefRouteRunner` & `GridWorldSim`**
   - Pull the closed-loop candidate-set route runner + re-anchoring loop out of `PokemonBDSP_AutoStoryPlannedRoute` and `PokemonFRLG_OverworldRecovery` / `OverworldReanchor`.
   - Pull the common tile/warp/trainer/wild-battle simulation core out of `AutoStoryGridSim` / `OverworldSim` and `AutoStorySim` / `AutoStoryRowSim`.
   - *Est. savings:* **~15–20k lines** across prod + tests.

3. **Phase 3 — Extract Shared `StoryStepRegistry` + `EmbeddedStoryFiles`**
   - Unify checkpoint JSON/dev-file loading (`AutoStoryFiles` vs `StoryReplay_IO`) and standard grid-action step dispatch (`AutoStoryWalkSteps` / `AutoStoryExecutor` vs `StoryReplay_Executor` / `AutoStoryStepTypes`).
   - *Est. savings:* **~4–6k lines**.

4. **Phase 4 — Promote FRLG's Pure RPG Logic Before Scaling BDSP Past Gym 1**
   - Extract `PocketList`, `HealingPlanner`, `MartPlanner`, `PartyCache`, `MoveLearnPolicy`, and `BattlePolicy` / `BattleTrack` / `PlannedPolicy` into `Pokemon/AutoStory/` (with a pluggable damage-calc + UI adapter).
   - *Payoff:* avoids re-writing ~15–20k lines of battle/bag/healing/mart/party logic when BDSP moves past Gym 1 or when Emerald/HGSS AutoStory begins.

---

## 5. Sequencing Against Planned-but-Not-Done Work

Mapping open issues and planned-but-not-done items from `SHARED_CORE.md`, `plza/UPSTREAM_PLAN.md`, and GitHub onto the candidates above shows where doing planned work **first** vs **after** extraction saves double effort:

| Planned / Open Item | Source | Overlaps Candidate | Recommended Order |
|---|---|---|---|
| **Delete old runner & legacy adapters** (`E1` / `B5`: `StorySegments`, `StoryStepExecutor`, `TableExecutor`, `StoryRun`, `RunnerExecutor`) | `SHARED_CORE.md` (`E1`), migration design (`B5`) | **#3** (Step Dispatch / Executor / Table) | **Do BEFORE Phase 3** (right after `#772`/`#779` land). Deletes frozen old-runner glue so Phase 3 doesn't refactor throwaway code. |
| **`K1`–`K2` (`#728` keyboard) + `E2` FRLG fit check (`#779` green) + `C1`–`C9` core freeze** | `SHARED_CORE.md` (`S17`, `E2`), `UPSTREAM_PLAN.md` | Base contract for **#3, #5, #6** | **Do FIRST.** Stabilizes `StoryEngine` signatures before layering new shared abstractions on top. *(No need to wait on PLZA game PRs `P1`–`P10`.)* |
| **`#159` Shared overlay writer & run records** (drops BDSP's `AutoStoryOverlayLog`) + **`#160`** (grid-free route log) | GitHub `#159`, `#160` | **#3** (`OverlayLog`, `LegTraceIO`, `StepTrace`) | **Do WITH Phase 1.** Small, self-contained win that deletes BDSP's duplicate `OverlayLog` and locks the log/event shape before unifying trace IO. |
| **`#154` (`map_mismatch` outcome)** + **`#163` (confirm row end-state & re-anchor)** + **`#164` (BDSP turn-tap check)** | GitHub `#154`, `#163`, `#164` | **#1** (`BeliefRouteRunner` & Re-anchoring) | **Extract Candidate #1 FIRST** (except `#164`, a small BDSP-only bugfix). Implementing `#154`/`#163` beforehand means writing re-anchoring and `map_mismatch` twice (BDSP `PlannedRoute` + FRLG `OverworldRecovery`). |
| **`#162` (sim timing/placement variance)** + **`#203` / `#217` (BDSP sim pace, tap replay, map-crossing stall)** | GitHub `#162`, `#203`, `#217` | **#2** (`GridWorldSim` / `StoryRowSim`) | **Extract Candidate #2 FIRST for `#162`.** Variance injection lands once for both BDSP and FRLG; `#203`/`#217` then plug into `GridWorldSim`'s `MovementPhysics` hook instead of expanding a BDSP-only sim. |
| **Postgame & feature programs:** `#780` (Legendary Catch), `#781` (Mail Glitch / Shiny Dex), `#146` (Sevii + Mewtwo), `#137+` (BDSP Gyms 2–8) | GitHub `#780`, `#781`, `#146`, `#137` | **#5 & #6** (`PocketList`, `PartyCache`, `HealingPlanner`, `BattlePolicy`) | **Extract Phase 4a (`PocketList` + `PartyCache` + `HealingPlanner`) BEFORE scaling these.** Prevents a third copy of Bag/Party scanning across `#780`, `#781`, and BDSP Gyms 2–8 (FRLG's `PocketList` is already active in `#785`). |

### Combined Execution Sequence
1. **Unblock & clean the base:** land `#728` (`K1`–`K2`), fix CI on `#741`/`#771`/`#779`, merge `#572`–`#771` + `#772`/`#779`, then run **`E1`/`B5` (delete old runner)** + **`#159`** + **Phase 1** (delete bypassed map & wall-recovery wrappers).
2. **Nav & sim unification:** run **Phase 2** (`BeliefRouteRunner` + `GridWorldSim`), then land **`#154`, `#162`, `#163`** once in the shared layer.
3. **RPG utilities before postgame / BDSP Gym 2+:** run **Phase 4a** (`PocketList`, `PartyCache`, `HealingPlanner`), then build **`#780`, `#781`, `#146`, `#137+`** on top of them, followed by **Phase 4b** (`BattlePolicy`/`PlannedPolicy`) when BDSP needs multi-turn battle planning.
