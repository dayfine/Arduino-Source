# FRLG map resources

FRLG's existing `overworld_map_json()` API now embeds JSON from prepared
`Packages/Resources/Pokemon/Maps/frlg`, rather than an authored source-map copy.
Its native map decoder, route/story/recovery code and controller behavior are
unchanged. Resource preparation happens before configuring, with no implicit
fallback to the deleted source directory and no map download during configure.
This is a change of build input, not runtime filesystem loading.

The resource lock pins PokemonSimulation main `40717c7c809c415c751fd7a348dfc1895f7ec3d7`.
All 53 maps were compared byte-for-byte with the original native tree before
removing that copy. This initial adoption uses the approved v1 pack; upgrading
to v2 is a later explicit lock change after Simulation #56 lands and native
validation passes. #114's shared v2 reader is already present in this handoff.
BDSP's held map differences are not imported or overwritten.

Clone a full tools/data checkout outside deployed Resources, then explicitly
select the pinned tool version used by CI (a draft dependency until Simulation
#57 lands):

```sh
git clone https://github.com/dayfine/PokemonSimulation.git /path/to/pokesim
git -C /path/to/pokesim checkout 0efe131008c4bcdceb4d036a7022171cbcb2375a
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

The old FRLG Python builder is retained temporarily because `frlg_battle_data.py`
imports its decomp reader. Move that dependency to the simulation owner before
removing the module; do not break battle-data regeneration as incidental cleanup.
The shared Sinnoh decomp CLI also remains a separate consumer migration.

Native map, story, transition and recovery tests must run from this tree without
the old map directory before adopting this handoff in the owning chain. This
fork setup is not yet an upstream resource submission: handoff must make the
required data available through the agreed Packages repository/process.
