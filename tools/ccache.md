# ccache for local builds (macOS)

Kept from fork PR https://github.com/dayfine/Arduino-Source/pull/57 (closed; its upstream copy
https://github.com/PokemonAutomation/Arduino-Source/pull/1767 was not merged). One shared cache for
every worktree: a second worktree at the same revision built in 41 s (1,601 of 1,607 compiles from
cache) against 13–14 min uncached (Apple Clang 21, Qt 6.8.3, ccache 4.14).

`brew install ccache` (4.8+), then add these to the usual configure line (no preset needed;
SRC is the worktree's parent of SerialPrograms, i.e. the worktree root):

```sh
SRC=$(cd "$(git rev-parse --show-toplevel)" && pwd -P)
L="ccache;cache_dir=$HOME/Library/Caches/pokemonautomation-ccache;base_dir=$SRC"
cmake -S "$SRC/SerialPrograms" -B "$SRC/SerialPrograms/build" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER_LAUNCHER="$L" -DCMAKE_CXX_COMPILER_LAUNCHER="$L" \
  -DCMAKE_C_FLAGS="-fdebug-prefix-map=$SRC=. -fdebug-compilation-dir=." \
  -DCMAKE_CXX_FLAGS="-fdebug-prefix-map=$SRC=. -fdebug-compilation-dir=." \
  ...usual Qt / AGL / OpenCV options...
```

- Use one spelling of the tree path. `pwd -P` resolves links; a tree under `/tmp` (a link to
  `/private/tmp`) configured with one spelling and compiled with the other misses every file.
  Keep worktrees in `~/Projects/wt/<name>`.
- `base_dir` makes paths relative, so worktrees share hits; the debug-prefix flags keep the
  checkout path out of objects (start the debugger from the build dir).
- Hits need the same compiler, SDK, Qt and flags. A widely included header still rebuilds its users.
- The cache's size limit lives in the cache directory's own `ccache.conf`
  (`max_size = 15G`, raised from 5 GB on 2026-11-02 when the cache was full at 48% hits), not in the
  launcher; change it there with `ccache --dir ~/Library/Caches/pokemonautomation-ccache -M <size>`.
- Stats: `ccache --dir ~/Library/Caches/pokemonautomation-ccache --show-stats`.
- The full preset (macos-ccache in CMakePresets.json) is on branch `build/local-ccache`.
