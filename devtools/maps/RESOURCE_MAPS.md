# BDSP map resources

BDSP's existing `bdsp_map_json()` API embeds the 22 maps from prepared
`Packages/Resources/Pokemon/Maps/bdsp`. The shared nav-core reader handles their
compact v2 format. Source-map copies and the duplicate BDSP builder/Sinnoh parser
are removed; Python authoring and its tests live in PokemonSimulation. Native
map/route/story tests remain here. No controller behavior changes in this handoff.

## Publishing prerequisite and pins

[PokemonSimulation #65](https://github.com/dayfine/PokemonSimulation/pull/65)
merged by squash as `4de7742a764d27293b4525d6ad2a6fc8a95f3e3a`. The checked-in resource lock and
CI tools now use that approved revision. The squash preserves the reviewed
candidate's manifest/license and all 22 map bytes. Normal resource preparation
verifies the published ancestry, manifest hash and exports before configuration.

Before enrichment, all 22 maps matched #226 at
`fc56a720b9c0b1f4d945dbef382d61b0a6382b97` byte-for-byte. The publishing change
adds resolved Diamond/Platinum-baseline identities, quantities, collection flags
and provenance to 15 pickups, with no geometry, point, NPC, trigger or warp edits.
Pickup names and quantity 1 are omitted on the wire: `source_event: 3`
restores internal label `item_3`; `item: escape_rope` identifies the reward.
Explicit custom names and quantities above one remain supported.
These are explicitly remake baselines, not verified BDSP item data. Reading a
map never grants an item or proves that a controller action collected it.

The shared v2 normalizer preserves all JSON extensions. The existing native
`MapData` model consumes its known navigation fields (including collection
flags); typed battle/inventory consumers for reward metadata remain future work.

## Prepare and build

On a fresh Arduino checkout, clone Packages before preparing the map subdirectory.
Skip this command when the checkout already has its Packages repository:

```sh
git clone --depth 1 --branch master https://github.com/PokemonAutomation/Packages.git /path/to/Arduino-Source/Packages
```

Use the same approved tool revision pinned in CI. Preparation reads the separate
data revision from the resource lock; tools and data pins are independent.

```sh
git clone https://github.com/dayfine/PokemonSimulation.git /path/to/pokesim
git -C /path/to/pokesim checkout 4de7742a764d27293b4525d6ad2a6fc8a95f3e3a
PYTHONPATH=/path/to/pokesim/src python3 -m pokesim.maps.consumer_sync prepare \
  --layout resources --source /path/to/pokesim --consumer /path/to/Arduino-Source
cmake -S SerialPrograms -B /path/to/build \
  -DPOKESIM_TOOLS_SOURCE=/path/to/pokesim -DPOKESIM_SOURCE_CACHE=/path/to/pokesim
cmake --build /path/to/build -j4
```

Prepared resources include license/provenance and are ignored by Git. CMake
checks their exact inventory, hashes and publishing ancestry before embedding.
Missing, modified, extra or unapproved files fail; configuration never fetches
maps or falls back to an old authored directory.

To deliberately advance the approved BDSP data pin after publication:

```sh
PYTHONPATH=/path/to/pokesim/src python3 -m pokesim.maps.consumer_sync update \
  --layout resources --source /path/to/pokesim --consumer /path/to/Arduino-Source \
  --game bdsp --revision FULL_APPROVED_PUBLISHING_SHA
```

Update validates the existing lock and files before changing either; it preserves
independent game pins. Commit the resource lock, not the generated Packages files.
Do not rewrite the resource pin merely to make a locally edited map pass.

## Offline tools and tests

The existing decomp CLI forwards to the publisher parser. Fetch and overlay
commands still work without the Python package. For decompilation, supply the
approved tools on PYTHONPATH:

```sh
PYTHONPATH=/path/to/pokesim/src python3 -m devtools.maps decomp ROUTE_204_NORTH --text
python3 -m unittest devtools.maps.tests.test_overlay
cmake -S devtools/runtime-map-tests -B build/runtime-map-tests
cmake --build build/runtime-map-tests -j4
ctest --test-dir build/runtime-map-tests --output-on-failure
python3 devtools/runtime-map-tests/prepare_parity.py --publisher /path/to/candidate \
  --publisher-revision 4de7742a764d27293b4525d6ad2a6fc8a95f3e3a --out /tmp/bdsp-map-pairs.json
build/runtime-map-tests/RuntimeMapTests /tmp/bdsp-map-pairs.json
```

Parity fixtures require a clean checkout of the exact supplied candidate SHA.
They can test an unmerged candidate; that does not approve it as a resource pin.
The native schema workflow compares 150 complete records on three platforms.
Full application build/story validation is a separate gate after publication.
This fork resource setup is not an upstream Packages submission.
