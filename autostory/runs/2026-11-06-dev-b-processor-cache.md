# Console B development build: stale processor detection

User reported `unsupported argument 'nehalem' to option '-march='` while starting
the development instance. No app or console was launched during this repair.

Reproduced with `/usr/bin/clang++ -arch arm64 -march=nehalem -x c++ -fsyntax-only -`
on a minimal stdin translation unit. Console B's generated CoreLib and
SerialProgramsLib flags contained this same incompatible combination.

The cached CMake 4.4.4 `CMakeSystem.cmake` recorded host/system `x86_64`, while
`CMAKE_OSX_ARCHITECTURES` was `arm64`. The current CMake executable is native
ARM64; stale system detection, rather than an active Intel CMake executable or
environment CXXFLAGS, caused the architecture branch mismatch.

With the existing console B lock protocol and shared build mutex held, preserved
the local config/cache under explicit `/tmp` paths, added
`-DCMAKE_APPLE_SILICON_PROCESSOR=arm64` to both ARM64 slots in
`~/.config/pokemonautomation/dev-labs.json`, and configured B using `cmake --fresh`
with its complete existing arguments and `PA_LAB_PROFILE=lab-B`.
Configure succeeded; system/host now report `arm64`. No source changes or
generated-file edits. Slot A has no configured CMakeSystem file yet and did not
need a build-cache refresh; its explicit processor setting applies on creation.

Verified both generated library flag sets target ARM64, omit Intel architecture
options/x64 dispatch, and the GUI library enables `PA_AutoDispatch_arm64_20_M1`.
Compiled the production CoreLib Exceptions object using its generated make rule
under the same locks: exit 0, with `file` confirming a Mach-O ARM64 object.
A complete app build remains the human launcher's next step. This repair does not establish full native test or GUI/console readiness.

Backups/log: `/tmp/dev-labs-before-processor-fix.json`,
`/tmp/slot-B-before-processor-fix-CMakeCache.txt`,
`/tmp/slot-B-processor-fix-configure.log`.
