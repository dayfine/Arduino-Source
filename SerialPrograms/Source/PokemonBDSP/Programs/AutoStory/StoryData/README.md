# BDSP authored checkpoint pilot

`bdsp-mine-01.03.plan.json` replaces `oreburgh_mine_to_roark()`'s C++ event list in the real story table. It preserves #226 revision `0738cd55b562d1dd685cdea289663a015a1cbe59`: 15 events, with Barry/healing setup already moved into `01.02b`. The same JSON is consumed by PokemonSimulation's `data/story/bdsp-mine-pilot`.

The common `pokesim-authored-checkpoint/1` envelope contains `checkpoint.id`, `title`, `start`, `events` in that order. BDSP's `execution` extension preserves `on_failure` and `saves_in`; it does not use FRLG's save-menu field or import its supplies. The loader rejects unknown fields/policies and requires a save-card location exactly when saving/retrying. Schemas live in PokemonSimulation.

Two gates:

1. Native registered tests exercise the actual JSON-backed table through the existing engine/fake-game simulation, including bounded retry exhaustion before healing. Data tests check all 15 event IDs, policy/card fields and common-record round trip, and reject unsupported inputs.
2. Linux CI checks exact byte equality with a pinned PokemonSimulation revision and runs its nine modeled scenarios, including selected full ordered-action assertions.

This changes authoring, not game executor behavior. The native fake still uses declared scene/door fallbacks; a passing integration test does not prove detector/controller correctness. Python validates pinned local terrain and bounded wall movement, but requires scripted door/dialogue/scene/heal evidence and explicit wandering-NPC clearance windows. Missing transition links, native robust planner cost parity, analog/queue timing and live recovery remain separate gaps. Python's diagnostic policy is stop/no-save; the native program retains its real start-save/reload policy.

The source #226 comments record the mine warp-column evidence and lab failures behind the route. Its map resource lock remains unchanged at PokemonSimulation `4de7742a764d27293b4525d6ad2a6fc8a95f3e3a`. No map correction, console run, profile or save is part of this change.
