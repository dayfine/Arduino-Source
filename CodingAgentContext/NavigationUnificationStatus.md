# Navigation Unification Status

Tracking the stacked PRs and local integration branches unifying grid navigation, robust leg planning, and wandering-NPC recovery across `Pokemon::Navigation`, `PokemonFRLG`, and `PokemonBDSP`.

## 1. Upstream Base Stack (`PokemonAutomation` / `dayfine/Arduino-Source`)

| Order | PR | Branch | Base | Status | Summary |
| --- | --- | --- | --- | --- | --- |
| 1 | [#572](https://github.com/dayfine/Arduino-Source/pull/572) | `autostory/story-range-options-sv` | `main` | Open | AutoStory: shared start/end checkpoint range options, from SV autostory |
| 2 | [#667](https://github.com/dayfine/Arduino-Source/pull/667) | `autostory/screen-machine` | `autostory/story-range-options-sv` | Open | CommonTools: screen events and a screen state machine |
| 3 | [#648](https://github.com/dayfine/Arduino-Source/pull/648) | `autostory/shared-core` | `autostory/screen-machine` | Open | AutoStory: shared core |
| 4 | [#653](https://github.com/dayfine/Arduino-Source/pull/653) | `autostory/grid-core` | `autostory/shared-core` | Open | AutoStory grid-core: step and checkpoint data fields |
| 5 | [#741](https://github.com/dayfine/Arduino-Source/pull/741) | `autostory/navigation-seam` | `autostory/grid-core` | Open (`d6d4ca2af`) | Pokemon Navigation: shared `RobustModelFromLegModel` and `WanderRecovery` seam (`Pokemon_WanderRecovery` + `Pokemon_RobustLegPlanner`, 36/36 unit tests passing) |

## 2. FRLG Chain (`PokemonFRLG`)

| PR / Commit | Branch | Base | Status | Summary |
| --- | --- | --- | --- | --- |
| [#652](https://github.com/dayfine/Arduino-Source/pull/652) | `frlg/lean` | `main` | Open | FRLG AutoStory: lean branch (base for FRLG stack) |
| [#700](https://github.com/dayfine/Arduino-Source/pull/700) | `frlg/wanderer-routing` | `frlg/lean` | Open | PokemonFRLG: add tunable wandering-NPC routing and bump recovery (integrates `Pokemon_WanderRecovery` / `SimWandererBlock`) |
| [#704](https://github.com/dayfine/Arduino-Source/pull/704) | `frlg/pc-box-grid-switch` | `frlg/lean` | Open | PokemonFRLG AutoStory: switch box via `BOX_GRID` header on multi-box withdraw |
| [#702](https://github.com/dayfine/Arduino-Source/pull/702) | `frlg/cerulean-nugget-bridge-10x` | `frlg/lean` | Open | FRLG AutoStory: Cerulean / Nugget Bridge 10x and `05.01`–`05.10` plan updates |
| [#712](https://github.com/dayfine/Arduino-Source/pull/712) | `frlg/stage6-7-non-mgba-fixes` | `frlg/lean` | Open | FRLG: Stage 6-7 non-mGBA warp, supply, and story plan fixes |
| [#743](https://github.com/dayfine/Arduino-Source/pull/743) | `frlg/04-05-poison-survival` | `frlg/lean` | Open | PokemonFRLG AutoStory: add `04.05` `poison_survival` postcondition |
| [#744](https://github.com/dayfine/Arduino-Source/pull/744) | `frlg/nugget-farm-option` | `frlg/lean` | Draft | FRLG AutoStory: add `NUGGET_FARM_COUNT` option and `nugget_farm_skipped` condition |
| [#745](https://github.com/dayfine/Arduino-Source/pull/745) | `frlg/cerulean-ledge-safe-open-leg` | `frlg/lean` | Open | FRLG: use Cerulean `(25, 18)` ledge in `05.08`/`05.09` and trust safe open legs |
| [#747](https://github.com/dayfine/Arduino-Source/pull/747) | `frlg/navigation-seam-rebase` | `autostory/navigation-seam` | Draft (`5991f13d3`) | FRLG: seam rebase onto `autostory/navigation-seam` (`d6d4ca2af`) |

## 3. BDSP Chain (`PokemonBDSP`)

| PR | Branch | Base | Status | Summary |
| --- | --- | --- | --- | --- |
| [#455](https://github.com/dayfine/Arduino-Source/pull/455) | `bdsp/rebuild-detectors` | `bdsp/rebuild-base` | Open | BDSP AutoStory rebuild 1/6: detectors |
| [#456](https://github.com/dayfine/Arduino-Source/pull/456) | `bdsp/rebuild-walker` | `bdsp/rebuild-detectors` | Open | BDSP AutoStory rebuild 2/6: maps, planner and walker |
| [#460](https://github.com/dayfine/Arduino-Source/pull/460) | `bdsp/rebuild-sim` | `bdsp/rebuild-shell` | Open | BDSP AutoStory rebuild 4/6: story simulation |
| [#461](https://github.com/dayfine/Arduino-Source/pull/461) | `bdsp/rebuild-seg00` | `bdsp/rebuild-sim` | Open | BDSP AutoStory rebuild 5/6: segment-00 tests |
| [#462](https://github.com/dayfine/Arduino-Source/pull/462) | `bdsp/rebuild-seg01` | `bdsp/rebuild-seg00` | Open | BDSP AutoStory rebuild 6/6: segment 01 plans and tests |
| [#432](https://github.com/dayfine/Arduino-Source/pull/432) | `bdsp/wanderer-zone-stand` | `bdsp/0005c-plaza` | Open | BDSP AutoStory: wait for wandering clowns inside their zone; `00.05b` checks its coupons, `00.05c` re-talks none |
| [#746](https://github.com/dayfine/Arduino-Source/pull/746) | `bdsp/navigation-seam-rebase` | `autostory/navigation-seam` | Draft (`fcc891f99`) | BDSP: seam rebase synced with `autostory/navigation-seam` (`d6d4ca2af`) |
