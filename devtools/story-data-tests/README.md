# Shared story-record codec tests

Run without Qt, controllers, profiles or game saves:

```sh
cmake -S devtools/story-data-tests -B build/story-data-tests
cmake --build build/story-data-tests -j4
ctest --test-dir build/story-data-tests --output-on-failure
```

For a multi-configuration generator, add `--config Debug` to the build,
`-C Debug` to CTest and use `build/story-data-tests/Debug/StoryDataTests.exe`.
The same native cases are registered in the application's unit-test runner.

`Pokemon_StoryStepJson` reads the common `StoryCheckpoint`, `StoryStep` and
declared-pose records. It keeps action order, alternative tiles and absent
facing, and fills arg fallbacks. Unknown fields, kinds, actions and invalid
primitive types fail. The C++ side has no writer; its tests check the values
read.

The portable workflow checks the Python reader against the synthetic
conformance cases at
[PokemonSimulation revision 866613f](https://github.com/dayfine/PokemonSimulation/tree/866613fa03e5c4a5eb7e57c91b59fafb44fc7ee3/data/story).
This fixture revision is a contract-test dependency, not an approved
map-resource revision.

See the [shared checkpoint contract](https://github.com/dayfine/PokemonSimulation/blob/866613fa03e5c4a5eb7e57c91b59fafb44fc7ee3/docs/story/CHECKPOINT_CONTRACT.md)
for the field boundary and proposed common requirements, state, save/retry and
handoff envelope. Both games can use those policies; their menu/detector/action
bindings remain game specific.

These are synthetic codec tests, not exports of either game's authored story
tables or captured controller replay. Evaluated FRLG/BDSP table exporters,
policy/state envelope binding and execution parity remain subsequent work.
No gameplay behavior changes here.

The same CTest command also runs `AuthoredCheckpointTests`: shared authored-envelope parsing and embedded-resource lookup for FRLG, BDSP, and opaque future game settings. Game adapters must validate their own execution object before running anything. The shared codec does not interpret FRLG menus, BDSP save-card policy, native bookkeeping, or controller evidence.
