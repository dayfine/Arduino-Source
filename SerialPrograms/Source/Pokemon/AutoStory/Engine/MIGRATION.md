# Moving a game onto the story engine

The engine (`Pokemon_StoryEngine.h`) replaces each game's `StoryExecutor` and the shared
`StoryStepExecutor`. A game moves in its own PR, after this one, in this order: FRLG, BDSP, PLZA.
Each move deletes that game's executor; the last one deletes `StoryStepExecutor`.

## What each game does

1. **Backends.** Write a console backend: `StoryBackend<Pose>` over the console handle and the
   controller context, `kind()` CONSOLE. `observe()` waits for pending input
   (`wait_for_all_requests()`), takes a frame and fills `ScreenObservation::events` from the
   game's detectors (fade, text box, choice box, interact prompt, battle, can move, still).
   `stick(degrees, duration, release, pace)` holds the move stick at an angle, then lets go for
   `release` (a free-moving game's walk and turn; `hold()` is the 90-degree case). A game
   without tiles reports the way the player
   faces as the `"heading"` reading (`STORY_HEADING`, read with `story_heading()`), from
   `observe()` when cheap or from a reader of that name. `battle()` calls the game's battle code.
   A simulation backend wraps the PokemonSimulation model the same way (`kind()` SIMULATION);
   `FakeStoryBackend<Pose>` is the scripted one for unit tests, for any pose type (its script's
   `kind` can say CONSOLE to run console-only code in a test). Don't subclass the backend to add
   inputs: what is missing belongs in `StoryBackend`.
2. **StoryGame.** Build the game's `StoryGame<Pose>` once:
   - `actions`: the game's own actions (GAME steps, e.g. FRLG `take_warp`) and its replacements of
     built-ins that need its map or menus: `route`, `ready`, `train`, `shop`, and the kinds with no
     built-in (`heal`, `new_game`, `options`). A replacement uses the built-in's kind and name
     (`find_story_action(builtin_story_actions(), kind, name)` gives its spec). Don't register
     `save`: the engine owns the SAVE step so it can hold it back while testing; give `save`.
     A handler gets a `StoryActionCall`: `call.args` (fallbacks filled in) and `call.step` (its
     name and why, e.g. for a log label or a per-scene setting). An action that works only
     through screens nothing but a console shows (menus, a keyboard, a battle AI) sets
     `spec.console_only`; the engine refuses it elsewhere as `"console_only"` and never reloads
     for it, so its handler can take the console from what it captured when the game was built.
   - `legs` and `face` exist only for a pose type with tile moves (`StoryPoseTraits::TILE_MOVES`);
     a game without tiles needs no replacement for them, and a step naming them fails its lint.
   - `readers` (`StoryGameReader`): `"pose"` (map, tile, facing into `Observation::pose`), `"bag"`
     (`bag.<ITEM>` readings), story flags (`flag.<name>`), and one per `skip_if` condition the
     game answers from the screen. Each says what it `supplies` (`"pose"`, `"heading"`,
     `"bag.*"`). Postconditions are checked only with what the readers read, and only readers that
     opt in with `StoryReaderUse::AFTER_CHECKPOINT` (cheap and safe on any end screen) and supply
     a needed postcondition run then; a `console_only` reader never runs off a console.
   - `save(backend, checkpoint)` (the checkpoint whose end or SAVE step it is: e.g. an autosave
     wait for some checkpoints, the menu for others), `reload`, `begin_attempt` (today's
     `StoryStepExecutor::begin_attempt`) and `interrupts` (wild battles, trainers).
3. **Data.** Every checkpoint is a checked-in JSON file with `metadata`: `phase`, and for
   verification or reproduction `verified.steps_hash` (`StoryEngine::steps_hash()`). Add a unit
   test that loads every checkpoint, lints it (`StoryActionRegistry::lint`) and fails when a
   verification or reproduction checkpoint's recorded hash differs from its steps' hash. Delete the
   C++ table rows once the JSON parity test passes.
4. **Run.** Replace the program's executor loop with `StoryEngine::run_segment(id, checkpoints,
   state, attempts)`: it runs each with `run_checkpoint()` (the last at `RUN_END`), reloads and
   retries a checkpoint only when the reload returns to its start save (the first one, or one
   whose previous checkpoint saved at its end, #329), and never for a lint, `console_only` or
   game-not-running failure. Verify mode is `StoryEngine::verify_checkpoint(checkpoint, state,
   policy)`: runs from one save, a save only at the end of the last when every run passed. The
   save mode comes from the checkpoint's phase, not from the run options.
   Scene loops over known screens use `run_screen_machine_on_observations()`
   (CommonTools/ScreenMachine): `ScreenState::matches_observation`, a `StepTrace` built with a
   log function (no video), and a returned failure instead of an exception.
5. **Tests.** Port the executor's tests to the engine on the fake or simulation backend; each
   console failure stays a red-first regression test (the 00.31 warp is the worked example in
   `Pokemon_StoryEngine_Tests.cpp`).

## Per game

- **FRLG** (GridPose): `take_warp` from `PokemonFRLG_OverworldWalk.cpp` `wait_out_warp`, written
  against the backend as in the test; the route action over its grids; the PC, Mart and nurse
  actions; position, facing and Bag readers (tile and facing at least, for #329's check).
- **BDSP** (GridPose): the same, with D-pad legs; its warp and settle rules become actions or
  readers.
- **PLZA** (ZonePose, in the engine with its instantiations and `observed_state()`): a declared
  pose (`"start"`, a step's `"declared_after"`) names the zone as its `"map"` and nothing else
  (`StoryPoseTraits<ZonePose>::from_declared()`; `"tiles"` may be left out). Turns are
  `stick()` after the camera recenter; the minimap arrow is the `"heading"` reading.

A new pose type: a struct with `pose_matches` / `pose_text`, a `StoryPoseTraits` specialization
(`TILE_MOVES`, `from_declared()`), and an explicit instantiation line in each engine `.cpp`.

## Moves in shared code when the last executor goes

- `build_story_checkpoint()`, `story_event()`, `pose_of()` and `unsupported_step()` move from
  `Pokemon_StoryStepExecutor.*` into the engine; `RunnerExecutor` in `Pokemon_StoryEngine.cpp`
  (the adapter over the old interface) goes.
- `lint_story_checkpoint()` is replaced by `StoryActionRegistry::lint()`.
- `run_segment()` / `verify_checkpoint()` in Pokemon_StorySegments (over `Segment` tables) go once no
  game uses them: `StoryEngine::run_segment()` / `verify_checkpoint()` replace them.
