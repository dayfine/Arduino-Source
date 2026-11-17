# Local Bazel Setup (`Arduino-Source` Fork-Only)

This is a **local, fork-only** Bazel setup for fast incremental C++23 builds and unit tests on GUI-free subsystems (e.g., FRLG AutoStory planning/simulation). Full GUI builds, Qt6/OpenCV targets, and CI remain on `CMakeLists.txt` (`CMake + Ninja + ccache`).

## Package Layout (4 `BUILD.bazel` files)

| File | Purpose |
| :--- | :--- |
| `WORKSPACE` & `.bazelrc` | Bazel 6 workspace root (`--noenable_bzlmod`), `-std=c++23`, `--test_output=errors`. |
| `BUILD.bazel` | Shared header targets (`third_party_core_headers`, `common_headers`, `serial_programs_headers`) and exports `bazel_embed_text.py` + `bazel_test_main.cpp`. |
| `Common/Cpp/BUILD.bazel` | `//Common/Cpp:common_cpp` (non-Qt `Common/Cpp` + `3rdParty-Core`). |
| `SerialPrograms/Source/Pokemon/BUILD.bazel` | `//SerialPrograms/Source/Pokemon:pokemon_types` (`Pokemon_Types.cpp`). |
| `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/BUILD.bazel` | Colocated `genrule`s (`BattleData/*.json`, `StoryData/*.json`), `planned_battle_lib`, and `cc_test` targets. |
| `bazel_embed_text.py` | Python equivalent of `SerialPrograms/cmake/EmbedTextFiles.cmake`. |
| `bazel_test_main.cpp` | Generic `UnitTestDatabase` runner parameterized by `PA_BAZEL_TEST_NS` and `PA_BAZEL_TEST_FN`. |

## Running Tests

```sh
bazel test --noenable_bzlmod --test_output=all //SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory:planned_battle_fake_test
```

## Adding a New Test Target

1. **Create or update the subsystem's `BUILD.bazel`** (next to the `*_Tests.cpp` file).
2. **Add a `cc_test` using `//:bazel_test_main.cpp`** with `local_defines`:
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
       deps = [":my_lib"],
   )
   ```
3. **Wire any `EmbedTextFiles.cmake` JSON data** via a `genrule` calling `$(location //:bazel_embed_text.py)`.
4. **If `nm -u bazel-bin/.../<target> | grep PokemonAutomation` shows missing symbols**, either add the defining `.cpp` to the library's `srcs` or stub non-tested helpers in a local `bazel_*_stubs.cpp`.

## Upstream PR Hygiene

- **Never merge `bazel/*` branches into upstream-bound branches.** Keep Bazel commits on fork-only branches (e.g. `bazel/*`).
- **To use Bazel untracked in any worktree**, list these patterns in `~/Projects/Arduino-Source/.git/info/exclude`:
  ```text
  WORKSPACE
  .bazelrc
  BUILD.bazel
  **/BUILD.bazel
  bazel-*
  bazel_embed_text.py
  bazel_test_main.cpp
  **/bazel_*_stubs.cpp
  BAZEL_LOCAL.md
  ```
