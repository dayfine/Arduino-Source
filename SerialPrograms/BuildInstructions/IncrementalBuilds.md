# Incremental builds and compiler caching

Keep a configured build directory for each active checkout and configuration.
CMake/Ninja rebuild objects affected by changes; a compiler cache can also
reuse previous compilations after switching revisions or creating another
worktree. Linking, generated-code work and deployment still run when required.

## Platform support

| Platform | Local compiler-cache instructions | Status |
|---|---|---|
| macOS | [Mac compiler-cache guide](Build-Mac-CompilerCache.md) | `macos-ccache` preset validated on Apple Silicon with Apple Clang; Intel Mac performance unmeasured |
| Windows | **Missing: validated local Windows compiler-cache setup** | Normal [Windows build instructions](Build-Windows-Qt6.8.3.md) exist, but they do not establish local cross-worktree cache reuse |
| Linux | Local compiler-cache guide not yet provided | Existing [Linux build instructions](Build-Ubuntu-Qt6.8.2.md) remain the starting point |

CI already uses ccache on supported jobs. That does not substitute for a local
Windows or Linux guide. The Mac preset uses Apple Clang flags and a Mac cache
path; do not apply it to Windows. A Windows follow-up needs to document the
compiler/generator, assembly-listing and debug-symbol settings, cache location,
source/header invalidation, and measured reuse between worktrees. It must be
validated on Windows before being presented as a working recipe.

## What each layer does

| Layer | Reuses | Still requires |
|---|---|---|
| Existing CMake/Ninja build directory | Objects whose dependencies and commands have not changed | Recompilation after relevant source/header/flag changes; one build directory per checkout/configuration |
| ccache | Compatible previous C/C++ compilation results, including across worktrees | A first compilation for new inputs; linking, testing and deployment; local object files in each build directory |
| Bazel with action caching | Cacheable build/test action outputs under its declared dependency model | Migrating and maintaining build rules/toolchains; making inputs and outputs explicit; infrastructure for remote caching/execution if used |

For repeated local C++ builds, first measure the existing incremental build and
compiler-cache hit rate. This adds reuse without replacing the project's Qt,
code-generation, dependency and deployment configuration. If substantial time
remains in uncached generation, linking or tests, profile those steps before
choosing a larger build-system migration. High compile-cache hit rates alone
are not an end-to-end comparison against Bazel.

Bazel's [remote action caching](https://bazel.build/remote/caching) and
[remote execution](https://bazel.build/remote/rbe) are broader capabilities worth
evaluating when those requirements justify maintaining a different build graph.
Remote compiler caching alone is not a reason to migrate: ccache also supports
[remote storage](https://ccache.dev/manual/latest.html#_remote_storage_backends).
No Bazel build or timing comparison is established by the Mac ccache trial.

## Disk usage and binary size

Compiler caching accelerates compilation; it is not a binary-size optimization.
Keep these measurements separate:

- Executable/app size, plus runtime libraries and resources needed for deployment.
- Build-directory size, including objects and static archives with debug data.
- Shared compiler-cache size, bounded by its own eviction policy.
- Retained deployments and logs from previous runs.

Ccache compresses its stored results. Each configured build still materializes
its own objects, and repeated deployments can still duplicate resources.
Resource sharing and retention policies address that separate source of growth.
For executable-size comparisons, hold source, compiler, architecture, flags and
packaging constant. Cache-path normalization can change debug metadata, so do
not infer binary shrinkage from a faster build or compare different revisions
as if caching were their only difference.

The [Mac guide](Build-Mac-CompilerCache.md#measure-binary-and-disk-size) includes
measurement commands. Platform-specific setup, rollback and validation belong
in the respective platform guide.
