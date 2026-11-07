# Full native validation follow-up

User requested completing full native validation now. No local console, GUI,
save or profile operations are part of this validation.

## Baseline results at b24bc1848

| Compiler | Run | Result |
|---|---|---|
| GCC | 42337637019 | Compiled; 32,961 passed / 69 missing-image FileExceptions |
| Clang (Linux) | 42337636983 | Compiled; 27,031 passed / 69 missing-image FileExceptions |
| Clang (Windows) | 42337637015 | Compiled; 32,964 passed / 69 missing-image FileExceptions |
| MSVC | 42337636992 | C4244 under /WX in RNG top-bit literal loop; tests not reached |

All observed baseline test failures in the three completed suites were missing
images, not failed detector assertions. Platform-dependent counts are retained
as reported, not combined into one cross-platform total.

## Repairs and review branches

#238 merged into #224 at `fa131a8b09669913808adf860c2358b7fdaafaf1` while validation
was ongoing. Do not reopen it. Fork #243 is the three-file follow-up against
`frlg/stage7-celadon`, on `frlg/map-resource-handoff` at `7a5b51f12`.

- MSVC: explicitly type `0x0000` and `0x8000` as uint16_t in the loop initializer.
  Values and search behavior are unchanged. The active owning branch is #214,
  not the older standalone #181: its source matches the aggregate and already
  contains the lint cleanup. Owner commit `d633063b1` is pushed and merged into
  the validation branch. An attempted push to the old #181 was refused for 29
  inherited lint findings; it was not bypassed. The resulting exploratory merge
  was aborted. An unpublished local commit remains on `integration/frlg-rng-msvc-ci`;
  do not merge that stale standalone version into the story chain.
- Fixtures: fork CommandLineTests main already covers 58/69 missing paths. Its
  pending #10/#11 and moon-stone branch supply the remaining 11. The complete
  fixture branch also retains the one newer upstream SV frame absent in fork
  main. All 1,198 current upstream files are preserved byte-for-byte; coverage
  of all 69 missing paths is checked by complete Git tree/blob comparison.
- Fixture fork draft #12: `validation/frlg-native-complete`, commit
  `49e507fde8c3a54cd0264db0dbea7c3ee3618085`, based on fork main `7c6378e24`.
  It adds 12 existing public image blobs, with exact original revisions/blob IDs
  in the commit message. No new captures, image edits or test exclusions.
- #243's CI pins that complete fixture revision. Map data pins remain unchanged.
  Upstream handoff must arrange publication of required image fixtures too.

The recreated validation branch initially used `d075fccd5` for dispatch. The
additional merge acknowledges the squash-merged #238 baseline so #243 shows only
these repairs. Before that merge, the parent tree was verified identical to
`b24bc1848`; after it, the complete tree was verified identical to `d075fccd5`.
No user/other-agent changes were discarded.

## Fresh full native matrix

All dispatched against `d075fccd5a5826f84319abdfdd7dcac8c67f7624`, whose complete
tree matches #243. Linux/Windows builds are in progress; macOS runners are queued.
Code health and the focused Linux parity job already pass on the follow-up.

| Job | Run URL |
|---|---|
| GCC | https://github.com/dayfine/Arduino-Source/actions/runs/42340105885 |
| Linux Clang | https://github.com/dayfine/Arduino-Source/actions/runs/42340106165 |
| Windows Clang | https://github.com/dayfine/Arduino-Source/actions/runs/42340106132 |
| MSVC | https://github.com/dayfine/Arduino-Source/actions/runs/42340105953 |
| macOS ARM | https://github.com/dayfine/Arduino-Source/actions/runs/42340105875 |
| macOS Intel | https://github.com/dayfine/Arduino-Source/actions/runs/42340106134 |

Do not claim a green full suite until results exist with these complete fixtures.
Inspect all failures and record counts/results. Superseded queued macOS runs on
this validation branch were cancelled to avoid building old fixture/code inputs.
Other branches' runs are untouched. Actual console/detector timing remains a
separate live validation concern.
