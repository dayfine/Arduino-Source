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

## Proposal

Use option 2 for lab deployments and option 5 for worktrees that have been idle
for more than a few days, then measure with `df` before and after.

Open questions:
- Should the console-B copy (`<name>-B`) share the `.app` too? Today it is a
  second signed copy with a different bundle id.
- Should `Resources` hashing skip files the app writes at runtime, if any?
- Keep this in lab scripts only, or document it next to the ccache guide?
