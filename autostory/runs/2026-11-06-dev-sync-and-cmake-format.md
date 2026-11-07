# Dev feature sync, upstream #1845 refresh, and CMake formatting

User authorized removing use_qt_ui from upstream #1845, an amend-aware force
option for the dev integration merge, resolving #1845's upstream conflict,
and CMake format targets. No upstream PR was created/reopened/marked ready and
no upstream description was edited. No app/console/save/device operations.

## Upstream #1845

Fork branch `upstream/headless-tests`, worktree `~/Projects/wt/pr207-upstream`.
Removed the standalone runner's use_qt_ui definition and unused Globals include
at `61f361a6c`. The normal push was rejected for four inherited findings against
fork main; intended-upstream-base Code Health passed. User approved exactly that
one-time pre-push bypass, and it was used only for the authorized fork push.
The first native runs dispatched before that push completed were old-head runs;
they were cancelled and never counted as validation.

The actual 61f native link failed: SerialPABotBase2_Connection still referenced
use_qt_ui on that old base. Current upstream main already has #1856
(`91fbbce08`) removing the function and its consumers. Merged upstream main
`bd350bb90` into the handoff branch and resolved the single CMake option-block
conflict by keeping both TEST_RUNNER_EXE and upstream Python-binding/miniz
options. The native unit-test registration auto-merge also preserves upstream's
new tests. No separate UI implementation/refactor was added.

Merge head `1f0edf090162956e38ea33bb48d53a70c13bcc27` has no use_qt_ui references
under Common/SerialPrograms and passes Code Health against upstream/main.
The fork-main hook instead reported 36 inherited upstream findings. The user
approved a second, specific one-time bypass for this merge push. It was pushed
to dayfine/Arduino-Source only; #1845 now reports MERGEABLE, pending normal checks.
Do not reuse either approval for a future bypass.

Fresh native runs at 1f0 (all still pending when recorded):

| Compiler | Run |
|---|---|
| GCC | 42350588096 |
| Linux Clang | 42350588054 |
| Windows Clang | 42350588079 |
| MSVC | 42350587902 |
| macOS ARM | 42350588150 |
| macOS Intel | 42350587909 |

Old 61f failure runs are historical evidence, not tests of the merged head.
Do not claim a full native pass until fresh results exist. The fork #207 source
branch was not rewritten; the user-requested update is on the upstream handoff.

## Dev source sync: fork #246

Tools branch `tools/dev-feature-sync`, head `ed9e8e5b3`, based on agents.
New command, available immediately from its review checkout:

```sh
python3 ~/Projects/wt/dev-feature-sync/lab/scripts/sync_dev.py B bdsp/my-fix --force
```

Normal mode does an ordinary merge. Force mode compares previous/current
feature trees, only replaces paths whose integration blob/mode is still the old
feature (or already new), and records a two-parent merge retaining all history.
Integration customizations, unrelated dirty work and other pending merges are
refused. Matching pending merge work/index is backed up before abort; ordinary
commit hooks are never bypassed. Uses the same console/build locks as launcher.
The helper also handles a feature amended again while its merge is pending.

All 33 launcher/sync tests pass locally and Agents Check 42350251572 passes.
They use temporary Git repositories and linked worktrees; no consoles.

Applied to actual configured B: old integration `81262c296`, prior feature
`51ec5c915`, pending incoming `07affe49b`, current feature `48dec6f5`.
New integration `0c4ec5e1a16ce342139efef90dc8a3dc152fa6d3` is clean, first parent
81262 and second parent 48dec. Only the two user-feature paths changed:
OreburghMine.json and StoryTable_Tests.cpp. Their contents match that committed
feature snapshot; no map correction was authored/imported by this sync.
The conflict/index/files backup is:
`~/Projects/Arduino-Source/.git/dev-feature-sync-backups/20261107T070942Z-32032242`.
The user's feature checkout itself was not modified.

## CMake formatting: fork #247

Stacked on #245, head `0dedcfeafe3b2a33c68809d31c08f989f6379de9`.
Parent workflow path filters updated in #244 (`16a53cc2a`), merged into #245
(`b31e01930`), then into #247. No parent history was rebased or force-pushed.

Adds explicit format and format-check custom targets before the core-only early
return. They call #245's incremental formatter, discover Python only at target
time, and do not depend on app targets or clang-format. The configured source
checkout is the formatting scope; this is not implicitly the terminal's checkout.
Options: PA_FORMAT_BASE (HEAD default) and optional PA_FORMAT_PYTHON. Dependency
errors are clear and do not break normal configuration.

All 38 local tests pass, including real CMake hosts with spaces in source/build
paths, non-writing checks, staged-index preservation, missing Python, committed
parent refs and an ALL target proving no application build dependency. Formatter
CI 42350780340, Code Health 42350780336 and tabs 42350780332 pass.

Also configured the real SerialPrograms tree with PA_CORE_ONLY=ON and
COMMAND_LINE_EXE=OFF at `~/Projects/dev-builds/format-target-validation`, holding
the shared build mutex, and ran both actual format targets successfully. No C++
app build was performed. Configure log: `/tmp/cmake-format-real-configure.log`.

```sh
cmake --build /path/to/configured-build --target format-check
cmake --build /path/to/configured-build --target format
```

The source branch must first contain #247 and be reconfigured. Existing dev
builds do not gain the target automatically before the code is picked up.
CTest registration remains a separate follow-up: native SerialProgramsTests is
not yet registered with add_test in #1845. No CTest support is claimed here.
