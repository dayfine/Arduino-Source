# Disk use of local builds and lab deployments

Goal: keep many builds (dozens of lab deployments, several worktrees) within a
few GiB, without risking one build running with another build's files.

## Measured (2026-10-29, Apple Silicon)

| What | Size each | Notes |
|---|---|---|
| Worktree build directory | 2.1 GB | 1.0 GB objects, 0.9 GB `libSerialProgramsLib.a`, 0.2 GB `Resources` |
| Lab deployment (`~/BDSPLab/builds/<name>`) | 334 MB | 300 MB `Resources`, 30 MB `.app` |
| Shared ccache (`macos-ccache` preset) | 0.76 GB total | about 250 MB per fully rebuilt revision; capped at 5 GB |

The ccache preset (PokemonAutomation/Arduino-Source#1767) makes rebuilds fast. It
doesn't make any of these smaller. Today `~/BDSPLab/builds` holds 12
deployments (6 builds, each with a console-B copy) in 3.7 GB, almost all of it
copies of `Resources`.

## What the app already allows

`get_resource_path()` (`CommonFramework/GlobalAutoPaths.cpp`) looks for a
`Resources/` folder next to the executable and then up to five parent folders.
A deployment can therefore use a `Resources` folder it doesn't own, without
any code change.

## Options

1. **One `Resources` folder shared by every deployment.** This is the smallest:
   each build is about 30 MB. The risk is version skew. Resources change between
   revisions (models, JSON, images), so an older build would silently read newer
   files. Rejected.
2. **Content-addressed snapshots.** Hash the `Resources` tree at deploy time and
   store it once as `builds/resources/<hash>/`, which is never modified. Each
   deployment's `Resources` is a symlink to its snapshot, so builds with
   identical resources share one copy and a changed tree gets a new snapshot.
   Pruning deletes snapshots that no deployment links to.
3. **APFS clones (`cp -c`).** Each deployment is an independent copy, and
   identical blocks share disk until written. There is no sharing risk and the
   change is one flag. Drawbacks: macOS/APFS only, and `du` reports full sizes, so
   savings show only in `df`.
4. **Hard links (`rsync --link-dest`).** Unchanged files are shared and changed
   files are copied. Writing a file in place would change it in every
   deployment, and nothing guards against that.
5. **Worktree build directories.** With ccache a full rebuild at a cached
   revision takes about 40 s, so idle worktrees can drop `build/` entirely
   instead of keeping 2 GB of objects each.

## Trial (2026-10-29)

Seven lab builds, each with its console-B copy, deployed five ways into a
scratch folder. Disk use was measured with `df`, which counts shared blocks
once. The seven builds hold only two distinct `Resources` trees: BDSP's, with
1,467 files (300 MB), and upstream's, with 686 files (199 MB).

| Option | Disk for 14 deployments |
|---|---|
| Plain copy (today) | 4,212 MB |
| 1. One shared `Resources` | 718 MB, but 2 of the 7 builds would read the other branch's files |
| 2. Content-hash snapshots + symlink | 912 MB: 500 MB of trees plus 420 MB of `.app` copies |
| 3. APFS clones (`cp -c`) | about 0 MB, cloned from existing deployments |
| 4. Hard links (`pax -rwl`) | 965 MB |

Notes:
- macOS ships openrsync, which didn't hard-link with `rsync --link-dest`.
- The clone result is a best case and doesn't hold for real deploys. Every
  build re-copies the whole `Resources` folder into the build directory: all
  686 files carry the time of the last link. A clone taken from the build
  folder after a rebuild therefore shares nothing with older deployments, and
  each deploy costs a full `Resources` (200-300 MB).
- Snapshots dedupe by content, so they stay at one copy per distinct tree
  whatever the build does.

## Adopted (2026-10-29)

- **`Resources`:** content-hash snapshots in `~/BDSPLab/builds/.resources/<hash>/`,
  linked from each deployment; a prune keeps the 5 newest deployments per build
  family (`bdsp-r105` and `frlgnav8` are families `bdsp-r` and `frlgnav`) and
  deletes snapshots nothing links to.
- **`.app`:** APFS clones for the console-B copy.
- **Worktrees:** build directories only for worktrees in use; the others are
  deleted and rebuilt from ccache when needed (about a minute).

Result: 12 deployments (two build families, with console-B copies) take 587 MB,
199 MB of it the one snapshot they share, against 4.2 GB for 14 before. Three
worktree build directories in use take 6.4 GB.

## Lab output

What the lab apps write grows faster than the builds and isn't covered above:
per-leg route screenshots and move screenshots from AutoStory runs (2.2 GB in
the console-B profile after two days), Command File Runner probe shots (1 GB,
cleared), and rotated logs. These are run evidence, not build output, so they
are cleared by hand after a run is recorded.

Open questions:
- Should `Resources` hashing skip files the app writes at runtime, if any?
- Keep this in lab scripts only, or document it next to the ccache guide?
- Should AutoStory keep per-leg screenshots only for failed runs?
