# CMake formatting targets

After this integration is present in the configured source checkout:

```sh
cmake --build /path/to/build --target format-check
cmake --build /path/to/build --target format
```

`format-check` previews eligible changes and fails if any remain. `format`
applies them. Neither target builds or links the app. They use the
[edited-line formatter](INCREMENTAL.md), not clang-format: tracked staged and
unstaged changes against HEAD, with all original leading columns/newlines kept.
They skip block reflows, preprocessor definitions and declaration lines. Review
the output and stage changes yourself; the index is never changed automatically.

The target belongs to the build directory's **configured source checkout**.
Running a lab build's target formats that lab checkout, not a different feature
worktree in the terminal's current directory. Use a build configured from your
feature worktree, or invoke the Python command with `--repo` to choose another.
An old build does not gain targets until its source picks up this integration
and CMake reconfigures.

Python 3.8+, Uncrustify 0.83.0 and Clang are needed when formatting eligible
changes. Python discovery happens only when a format target runs; it does not
add a Python/formatter requirement to normal app configuration. No dependency
is downloaded automatically and no clang-format installation is required.

Optional settings:

```sh
cmake -S SerialPrograms -B /path/to/build \
  -DPA_FORMAT_BASE=origin/main \
  -DPA_FORMAT_PYTHON=/path/to/python3
```

`PA_FORMAT_BASE` defaults to HEAD; another ref selects its merge base with HEAD
for whole-branch formatting. Use the actual parent for a stacked branch. Leave
`PA_FORMAT_PYTHON` empty to discover Python at target time. `UNCRUSTIFY` and
`CLANG` environment variables select non-PATH executables as with the CLI.

To configure a tools-only build without Qt/OpenCV or application dependencies:

```sh
cmake -S SerialPrograms -B /path/to/tools-build \
  -DPA_CORE_ONLY=ON -DCOMMAND_LINE_EXE=OFF
cmake --build /path/to/tools-build --target format-check
```

This still performs normal CMake compiler/thread checks but compiles no app
when the explicit formatting target is requested. There is no `cmake --format`
flag; formatting is a project-defined build target. CTest registration for the
native application tests is a separate integration.

Tests live in `test_cmake.py` beside the formatter tests and include the actual
CMake module. They check source/build paths with spaces, a committed parent ref,
index preservation, useful missing-Python diagnostics, and that explicit format
targets do not run an unrelated ALL/application target.
