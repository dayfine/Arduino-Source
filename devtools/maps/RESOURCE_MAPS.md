# FRLG map resources

FRLG's existing `overworld_map_json()` API now embeds JSON from prepared
`Packages/Resources/Pokemon/Maps/frlg`, rather than an authored source-map copy.
Its native map decoder, route/story/recovery code and controller behavior are
unchanged. Resource preparation happens before configuring, with no implicit
fallback to the deleted source directory and no map download during configure.
This is a change of build input, not runtime filesystem loading.

The resource lock and CI tools pin published PokemonSimulation main
`906956e94fc912ae9adb0e44845da780f3bc7e4c`. The 53-map resource pack now uses the
compact v2 wire format, read by #114's shared decoder. V2 groups records by kind
and omits empty sections; map geometry, triggers and per-kind record order must
match the previous v1 pack. Native schema, map, route/story and recovery tests
verify this adoption; their current results are recorded in the fork PR.
BDSP's held map differences are not imported or overwritten.

Clone a full tools/data checkout outside deployed Resources, then explicitly
select the published tool version used by CI:

```sh
git clone https://github.com/dayfine/PokemonSimulation.git /path/to/pokesim
git -C /path/to/pokesim checkout 906956e94fc912ae9adb0e44845da780f3bc7e4c
PYTHONPATH=/path/to/pokesim/src python3 -m pokesim.maps.consumer_sync prepare \
  --layout resources --source /path/to/pokesim --consumer /path/to/Arduino-Source
cmake -S SerialPrograms -B /path/to/build \
  -DPOKESIM_TOOLS_SOURCE=/path/to/pokesim -DPOKESIM_SOURCE_CACHE=/path/to/pokesim
```

CI checks out Packages, prepares this same resource lock through the pinned
integration action, and passes its tools/cache paths to CMake. Missing, partial,
modified, extra or unapproved resource data fails before compilation. The pack
includes pinned license/provenance. Generated resources and external caches are
ignored; the resource lock is checked in.

The duplicate FRLG builder and Python tests are removed. `frlg_battle_data.py`
is a thin CLI forwarder to `pokesim.games.frlg.battle_data_export`; set
`PYTHONPATH=<pinned-tools>/src` when running it. All five existing battle-data
outputs reproduced byte-for-byte after the move. Map-builder tests already live
beside their source in PokemonSimulation. Native game tests remain here.
The shared Sinnoh decomp CLI remains a separate consumer migration.

Native map, story, transition and recovery tests must run from this tree without
the old map directory before adopting this handoff in the owning chain. This
fork setup is not yet an upstream resource submission: handoff must make the
required data available through the agreed Packages repository/process.

## Full native test fixtures

Fork CI pins `dayfine/CommandLineTests` at
`d150a6c547434110a920f95ba7857b93d5bb9222`. This includes the pending FRLG
fixture submissions needed by the existing native image tests and preserves
all current upstream fixture files. It is a test-data pin, independent of the
map resource lock. The upstream-only fixture checkout lacked 69 required frames;
those failures must not be skipped or treated as passing detector tests.
Upstream handoff must also arrange publication of the required image fixtures.
