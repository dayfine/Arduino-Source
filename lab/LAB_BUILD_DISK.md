# Disk use of local builds and lab deployments

Goal: keep many builds (dozens of lab deployments, several worktrees) within a few GiB, without
one build ever running with another build's files. Measured on Apple Silicon, 2026-10-29; deployment and build sizes rescanned 2026-11-08.

## Do this

1. **Store each deployment's `Resources` as a content-hash snapshot**, linked from the deployment
   (`~/BDSPLab/builds/.resources/<hash>/`, made by `snapshot_resources.sh` from `bdsp_deploy.sh`).
   Builds with identical resources share one copy; a changed tree gets a new snapshot, so no build
   sees another branch's files. The app only reads `Resources` (`get_resource_path()` looks for it
   next to the executable and up to five parents; downloads go to the profile's
   `DownloadedResources`), and no snapshot had a file modified after a day of runs.
2. **Make the console-B copy of a deployment with APFS clones** (`cp -c`, in `make_slot_b.sh`).
   It costs almost nothing; check savings with `df`, since `du` counts clones in full.
3. **Prune deployments to the 5 newest per build family.** For `start_dev.py` deployments
   (`~/Projects/dev-deployments/<slot>/`, one family per slot folder) the lab disk GC does it:
   [`scripts/disk_gc.sh`](scripts/disk_gc.sh), live copy `~/BDSPLab/disk_gc.sh`, run by launchd job
   `com.dayfine.lab-disk-gc`. It keeps the newest 5 by mtime, never the one a running SerialPrograms
   loads from, and never a symlink; `disk_gc_test.sh` checks that. For the older
   `~/BDSPLab/builds` deployments, `prune_builds.sh` keeps 3 per family (`bdsp-r105` and `frlgnav8`
   are families `bdsp-r` and `frlgnav`) and deletes snapshots nothing links to.
4. **Keep build directories only for worktrees in use**, with the `macos-ccache` preset
   (PokemonAutomation/Arduino-Source#1767): a cached revision rebuilds in about a minute, so an
   idle worktree's 2.1 GB `build/` can go. Keep the worktree itself (see "Don't").
5. **Share a `Packages` checkout only between worktrees that need the same resource set.**
   Upstream branches can link to one checkout; BDSP AutoStory branches use `~/BDSPLab/packages`.
6. **Clear lab run output after the run is recorded**: AutoStory route and move screenshots (keep
   `failures/` and `saves/`), Command File Runner probe shots, and old logs. They grow faster than
   the builds (2.2 GB of AutoStory screenshots in two days).

These stay in the lab scripts; the ccache guide (#1767) covers the only part upstream needs.

## Don't

| Technique | Why it fails | Measured (14 deployments; plain copies 4,212 MB) |
|---|---|---|
| One `Resources` folder for every deployment | A build silently reads another branch's files: 2 of the 7 builds tried would have | 718 MB |
| APFS clones for `Resources` across deployments | Every build re-copies `Resources`, so a clone from a fresh build shares nothing with older deployments | ~0 in the trial, a full tree (200-300 MB) per deploy in practice |
| Hard links (`pax -rwl`; macOS's openrsync ignores `--link-dest`) | No smaller than snapshots, and writing a file in place changes it in every deployment | 965 MB |
| Deleting a worktree that has deployments | The app loads `onnxruntime` through an rpath into the worktree it was built in; its deployments then crash at launch (dyld "Library not loaded") | — |
| Linking a worktree to a `Packages` checkout with another resource set | Builds fine, then fails at run time on the first missing file ("FileException: Unable to open file") | — |
| Deleting a folder without looking for links into it | Lab profiles linked `DownloadedResources` to one profile's copy; deleting it broke six profiles | — |

After pointing a worktree at another `Packages`, delete `<build>/Resources`: the build copies it
only when the folder is missing (a post-build step), so it won't refresh by itself.

## Numbers

| What | Size |
|---|---|
| Worktree build directory | 2.1 GB (1.0 GB objects, 0.9 GB `libSerialProgramsLib.a`, 0.2 GB `Resources`) |
| Lab deployment, plain copy | 334 MB (300 MB `Resources`, 30 MB `.app`) |
| `Resources` trees | BDSP 1,467 files, 300 MB; upstream 686 files, 199 MB |
| Shared ccache | about 1 GB; about 250 MB per fully rebuilt revision; capped at 5 GB |

Scan of 2026-10-29 (85 GB free of 460 GB):

| Item | Size |
|---|---|
| Lab deployments (16, with three snapshots) | 1.2 GB, 0.7 GB of it snapshots |
| Build directories in use (`bdsp/floaroma-mvp`, `frlg/brock-mvp`, `lab/frlg-nav-cfr`) | 6.4 GB |
| Other build directories (`bdsp/new-game-intro-v2`, `build/local-ccache`, `bbq/m8a`) | 6.2 GB |
| `Packages` checkouts (BDSP and upstream) | 1.8 GB |
| Qt 6.8.3 (debug symbols removed) | 1.6 GB |
| ccache | 0.9 GB |
| SerialPrograms profiles and logs | 4.8 GB |

Scan of 2026-11-08, 20:00 (`du -sh`; 38 GB free of 460 GB). `start_dev.py` deployments are plain
copies of about 236 MB each (199 MB `Resources`, 33 MB `.app`, 4 MB `Firmware`); slot-A ones also
hold run output written into the deployment folder, up to 404 MB.

| Item | Size |
|---|---|
| `~/Projects/dev-deployments/slot-B` before the user pruned it by hand to the newest 3 | about 50 deployments, 12 GB |
| `~/Projects/dev-deployments/slot-B` (3 deployments, 236 MB each) | 708 MB |
| `~/Projects/dev-deployments/slot-A` (6 deployments, 242-404 MB) | 1.9 GB |
| Build directory `~/Projects/dev-builds/slot-A` | 2.5 GB |
| Build directory `~/Projects/dev-builds/slot-B` (1.5 GB `CMakeFiles`, 1.0 GB `libSerialProgramsLib.a`, 199 MB `Resources`) | 2.8 GB |
| Build directory `~/Projects/dev-builds/slot-B-frlg` | 511 MB |
| Build directory `~/Projects/dev-builds/slot-B-plza` (build in progress) | 196 MB so far |
| Older `~/BDSPLab/builds` deployments | 1.3 GB |
| ccache | 4.6 GB |

With the 5-per-slot rule, a slot's deployments stay near 1.2 GB (5 x 236 MB) plus run output,
instead of the 12 GB slot-B reached in about two days of builds. The first dry run of the rule
would delete one deployment, slot-A's oldest (265 MB).
