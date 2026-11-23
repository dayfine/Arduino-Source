# Shared story-record codec tests

Run without Qt, controllers, profiles or game saves:

```sh
cmake -S devtools/story-data-tests -B build/story-data-tests
cmake --build build/story-data-tests -j4
ctest --test-dir build/story-data-tests --output-on-failure
```

For a multi-configuration generator, add `--config Debug` to the build and
`-C Debug` to CTest. The same native cases are registered in the application's
unit-test runner.

`StoryDataTests` builds and runs the Qt-free shared AutoStory S10 codec and
engine tests directly:

- `Pokemon_StoryCheckpointJson_Tests`: `read_story_checkpoint()`, `StoryActions`,
  `reject_unknown_keys()`, and envelope validation for
  `pokesim-authored-checkpoint/1`.
- `Pokemon_GridStoryActions_Tests`: `Pokemon_GridStoryArgs` (`read_direction()`,
  `read_grid_pose()`, `read_story_legs()`, `read_story_items()`,
  `read_tile_declared()`) and shared grid actions (`legs`, `face`).
