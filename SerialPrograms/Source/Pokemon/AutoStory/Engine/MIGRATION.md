# Moving a game onto the story engine

The engine (`Pokemon_StoryEngine.h`) replaces each game's `StoryExecutor` and the shared
`StoryStepExecutor`. A game moves in its own PR, after this one, in this order: FRLG, BDSP, PLZA.
Each move deletes that game's executor; the last one deletes `StoryStepExecutor`.

## What each game does

1. **Backends.** Write a console backend: `StoryBackend<Pose>` over the console handle and the
   controller context. `observe()` waits for pending input (`wait_for_all_requests()`), takes a
   frame and fills `ScreenObservation::events` from the game's detectors (fade, text box, battle,
   can move, still). `battle()` calls the game's battle code. A simulation backend wraps the
   PokemonSimulation model the same way; `FakeStoryBackend` is the scripted one for unit tests.
2. **StoryGame.** Build the game's `StoryGame<Pose>` once:
   - `actions`: the game's own actions (GAME steps, e.g. FRLG `take_warp`) and its replacements of
     built-ins that need its map or menus: `route`, `ready`, `train`, `shop`, and the kinds with no
     built-in (`heal`, `new_game`, `options`). A replacement uses the built-in's kind and name
     (`find_story_action(builtin_story_actions(), kind, name)` gives its spec). Don't register
     `save`: the engine owns the SAVE step so it can hold it back while testing; give `save`.
   - `readers`: `"pose"` (map, tile, facing into `Observation::pose`), `"bag"` (`bag.<ITEM>`
     readings), story flags (`flag.<name>`), and one per `skip_if` condition the game answers from
     the screen. Postconditions are checked only with what the readers read.
   - `save`, `reload`, `begin_attempt` (today's `StoryStepExecutor::begin_attempt`) and
     `interrupts` (wild battles, trainers).
3. **Data.** Every checkpoint is a checked-in JSON file with `metadata`: `phase`, and for
   verification or reproduction `verified.steps_hash` (`StoryEngine::steps_hash()`). Add a unit
   test that loads every checkpoint, lints it (`StoryActionRegistry::lint`) and fails when a
   verification or reproduction checkpoint's recorded hash differs from its steps' hash. Delete the
   C++ table rows once the JSON parity test passes.
4. **Run.** Replace the program's executor loop with `StoryEngine::run_checkpoint()` per
   checkpoint; pass `SavePoint::CHECKPOINT_END` for all but the last checkpoint of a run. The save
   mode now comes from the checkpoint's phase, not from the run options.
5. **Tests.** Port the executor's tests to the engine on the fake or simulation backend; each
   console failure stays a red-first regression test (the 00.31 warp is the worked example in
   `Pokemon_StoryEngine_Tests.cpp`).

## Per game

- **FRLG** (GridPose): `take_warp` from `PokemonFRLG_OverworldWalk.cpp` `wait_out_warp`, written
  against the backend as in the test; the route action over its grids; the PC, Mart and nurse
  actions; position, facing and Bag readers (tile and facing at least, for #329's check).
- **BDSP** (GridPose): the same, with D-pad legs; its warp and settle rules become actions or
  readers.
- **PLZA**: add `ZonePose{std::string zone;}` with `pose_matches` / `pose_text` next to
  `Pokemon_StoryPoseConcept.h`, and an explicit instantiation line for it in each engine `.cpp`.
  `StoryCheckpoint` positions are `GridPose` today: PLZA steps carry their zone in action args
  until checkpoints are templated on the pose, and its postconditions need an `observed_state()`
  overload for `Observation<ZonePose>`.

## Moves in shared code when the last executor goes

- `build_story_checkpoint()`, `story_event()`, `pose_of()` and `unsupported_step()` move from
  `Pokemon_StoryStepExecutor.*` into the engine; `RunnerExecutor` in `Pokemon_StoryEngine.cpp`
  (the adapter over the old interface) goes.
- `lint_story_checkpoint()` is replaced by `StoryActionRegistry::lint()`.
- `run_segment()` / `verify_checkpoint()` (Pokemon_StorySegments) call the engine per checkpoint.
