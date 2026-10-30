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

## What works and what doesn't

Tried on 2026-10-29 with seven lab builds, each with its console-B copy, deployed into a scratch
folder and measured with `df` (which counts shared blocks once). The seven builds hold two distinct
`Resources` trees: BDSP's (1,467 files, 300 MB) and upstream's (686 files, 199 MB). A plain copy of
all 14 deployments took 4,212 MB.

| Technique | Works? | Disk for 14 | Why |
|---|---|---|---|
| Content-hash `Resources` snapshots, linked from each deployment | **Yes, in use** | 912 MB (500 MB of trees, 420 MB of `.app`) | One copy per distinct tree, whatever the build does; a deployment can only see the tree it was built with |
| APFS clones (`cp -c`) for the console-B `.app` copy | **Yes, in use** | the second copy is close to free | Clones of a just-made copy share blocks. macOS only, and `du` shows full sizes (use `df`) |
| APFS clones for `Resources` across deployments | No | ~0 in the trial, a full tree per deploy in practice | Every build re-copies `Resources`, so a clone from a fresh build shares nothing with older deployments |
| One `Resources` folder for all deployments | No | 718 MB | Version skew: 2 of the 7 builds would silently read the other branch's files |
| Hard links (`pax -rwl`) | No | 965 MB | Not smaller than snapshots, and writing one file in place changes it in every deployment. macOS's openrsync didn't link with `--link-dest` at all |
| Dropping idle worktrees' build directories | **Yes, in use** | 2.1 GB each | ccache rebuilds a cached revision in about a minute. Keep the worktree itself (see Pitfalls) |
| Sharing one `Packages` checkout between worktrees | **Only for the same resource set** | 0.8 to 1.1 GB each | Saves a copy where branches need the same resources; wrong for branches with their own (BDSP), see Pitfalls |

## In use now

`~/BDSPLab/bdsp_deploy.sh` stores `Resources` as snapshots in `builds/.resources/<hash>/` and clones
the console-B copy; `prune_builds.sh` keeps the 5 newest deployments per build family (`bdsp-r105`
and `frlgnav8` are families `bdsp-r` and `frlgnav`) and deletes snapshots nothing links to. Result:
12 deployments take 587 MB (199 MB of it one shared snapshot) instead of 4.2 GB, and the three
worktree build directories in use take 6.4 GB.

## Pitfalls found on 2026-10-29

- **A build's `Resources` come from its worktree's `Packages`.** Sharing one `Packages` checkout
  between worktrees (a link) saves 0.8 to 1.1 GB each, but only where the branches need the same
  resources. BDSP AutoStory needs its own set (1,467 files, with #1717's 789 AutoStory templates);
  a worktree linked to upstream's set (686 files) built fine and then failed at run time with
  "FileException: Unable to open file" on the first missing template.
- **`Resources` is copied once.** The copy is a post-build step that runs only when the build
  directory has no `Resources` yet, so pointing a worktree at another `Packages` doesn't refresh
  it: delete `<build>/Resources` and rebuild, or copy it by hand.
- **Deployments aren't self-contained.** The app finds `onnxruntime` through an rpath into the
  worktree it was built in. Deleting an idle worktree's build directory is safe; deleting the
  worktree breaks every deployment built from it (dyld "Library not loaded").
- **Look for links before deleting a folder.** Lab profiles linked their `DownloadedResources` to
  one profile's copy; deleting that copy broke resource downloads in six profiles.

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
