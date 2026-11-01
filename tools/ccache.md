# ccache for local builds (macOS)

Kept from fork PR https://github.com/dayfine/Arduino-Source/pull/57 (closed; its upstream copy
https://github.com/PokemonAutomation/Arduino-Source/pull/1767 was not merged). One shared cache for
every worktree: a second worktree at the same revision built in 41 s (1,601 of 1,607 compiles from
cache) against 13–14 min uncached (Apple Clang 21, Qt 6.8.3, ccache 4.14).

`brew install ccache` (4.8+), then add these to the usual configure line (no preset needed;
SRC is the worktree's parent of SerialPrograms, i.e. the worktree root):

```sh
SRC=$(git rev-parse --show-toplevel)
L="ccache;cache_dir=$HOME/Library/Caches/pokemonautomation-ccache;base_dir=$SRC;max_size=5G"
cmake -S SerialPrograms -B SerialPrograms/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER_LAUNCHER="$L" -DCMAKE_CXX_COMPILER_LAUNCHER="$L" \
  -DCMAKE_C_FLAGS="-fdebug-prefix-map=$SRC=. -fdebug-compilation-dir=." \
  -DCMAKE_CXX_FLAGS="-fdebug-prefix-map=$SRC=. -fdebug-compilation-dir=." \
  ...usual Qt / AGL / OpenCV options...
```

- `base_dir` makes paths relative, so worktrees share hits; the debug-prefix flags keep the
  checkout path out of objects (start the debugger from the build dir).
- Hits need the same compiler, SDK, Qt and flags. A widely included header still rebuilds its users.
- Stats: `ccache --dir ~/Library/Caches/pokemonautomation-ccache --show-stats`.
- The full preset (macos-ccache in CMakePresets.json) is on branch `build/local-ccache`.
