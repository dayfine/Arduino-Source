# BDSP authored checkpoint pilot

`bdsp-mine-01.03.plan.json` is the authoritative definition of the mine/Roark checkpoint. `oreburgh_mine_to_roark()` loads it into the existing story table. Edit this file here when changing the route; PokemonSimulation consumes an explicitly pinned Arduino revision for independent analysis. Its checked-in pilot plan is a frozen regression snapshot, not a second authoring source.

The shared `pokesim-authored-checkpoint/1` envelope contains `checkpoint.id`, `title`, `start`, `events` in that order. BDSP validates `execution.on_failure` and `execution.saves_in`. The common envelope and resource lookup live in `Pokemon/AutoStory/Pokemon_AuthoredCheckpointJson.*`. Bookkeeping defaults and game executors are unchanged.

## Validation and iteration

1. Native registered tests run the actual JSON-backed table through the existing engine/fake game, including retry exhaustion before healing. Data tests check parsing, table wiring, event IDs/order, policy/card values, and map-derived Center/Roark anchors. The table-wiring comparison is not an independent check against the removed C++ authoring list.
2. Linux CI pins a simulator revision reachable from its main branch and replays **this checkout's JSON** against nine independent scenario expectations. There is no byte-equality requirement against a second plan. A route change may still require revising a scenario expectation when its expected behavior intentionally changes; such failures are meaningful regression feedback, not a requirement to mirror JSON or repin simulator code for every edit.

A lab route edit therefore touches the Arduino plan and applicable tests/evidence. The next build re-embeds it automatically. CMake rejects malformed JSON syntax before launching the app; native tests perform full reader/policy validation. Run tests before deploying: CMake's syntax preflight alone cannot validate all semantics. The shared formatter writes authored JSON; Git normalization rules are unchanged. Whitespace and line-ending changes do not affect replay results. C++ retains the repository CRLF convention.

Coordinates remain literal data tied to the pinned map. The map-anchor test catches drift in the Center exit and Roark approach; it does not validate every tile or replace map review. See [the checkpoint evidence](bdsp-mine-01.03.md) for the warp columns, counted hall movement, wall choices and unresolved observations.

The native fake uses declared scene/door fallbacks and is not a detector/timing oracle. Python uses scripted transition/dialogue/scene/heal evidence and NPC-clearance windows. Missing warp links, native planner-cost parity, queued movement and live recovery remain gaps. Python's diagnostic policy is stop/no-save; the native program retains its start-save/reload policy. No map correction, console, profile or save is part of this authoring migration.
