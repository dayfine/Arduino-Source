# Navigation Unification Status

Tracking the stacked PRs unifying AutoStory shared core, grid navigation, robust leg planning, and wandering-NPC recovery across `Pokemon::AutoStory`, `Pokemon::Navigation`, `PokemonFRLG`, and `PokemonBDSP`.

## 1. Upstream Base Stack (`PokemonAutomation` / `dayfine/Arduino-Source`)

| Order | PR | Branch | Base | Status | Summary |
| --- | --- | --- | --- | --- | --- |
| 1 | [#572](https://github.com/dayfine/Arduino-Source/pull/572) | `autostory/story-range-options-sv` | `main` | Draft (`CLEAN`, green; upstream #1921) | AutoStory: shared start/end checkpoint range options, from SV autostory |
| 2 | [#667](https://github.com/dayfine/Arduino-Source/pull/667) | `autostory/screen-machine` | `autostory/story-range-options-sv` | Draft (`CLEAN`, green) | CommonTools: screen events and a screen state machine |
| 3 | [#648](https://github.com/dayfine/Arduino-Source/pull/648) | `autostory/shared-core` | `autostory/screen-machine` | Draft (`CLEAN`, green) | AutoStory: shared grid-free core (S1–S17) |
| 4 | [#653](https://github.com/dayfine/Arduino-Source/pull/653) | `autostory/grid-core` | `autostory/shared-core` | Draft (`cca36b933`, `CLEAN`, green) | AutoStory grid-core: shared tile primitives (`Pokemon_Direction`, `Pokemon_Grid`, `Pokemon_GateState`), `Pokemon_GridPose`, `Pokemon_StoryPostconditions`, S10 JSON parsers, and `add_grid_story_actions<Backend>()` |
| 5 | [#741](https://github.com/dayfine/Arduino-Source/pull/741) | `autostory/navigation-seam` | `autostory/grid-core` | Draft (`03d791e28`, `CLEAN`, green) | Pokemon Navigation: shared `RobustModelFromLegModel`, `RobustLegCosts::forbid_along_wander`, and `Pokemon_WanderRecovery` seam (36/36 unit tests passing) |

## 2. FRLG Chain (`PokemonFRLG`)

| PR | Branch | Base | Status | Summary |
| --- | --- | --- | --- | --- |
| [#652](https://github.com/dayfine/Arduino-Source/pull/652) | `frlg/lean` | `main` | Draft | FRLG AutoStory: lean branch (base for FRLG stack) |
| [#747](https://github.com/dayfine/Arduino-Source/pull/747) | `frlg/navigation-seam-rebase` | `frlg/lean` | Draft (`05de1bd1f`) | FRLG: adopt shared navigation seam (#741), `Pokemon_WanderRules`, `SimWandererBlocks`, and wanderer routing/recovery cleanly on `frlg/lean` (supersedes #700) |
| [#700](https://github.com/dayfine/Arduino-Source/pull/700) | `frlg/wanderer-routing` | `frlg/lean` | Closed (superseded by #747) | PokemonFRLG: earlier pre-seam wanderer routing PR |
| [#704](https://github.com/dayfine/Arduino-Source/pull/704) | `frlg/pc-box-grid-switch` | `frlg/lean` | Merged | PokemonFRLG AutoStory: switch box via `BOX_GRID` header on multi-box withdraw |
| [#702](https://github.com/dayfine/Arduino-Source/pull/702) | `frlg/cerulean-nugget-bridge-10x` | `frlg/lean` | Open | FRLG AutoStory: Cerulean / Nugget Bridge 10x and `05.01`–`05.10` plan updates |
| [#712](https://github.com/dayfine/Arduino-Source/pull/712) | `frlg/stage6-7-non-mgba-fixes` | `frlg/lean` | Open | FRLG: Stage 6-7 non-mGBA warp, supply, and story plan fixes |
| [#743](https://github.com/dayfine/Arduino-Source/pull/743) | `frlg/04-05-poison-survival` | `frlg/lean` | Merged | PokemonFRLG AutoStory: add `04.05` `poison_survival` postcondition |
| [#744](https://github.com/dayfine/Arduino-Source/pull/744) | `frlg/nugget-farm-option` | `frlg/lean` | Draft | FRLG AutoStory: add `NUGGET_FARM_COUNT` option and `nugget_farm_skipped` condition |
| [#745](https://github.com/dayfine/Arduino-Source/pull/745) | `frlg/cerulean-ledge-safe-open-leg` | `frlg/lean` | Open | FRLG: use Cerulean `(25, 18)` ledge in `05.08`/`05.09` and trust safe open legs |

## 3. BDSP Chain (`PokemonBDSP`)

| PR | Branch | Base | Status | Summary |
| --- | --- | --- | --- | --- |
| [#455](https://github.com/dayfine/Arduino-Source/pull/455) | `bdsp/rebuild-detectors` | `bdsp/rebuild-base` | Draft (frozen per E8) | BDSP AutoStory rebuild 1/6: detectors |
| [#456](https://github.com/dayfine/Arduino-Source/pull/456) | `bdsp/rebuild-walker` | `bdsp/rebuild-detectors` | Draft (frozen per E8) | BDSP AutoStory rebuild 2/6: maps, planner and walker |
| [#746](https://github.com/dayfine/Arduino-Source/pull/746) | `bdsp/navigation-seam-rebase` | `bdsp/rebuild-walker` | Draft (`4277539ee`; verified locally 56/56 Nav+BDSP tests pass, `code-health` green; Ubuntu draft CI fails only on frozen #456's ONNX URL) | BDSP: adopt shared navigation seam (#741) (`forbid_along_wander` & `WanderRecovery`) cleanly on `bdsp/rebuild-walker` |
| [#459](https://github.com/dayfine/Arduino-Source/pull/459) | `bdsp/rebuild-shell` | `bdsp/rebuild-walker` | Draft (`7e3475edb`, frozen per E8; conflicts with #456 on `PokemonBDSP_Tests.cpp`) | BDSP AutoStory rebuild 3/6: program shell |
| [#460](https://github.com/dayfine/Arduino-Source/pull/460) | `bdsp/rebuild-sim` | `bdsp/rebuild-shell` | Draft (frozen per E8) | BDSP AutoStory rebuild 4/6: story simulation |
| [#461](https://github.com/dayfine/Arduino-Source/pull/461) | `bdsp/rebuild-seg00` | `bdsp/rebuild-sim` | Draft (frozen per E8) | BDSP AutoStory rebuild 5/6: segment-00 tests |
| [#462](https://github.com/dayfine/Arduino-Source/pull/462) | `bdsp/rebuild-seg01` | `bdsp/rebuild-seg00` | Draft (frozen per E8) | BDSP AutoStory rebuild 6/6: segment 01 plans and tests |
| [#432](https://github.com/dayfine/Arduino-Source/pull/432) | `bdsp/wanderer-zone-stand` | `bdsp/0005c-plaza` | Closed (superseded by #456–#462 & #746) | BDSP AutoStory: wait for wandering clowns inside their zone; `00.05b` checks its coupons, `00.05c` re-talks none |

**Frozen BDSP Rebuild Chain (E8):** `#455`, `#456`, `#459`, `#460`, `#461`, and `#462` remain frozen per `SHARED_CORE.md` E8 until the post-`#741` BDSP re-cut (do not rebase/edit them in place; `#746` is the verified BDSP seam reference on `#456`, and its Ubuntu draft CI failure is only an inherited ONNX Runtime download error in frozen `#456`'s older `CMakeLists.txt`). During the post-`#741` BDSP re-cut, fold in `#746` plus `lab/dev-bdsp-527`'s 17 verified Console B / door-settle commits (`ec0d1599d`, full `00.03` -> `01.11` Floaroma Town run completed), then close the frozen rebuild slices.
