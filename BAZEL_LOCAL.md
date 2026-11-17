# Local Bazel Setup (`Arduino-Source` Fork-Only)

This is a **local, fork-only** Bazel setup for fast incremental C++23 builds and unit tests on GUI-free subsystems (e.g., FRLG AutoStory planning/simulation). Full GUI builds, Qt6/OpenCV targets, and CI remain on `CMakeLists.txt` (`CMake + Ninja + ccache`).

## Files

| File | Purpose |
| :--- | :--- |
| `WORKSPACE` | Marks the Bazel 6 workspace root (`--noenable_bzlmod`). |
| `.bazelrc` | Sets `--cxxopt=-std=c++23` and `--test_output=errors`. |
| `BUILD.bazel` | Defines `common_cpp`, `frlg_headers`, JSON `genrule`s, and `cc_test` targets. |
| `bazel_embed_text.py` | Python equivalent of `SerialPrograms/cmake/EmbedTextFiles.cmake` for embedding `*.json` data files. |
| `bazel_frlg_test_main.cpp` | Minimal `UnitTestDatabase` runner + lightweight stubs (`status_name`, `cure_of`) so tests don't pull in controller/OCR dependencies. |

## Running Tests

```sh
bazel test --noenable_bzlmod --test_output=all //:frlg_planned_battle_fake_test
```

## Adding a New Test Target

1. **Identify the `add_tests_*` entry point** in the `*_Tests.cpp` file (e.g. `add_tests_AutoStoryPlannedBattleFake(UnitTestDatabase&)`).
2. **Add a `cc_test` in `BUILD.bazel`** with:
   - `linkstatic = True`
   - `linkopts = ["-Wl,-undefined,error"]` (prevents macOS `ld` from silently deferring missing symbols to `0x0` at runtime)
   - The `*_Tests.cpp` file and its direct `.cpp` dependencies
   - `deps = [":common_cpp", ":frlg_headers"]`
3. **Wire any `EmbedTextFiles.cmake` data** via a `genrule` calling `bazel_embed_text.py`:
   ```python
   cmd = "python3 $(location bazel_embed_text.py) $@ <HeaderPath> <Namespace> <FunctionName> $(SRCS)"
   ```
4. **If `nm -u bazel-bin/<target> | grep PokemonAutomation` shows missing symbols** (or the binary segfaults at `0x0`), either add the defining `.cpp` to `srcs` or stub the non-tested helper in the test `main.cpp`.

## Upstream PR Hygiene

- **Never merge `bazel/*` branches into upstream-bound branches.** Keep Bazel commits on fork-only branches (e.g. `bazel/*`).
- **To use Bazel untracked in any worktree**, list these files in `~/Projects/Arduino-Source/.git/info/exclude`:
  ```text
  WORKSPACE
  .bazelrc
  BUILD.bazel
  **/BUILD.bazel
  bazel-*
  bazel_embed_text.py
  bazel_*_test_main.cpp
  BAZEL_LOCAL.md
  ```

## Scaling Up (When Adding More Subsystems)

Once you outgrow a single root `BUILD.bazel`, split into only the packages you actively test:
- `Common/Cpp/BUILD.bazel` (`common_cpp`)
- `SerialPrograms/Source/Pokemon/BUILD.bazel` (`pokemon_types`)
- `SerialPrograms/Source/<Game>/.../BUILD.bazel` (colocated `cc_library` + `cc_test` targets)
