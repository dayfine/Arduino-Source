# Data-unification checkpoint: FRLG resources

User authorized continuation while trying the two-console dev flow. No console,
GUI, save or panel operation was performed for this work.

## Published and prepared state

- Simulation #62 is merged on main at `b8ee01319ab0089e0bef902c171f6585ede0eff8`.
  It brings already-reviewed #61's PLZA opening draft onto main, preserving all
  four files exactly. All six Simulation CI jobs passed before merging.
- Simulation #63 updates the unification plan and native adoption gates; it
  targets main directly and all six jobs pass. It remains a draft for review.
- Arduino fork #238, `frlg/map-resource-handoff`, is now
  `b24bc1848e3de582b91c46b71f71e8a85996dc83`.
- FRLG's resource lock and tool action pin approved Simulation main
  `906956e94fc912ae9adb0e44845da780f3bc7e4c`, manifest SHA-256
  `856e07a265da0b273172a7aa97a2aa335c90ea0a39162d50912b55fb60a86bc7`.
  The pack is compact v2. All 53 maps preserve previous v1 content after
  canonical v2 conversion; only coordinate spelling, grouping and omitted empty
  sections differ. Terrain, triggers, metadata and per-kind order are preserved.
- The 53 native source maps and duplicate FRLG builder/tests are absent in #238.
  Existing native map APIs still embed prepared resources at build time. This
  does not add a runtime filesystem loader or console-side Python/networking.
- All five native FRLG battle-data files reproduce unchanged through the
  Simulation-owned exporter. Resources pass the strict lock/inventory/hash check.

## Owning-branch fixes

Two GCC `-Wdangling-reference` failures came from checkpoint lookup helpers
accepting temporary `std::string`s. They return references to persistent table
rows, not to those strings. String-view parameters remove those temporaries
without changing tests or gameplay:

- #222: `d034ec75e`, Vermilion Gym helper.
- #224: `8d6bbe7fb`, Celadon helpers; its parent merge is now `34c559bbc`.
- Both are merged through the owning parents into #238. Apple Clang syntax
  checks passed; new expectations/tests were not added for these mechanical fixes.

Nav-core #114 now includes `3df56e073`: its native parity fixture tool accepts an
explicit full immutable publishing revision, still defaults to historical v1
`40717c7c809c415c751fd7a348dfc1895f7ec3d7`, and normalizes v2 before pickup
identity enrichment. Previously, changing only the workflow checkout failed the
old hardcoded pin check; enriching v2 directly would silently do nothing. The
corrected generator produces 150 plain/enriched records, with enrichment
changing 20 of 75 maps. Production C++ performs parity assertions.

## CI boundary

At this checkpoint, code health/tabs and Linux/Windows native schema, eight
legacy MapData cases and 150-record parity pass. macOS runners remain queued.
Latest native schema run: https://github.com/dayfine/Arduino-Source/actions/runs/42337312895

Full native validation remains in progress; do not claim the full suite green:

- Latest Linux: https://github.com/dayfine/Arduino-Source/actions/runs/42337343015
- Latest macOS: https://github.com/dayfine/Arduino-Source/actions/runs/42337343634
- Windows clang: https://github.com/dayfine/Arduino-Source/actions/runs/42336749074
  This run uses `fbc6ec2d1`; later differences are parity tooling and a test-helper
  signature change, not gameplay/decoder/resource changes.

Earlier v1 Windows/macOS full builds completed with exactly 69 test failures,
all missing-image `FileException`s. They are not a green full suite and are not
v2 validation. Old Linux compile failures led to the owning-branch fixes above.
Superseded queued macOS runs were cancelled rather than consuming duplicate
build capacity. No CI failure was suppressed and no hook bypass was used.

## Next implementation

Review #238 and propagate its resource adoption through the native owner chain.
BDSP stays separate: Route201 collision/sign edits, seven mine corridor edits
and the Jubilife president relocation remain held pending anchored evidence.
Do not import those old pre-recovery WIP corrections to satisfy parity.

Then export evaluated native FRLG `00.05` Parcel and BDSP `01.03` mine/Roark
checkpoints, preserving game extras, defaults, skip/once-per-save semantics,
program-generated save/attempt behavior, uncertainty and explicit data bindings.
Compare native serialized records before replacing authored C++ tables; bind
native/Python replay afterward. Current FRLG/BDSP transcriptions and PLZA guides
are review data, not executable exports or battle-ready histories.

## Later validation and dev-flow steering

The v2 Windows run completed: 32,964 cases passed and 69 failed, all missing-image
`FileException`s. No other failed assertions were reported. Latest Linux at
`b24bc1848` now compiles and is running tests; macOS remains queued.

The user also requested device policies and reported the first external-build
configure failure. Both local lab policies were populated from the independent
`~/BDSPLab/consoles.json` A/B record and verified against fresh read-only Qt
serial/video/audio enumeration. Each has exactly one identity per type, no A/B
overlap, mode 0600, and passes the production policy parser. Actual device IDs
remain local. No driver/controller/camera/audio input was opened for this check.

Fork #242, `build/external-generated-source-groups`, fixes CMake IDE grouping for
Qt qrc files generated outside the source tree. Two configure-only regressions
failed with the original source_group call; all five pass after grouping files
marked GENERATED separately. Windows CTest needed explicit `-C Debug`, fixed in
`12bb5be64`. Linux/Windows CI pass; macOS CI remains queued. The actual slot-B
configure now succeeds with the existing external build/cache. Both local dev
branches include the production fix: A `05060b36d`, B `3a13883fd`. No app build,
GUI launch or console operation was performed for this configure fix.
