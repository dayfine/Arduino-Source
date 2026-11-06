# Versioned map reader preparation

`Pokemon_RuntimeMap` adds wire normalization inside the existing shared
`parse_map()` entry point. Both games gain v1/v2 support through that same API. It accepts unmarked or
explicit v1 and `pokesim-runtime-map/2`. The wire conversion is independent of
FRLG/BDSP and does not duplicate either game's map parser.

V2 uses `coord: [x,y]` and inclusive wander `min`/`max` pairs. Typed arrays decode
in NPC/item/sign/interactable order, preserving order inside each group and
allowing repeated names. Interactables retain their interaction payload and use
the historical item-sprite occupancy kind for the legacy native model; decoding
one grants no item or Pokemon. Pairs must contain exactly two integer coordinates
that fit native `int`. Mixed encodings and unsupported versions fail with a
source/path error. Source/world coordinates and trigger-box fields retain the
existing publisher contract. Extensions, zeros and false values survive in the
normalized JSON; the existing `MapData` model still consumes only its known fields.

The decoder is integrated into nav-core #114. Existing game loader call sites
remain unchanged; merge nav-core into each story chain to pick up support.
Published maps, generation code, resource paths and pins remain unchanged. Run
the games' route/story/recovery tests before activating a v2 resource pack.
Full route-policy parity and the five pending BDSP content differences remain
separate work.

## Offline native checks

```sh
cmake -S devtools/runtime-map-tests -B /path/to/build
cmake --build /path/to/build -j4
ctest --test-dir /path/to/build --output-on-failure
```

The focused target also runs all eight existing registered MapData cases,
including their legacy diagnostics and embedded map assets. The 26 new cases are registered in the application's native test
database. They cover v1 compatibility, mixed/malformed versions and pairs,
inclusive wander bounds, object groups, interaction-vs-item rejection, coordinate
events and complete normalized JSON parity, including unknown metadata.

For real data, prepare a clean PokemonSimulation checkout at
`40717c7c809c415c751fd7a348dfc1895f7ec3d7` (merged #53), then run:

```sh
python3 devtools/runtime-map-tests/prepare_parity.py \
  --publisher /path/to/PokemonSimulation --out /tmp/runtime-map-pairs.json
/path/to/build/RuntimeMapTests /tmp/runtime-map-pairs.json
```

The preparation command refuses another revision, tracked local edits or an
existing output. It generates 150 input/expectation records: all 75 checked-in
maps, both plain and with staged pickup/interaction enrichment. The expected
values come from that pinned Python codec; the native executable performs the
full JSON comparison and invokes both versioned and legacy production decoders.
No game maps or decomp inputs are copied into this PR. CI repeats these checks
on macOS, Linux and Windows with the immutable publishing pin. These are decoder
checks, not native walker replay or live-console validation.
