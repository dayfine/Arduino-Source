# Local Mac development

This checkout was configured on Apple Silicon (macOS 26, Apple Clang 21) on
2026-10-20. Start with a baseline build before changing BBQ behavior.

## Dependencies

- CMake and Ninja, installed with Homebrew.
- OpenCV **4** (`brew install opencv@4`); Homebrew's unversioned `opencv` is
  currently version 5, which this project's pkg-config lookup does not accept.
- Tesseract and Leptonica from Homebrew.
- Qt **6.8.3**, installed in `.cache/qt/6.8.3/macos` with aqtinstall.
  Required additional modules include qtmultimedia and qtserialport.
- Runtime data in the ignored `Packages/` checkout of
  `https://github.com/PokemonAutomation/Packages` (master).
- ONNX Runtime 1.23.2 is extracted by CMake from the bundled arm64 archive.

The local installer environment `.cache/build-tools` contains aqtinstall and
PyYAML. PyYAML also allows validation of the repository's log-reading skill.
Dependencies and generated files stay in ignored build/cache/package directories.

## Configure and build

From the repository root:

```sh
env PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig \
  cmake -S SerialPrograms -B build-local -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$PWD/.cache/qt/6.8.3/macos" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DWrapOpenGL_AGL=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/System/Library/Frameworks/AGL.framework/AGL.tbd \
  -DPA_SOCIAL_SDK=OFF -DCOMMAND_LINE_EXE=OFF
cmake --build build-local --parallel 6 > build-local/build.log 2>&1
```

`RelWithDebInfo` retains optimization and debug symbols. The optional Discord
Social SDK is disabled; DPP is absent and therefore disabled by CMake. Neither
is needed for local BBQ experiments. `COMMAND_LINE_EXE` controls the separate
controller CLI, not the main application's `--command-line-test-mode`.

When invoked through the agent's sandbox, Qt helper executables abort with
`Incompatible processor ... neon` because CPU-feature queries are restricted.
Configure, compile and launch with the required execution approval outside that
sandbox; do not alter Qt or disable hardware validation to hide the symptom.

With the macOS 26 SDK, Qt 6.8.3's OpenGL wrapper also requests AGL, whose linker
stub is absent from that SDK. The installed macOS 15.4 SDK has the arm64 stub;
the `WrapOpenGL_AGL` cache override above fixes final linking without changing
the repository or recompiling against an older SDK. Use this local override
only where that installed SDK path exists.

The complete arm64 build succeeded at checkout `ed29956a8`, with resources,
firmware and scripts deployed beside the app. The linker warns that Homebrew's
Tesseract was built for macOS 26.6.2 while this executable targets 26.0. The
current host is macOS 26.6.2, satisfying that dependency's OS requirement.

## Development profile

Launch the built application with:

```sh
open -n "$PWD/build-local/SerialPrograms.app" --args --profile bbq-dev
```

On this version, macOS stores the development profile below:

```text
~/Library/Application Support/SerialPrograms/Profiles/bbq-dev/
```

The profile contains separate settings, logs, screenshots, error dumps and
downloaded OCR data. A clean profile requires capture/controller/game settings;
alternatively copy the existing settings and downloaded resources into the new
profile before launch, keeping these private local files outside Git. Do not
point two running instances at the same controller or capture device. Close the
release instance before a gameplay experiment.

The `bbq-dev` profile has been prepared from the existing settings and downloaded
OCR resources. The app was deliberately not launched: the user chose to keep
the installed instance running while continuing analysis. Runtime startup and
capture/controller tests are still pending.

Launching the UI is a smoke test, not validation of a game route. Verify video,
audio, console type, language and controller connection before running a quest.

## Validation and experiments

The main executable accepts `--command-line-test-mode`; many tests require the
external `PokemonAutomation/CommandLineTests` fixtures, located as
`UnitTestResources/` or `CommandLineTests/` near the app. Do not report a missing
fixture run as a clean test pass. The GUI's Unit Test Runner supports selecting
tests; use targeted tests as changes become concrete.

For BBQ, first establish a baseline with the same game setup. Add structured
quest/attempt identifiers, landing and battle-entry timestamps, screenshots on
entry failure, and outcome/recovery messages before changing routes. Count
reroll BP and elapsed wall time when comparing throughput. Keep interrupted
attempts separate from completed failures. See the
[log-reading skill](../skills/serialprograms-logs/SKILL.md).
