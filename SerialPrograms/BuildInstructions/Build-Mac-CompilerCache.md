# Local compiler caching on macOS

This is the macOS companion to the
[incremental-build overview](IncrementalBuilds.md). Complete the normal
[Mac build setup](CompilingForMac.md) first; this guide adds an optional compiler
cache to an already working toolchain.

Validated on Apple Silicon (arm64), Apple Clang 21, Qt 6.8.3 and ccache 4.14.2.
Intel Mac performance has not been measured. The `macos-ccache` preset and
commands below are macOS-specific. A validated local Windows cache guide is
still missing; see the [platform status](IncrementalBuilds.md#platform-support).

## Install and configure

Install the tools once:

```sh
brew install ninja ccache
ccache --dir "$HOME/Library/Caches/pokemonautomation-ccache" --max-size 10G
```

The `macos-ccache` preset uses Ninja, RelWithDebInfo and Apple Clang. It leaves
other presets and CI configuration unchanged. It is intended for the standard
macOS toolchain; its debug-path flags are not MSVC or GCC options.

Configure from the repository root, adding the same Qt/SDK/dependency options
used for a normal build on your machine:

```sh
cmake -S SerialPrograms --preset macos-ccache
cmake --build build/macos-ccache --parallel 4
```

For example, when Qt is installed outside the default search path, append
`-DCMAKE_PREFIX_PATH=/absolute/path/to/Qt/6.8.3/macos` to the configure command.
The preset does not install Qt, resolve SDK compatibility or replace the
platform build guide. Do not migrate a live lab build while another process
is building or deploying it.

The preset sets both C and C++ compiler launchers to ccache. Per-invocation
settings select:

- `$HOME/Library/Caches/pokemonautomation-ccache`: one local cache shared by
  worktrees belonging to this user.
- `max_size=10G`: automatic eviction keeps the compiler cache bounded. This is
  a ccache size target, not a quota on build folders or deployed applications.
- `base_dir`: the current worktree root, so paths inside the checkout can be
  rewritten relative to the compilation directory.

These settings are part of the generated compiler command. Ordinary
`cmake --build` calls work without remembering shell environment exports.
For explicit inspection or maintenance, address this cache rather than
ccache's default cache:

```sh
ccache --dir "$HOME/Library/Caches/pokemonautomation-ccache" --show-stats
ccache --dir "$HOME/Library/Caches/pokemonautomation-ccache" --show-compression
```

Use the same relative build directory layout in each worktree. The preset
normalizes debug paths with `-fdebug-prefix-map=<worktree>=.` and
`-fdebug-compilation-dir=.`. This lets compatible builds share cached objects
without turning off directory hashing or header checks. Debug information
uses relative paths: launch the debugger from the matching build directory,
or configure its source-path mapping for the checkout being debugged.

The preset supplies `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS`. If you already need
custom flags, combine them with these flags in a local override; do not replace
required compiler options accidentally. Keep the compiler, SDK, dependency
versions and relevant flags consistent across worktrees to get cache hits.
Changing them should trigger misses. Qt-generated sources containing absolute
paths can also miss; inspect cache diagnostics before relaxing correctness checks.

A new cache must first be populated. The first compilation can be slower due
to hashing/compression overhead; measure subsequent builds as well. Compiler
caching does not share a CMake build directory between worktrees, eliminate
all generated-code work, or make a shared-header change free.

## Enable caching in an existing Mac build

For straightforward caching within one build directory, a preset is optional.
Add launchers to the existing configuration, retaining its other settings:

```sh
cmake -S SerialPrograms -B path/to/existing-build \
  "-DCMAKE_C_COMPILER_LAUNCHER=ccache;cache_dir=$HOME/Library/Caches/pokemonautomation-ccache;max_size=10G" \
  "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache;cache_dir=$HOME/Library/Caches/pokemonautomation-ccache;max_size=10G"
cmake --build path/to/existing-build --parallel 4
```

This enables caching but does not normalize paths for cross-worktree reuse.
Changing launchers or flags can cause Ninja to rebuild objects once. Do not
run `clean` as part of the normal edit/build cycle.

To remove caching from that configured build, explicitly clear both launchers:

```sh
cmake -S SerialPrograms -B path/to/existing-build \
  -DCMAKE_C_COMPILER_LAUNCHER= \
  -DCMAKE_CXX_COMPILER_LAUNCHER=
```

For a build created with `macos-ccache`, either keep using its dedicated
configuration or configure a separate ordinary preset. If undoing its settings
in place, also restore the original C/C++ flags; simply selecting another
preset need not clear variables already stored in CMakeCache.txt.

## Verify reuse and invalidation

Record wall time, build exit status and ccache counter differences for each
case. A shared cache may receive other jobs' traffic; run measurements without
concurrent compiles using that cache. Keep measured results with the build/run
record rather than treating one machine's timing as a portable guarantee.

1. Build once to populate the cache.
2. Build again without changes. Ninja should schedule no compilations;
   zero new cache hits is correct here. Custom deployment targets may still run.
3. Make a small source edit and rebuild. Only the affected compilation and
   dependent build steps should run. Restore the edit and check cache reuse.
4. Edit a shared header and verify its dependents are rebuilt. Restoring the
   header should allow reuse of earlier compilations.
5. Configure the same revision in another worktree with the same preset,
   dependency locations and toolchain. Build there and check cache hits. This
   measures reuse across worktrees, not just an unchanged Ninja build.
6. Run the applicable native tests on the resulting executable. Cache hit rate
   is a performance measurement, not evidence of application correctness.

For an unexpected rebuild, Ninja can explain its scheduling decisions without
executing build commands:

```sh
ninja -C build/macos-ccache -n -d explain
```

For unexpected cache misses, use ccache's debug logs on a small representative
compile. Do not disable header validation, enable broad sloppiness settings or
reuse stale objects just to increase hit rate.

A small dependency-free trial can use the same preset with
`-DPA_CORE_ONLY=ON -B build/ccache-core`, followed by
`cmake --build build/ccache-core --parallel 4`. Use a separate directory for the
full GUI build, and report core-only results as such.

## Measure binary and disk size

Compiler caching does not reduce the linked executable's code size. Keep
compiler options, source revision, architecture and deployment contents fixed
when comparing sizes; debug-path normalization can change debug metadata.
Measure the executable, app bundle, runtime resources, build intermediates
and shared cache separately:

```sh
# Executable size in bytes.
stat -f '%z' build/macos-ccache/SerialPrograms.app/Contents/MacOS/SerialPrograms

# Allocated disk space; the build directory includes the other listed outputs.
du -sh build/macos-ccache/SerialPrograms.app \
  build/macos-ccache/Resources build/macos-ccache/libSerialProgramsLib.a \
  build/macos-ccache

# Compressed cache size and the original size of cached outputs.
ccache --dir "$HOME/Library/Caches/pokemonautomation-ccache" --show-compression
```

The static `.a` libraries and object files are build intermediates, not files
to copy into every deployment. A local `.app` may rely on libraries installed
outside the bundle, so its size alone is not the size of a self-contained
redistributable package. If reducing distribution size is a separate goal,
evaluate release/debug-symbol packaging and runtime resources independently
of the compiler cache.

## Scope and references

This preset reduces repeated compilation. Worktree build directories still
contain their own objects, and deployment snapshots still contain their own
copies of the app/resources. Their retention is a separate concern. Keep
runtime resources, application signing and running lab instances under the
normal deployment workflow.

- [ccache manual: cross-directory builds and cache configuration](https://ccache.dev/manual/latest.html#_compiling_in_different_directories)
- [CMake compiler launcher property](https://cmake.org/cmake/help/latest/prop_tgt/LANG_COMPILER_LAUNCHER.html)
- [Ninja manual](https://ninja-build.org/manual.html)
