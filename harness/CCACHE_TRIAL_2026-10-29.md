# Local ccache trial, 2026-10-28–29

User requested trying shared local compiler caching and tracking the reusable
configuration in a draft PR. Draft: https://github.com/dayfine/Arduino-Source/pull/57

- Branch: `build/local-ccache`, commit `9e29b67aebeb53a933dc30a06209adeaf7363334`.
- Base/source revision for measurements: `fd9134c19` (`origin/main`).
- Primary worktree: `/Users/difan/Projects/Arduino-Source-ccache`.
- Apple Clang 21.0.0 (`clang-2100.0.123.102`), CMake 4.4.4, ccache 4.14.2,
  arm64, RelWithDebInfo, four build jobs.
- Qt 6.8.3 from the existing shared install; OpenCV 4 via pkg-config;
  existing macOS 15.4 AGL stub as documented by the lab workflow.
- Installed ccache via Homebrew. Cache:
  `/Users/difan/Library/Caches/pokemonautomation-ccache`, 10 GB cap.
- Opt-in configure/build preset `macos-ccache`; existing presets and CI unchanged.
  Cache settings are compiler-launcher arguments; debug paths are normalized
  without disabling ccache directory/header checks.

## Measurements

| Case | Seconds | Cache hits | Cache misses | Exit |
|---|---:|---:|---:|---:|
| cold-core | 58.620 | 0 | 130 | 0 |
| unchanged-core | 0.078 | 0 | 0 | 0 |
| peer-core | 2.309 | 130 | 0 | 0 |
| source-edit-core | 2.652 | 0 | 1 | 0 |
| header-edit-core | 24.651 | 0 | 50 | 0 |
| restore-core | 1.072 | 50 | 0 | 0 |
| cold-gui | 759.785 | 224 | 1380 | 0 |
| unchanged-gui | 0.366 | 0 | 0 | 0 |
| peer-gui | 62.702 | 1602 | 2 | 0 |
| unchanged-peer-gui | 0.360 | 0 | 0 | 0 |

These are single local observations, not controlled benchmark guarantees.
The first full GUI build reused 224 core compilation results; it was not an
entirely empty cache. The second full GUI build overlapped native test execution
on the first binary. The no-change builds scheduled no compiles.

Source/header invalidation used a temporary `static_assert(sizeof(int) >= 2)`
appended to `Common/Cpp/Time.cpp` / `Time.h` in the isolated checkout. Both files
were restored byte-for-byte before the full GUI builds and commit. The changed
header caused 50 misses; restoring it caused 50 hits.

Compared all 1,604 full-build object files across the two worktrees. Only
`SerialProgramsLib_autogen/mocs_compilation.cpp.o` and `qrc_darkstyle.cpp.o`
differed, matching the two cache misses. No attempt was made to hide those inputs
from hashing. The two full apps linked and deployed successfully.

## Verification

JSON/preset listing, relative documentation links and whitespace checks passed.
Use `git -c core.whitespace=cr-at-eol diff --check` to account for the repository's
existing CRLF JSON/Markdown. Harness self-tests: 22 passed; incremental C++
code-health check: zero failures (no changed C++ source).

Full verification uses the existing `devtools/verify.py`, offscreen Qt and
profile `agent-native-tests`. Fixtures are CommandLineTests revision
`c824d8a162e5d541c86912d0cfaf9b9ce030f265`. Initial run: 20,144 passed, 5 failed,
0 skipped, exit 1. All five failures were missing PaddleOCRDetection in the
selected test profile. Made the already-installed model available via a symlink
under that profile's DownloadedResources and reran verification.

Final rerun: **20,149 passed, 0 failed, 0 skipped; verification exit 0**.
GitHub code-health and no-tabs checks passed; platform build jobs were skipped
by existing CI conditions. Local full builds supply the application-build evidence.

## Artifacts and cleanup

Raw logs, measured commands, per-run cache counters, object comparison and binary
SHA-256 are in `/Users/difan/Projects/Arduino-Source-research/ccache-trial/`.
The disposable `Arduino-Source-ccache-peer` worktree and its approximately 2.1 GB
build directory were removed after comparison. The primary full build is retained
for this PR's tests and future incremental builds. Runtime lab deployments and
source in other active worktrees were not changed.

Adoption in another worktree: apply the PR, install ccache if absent, configure
with `cmake -S SerialPrograms --preset macos-ccache` plus that machine's normal
Qt/SDK options, then use `cmake --build build/macos-ccache --parallel 4`.
This does not reduce retained deployment/resource snapshots.

## Binary size and documentation follow-up

Measured the retained trial build (same source/preset as above):

| Artifact | Size |
|---|---:|
| `SerialPrograms` executable | 30,136,832 bytes (28.74 MiB) |
| `libSerialProgramsLib.a` | 917,660,608 bytes (875.15 MiB) |
| `libCoreLib.a` | 44,592,416 bytes (42.53 MiB) |
| App bundle, allocated disk space | about 29 MiB |
| Runtime Resources, allocated disk space | about 198 MiB |
| Entire GUI build directory, including outputs above | about 2.0 GiB |
| Shared ccache compressed data | 338.7 MB (26.4% of original cached output size) |

These are current sizes, not a controlled cache-on/cache-off binary comparison.
The app bundle does not include all externally installed runtime dependencies.
Ccache compression applies to the cache, not to the linked executable; debug
path normalization can change metadata. Do not interpret `size`'s Mach-O
PAGEZERO virtual address reservation as file size or resident RAM.

Recommendation: retain CMake/Ninja + ccache for the demonstrated local worktree
reuse. No Bazel implementation was benchmarked. Revisit broader action caching
or remote execution when generation/link/test time or genuinely new compilation
work warrants a build-system/infrastructure investment. Compiler caching alone
does not solve retained deployment copies.

Documentation follow-up on PR #57 separates `Build-Mac-CompilerCache.md` from
the general `IncrementalBuilds.md` overview, links it from the main Mac guide,
and explicitly identifies missing validated local Windows cache instructions
(normal Windows build instructions already exist). Also repaired an existing
broken UpdateClang.md link encountered during validation. Relative links,
heading anchors and CRLF-aware whitespace checks passed. No build/preset/code
changed in this follow-up, so the full native suite was not repeated.
