# Evaluated native story checkpoint capture

This first adapter exports FRLG stage 1's actual `story_checkpoints()` rows.
It does not parse C++ text, construct a story executor, load a save, open a
profile or send controller inputs. It is a full-library build target, not the
Qt-free core target; linking Qt libraries does not create an application/window.

From a clean committed checkout with the usual native build dependencies:

```sh
cmake -S SerialPrograms -B build -DPA_FRLG_STORY_EXPORT=ON
cmake --build build --target StoryCheckpointExport -j4
build/StoryCheckpointExport 00.05 > frlg-parcel.native.json
```

On multi-configuration generators add `--config RelWithDebInfo`, then run
`build/RelWithDebInfo/StoryCheckpointExport.exe`. The current full-library CI
builds the target on every platform and captures Parcel as a Linux artifact.
Reconfigure after switching source revisions: `source_revision` names the clean
revision used at configure time. Canonical captures should come from clean CI.

The `pokesim-native-checkpoint-export/1` capture contains the shared checkpoint
codec plus all fields of this source revision's FRLG checkpoint extension:
menu and supplies (including party moves/PP, optional stats, slot zero, and
foe bookkeeping). Defaults remain native bookkeeping, not measured state or
requirements. No inference of full HP, actual inventory or battle probability
is permitted from this capture alone.

Runtime configuration, map pins, executor bindings, entry evidence and effective
save/retry policy are explicitly unbound. This is not the proposed executable
story-plan envelope, and must not be passed directly to a console executor.
Other native revisions with more checkpoint fields need an updated adapter.
BDSP `01.03`, policy binding and native/Python execution parity remain next.

Stdout is machine JSON; import/format it through PokemonSimulation's native
export CLI and compact contract formatter before checking in review data.
Tests next to the adapter check the real Parcel row and every native extension
field using synthetic non-default state. They do not validate console timings.
