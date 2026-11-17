# Local Bazel Setup (`Arduino-Source` Fork-Only)

This is a **local, fork-only** Bazel setup for fast incremental C++23 builds and unit tests. Full GUI builds, Qt6 targets, and CI remain on `CMakeLists.txt` (`CMake + Ninja + ccache`).

## Generalized Architecture Rules

To keep pure logic/simulation suites fast, hermetic, and free of OpenCV/Tesseract/Qt/hardware dependencies:

1. **Separate Pure Logic from Console/Inference Implementations**
   - Pure C++23 code (enums, data tables, simulation, route planning, `StoryBackend`-abstracted runners, `UnitTest` logic) belongs in `*Logic.cpp` / `*Sim.cpp` / `*Data.cpp` / `*Table.cpp` with **zero** `ConsoleHandle`, `ProController`, `ImageViewRGB32`, `*Detector`, `*Reader`, or Qt includes.
   - Hardware/vision implementations (`Console*`, `*Detector`, `*Reader`, `pbf_*`, `ssf_*`) live in separate `.cpp` files (e.g. `CaptureLogic.cpp` vs. `Capture.cpp`, `OverworldSim*.cpp` vs. `OverworldWalk.cpp`/`OverworldFacing.cpp`/`OverworldBike.cpp`).
2. **Keep Pure Headers Free of Heavy Includes**
   - Never `#include` `ConsoleHandle.h`, `ProController.h`, `ImageViewRGB32.h`, or `*Reader.h` in a pure logic `.h` unless a type in that header's signature actually requires it.
3. **Two-Tier Bazel Package Structure**
   - **Tier 1 (Pure C++23):** `//Common/Cpp:common_cpp`, `//SerialPrograms/Source/Pokemon:...`, `//SerialPrograms/Source/Pokemon/AutoStory:pokemon_autostory`, `//SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld:frlg_overworld`, and pure `cc_test` targets (`story_failure_classes_test`, `planned_battle_fake_test`, `pace_sim_test`).
   - **Tier 2 (Inference / OpenCV / Tesseract):** `//SerialPrograms/Source:frlg_inference_deps` + `//SerialPrograms/Source/VideoGames/PokemonFRLG/Overworld:frlg_overworld_readers` (using `@homebrew_opencv//:opencv` and `@homebrew_tesseract//:tesseract` from `WORKSPACE`) for detector/OCR targets.

## Running Tests

```sh
bazel test //...
```

## Adding a New Test Target

1. **Colocate in the subsystem's `BUILD.bazel`** next to the `*_Tests.cpp` file.
2. **Reuse `//:bazel_test_main.cpp`** with `local_defines`:
   ```python
   cc_test(
       name = "my_test",
       srcs = ["MyFeature_Tests.cpp", "//:bazel_test_main.cpp"],
       local_defines = [
           "PA_BAZEL_TEST_NS=PokemonAutomation::NintendoSwitch::PokemonFRLG",
           "PA_BAZEL_TEST_FN=add_tests_MyFeature",
       ],
       linkstatic = True,
       linkopts = ["-Wl,-undefined,error"],
       deps = [":my_pure_lib"],
   )
   ```
3. **Embed JSON data (`EmbedTextFiles.cmake`)** via a `genrule` calling `$(location //:bazel_embed_text.py)`.
4. **If `dyld` or `nm -u bazel-bin/.../<target> | grep PokemonAutomation` reports missing symbols**, either move the pure helper into its own `.cpp` (preferred, upstream-friendly) or wire the missing library into the Tier 2 target.

## Upstream PR Hygiene

- **Never merge `bazel/*` branches into upstream-bound branches.** Keep Bazel files on fork-only branches (`bazel/*`) or untracked via `~/Projects/Arduino-Source/.git/info/exclude`.
- **Clean pure-vs-hardware `.cpp` splits** (e.g. `Supplies.cpp`, `CaptureLogic.cpp`) belong on the earliest open PR in the stack (e.g. `#613`) so `pa-sync` propagates them forward.
