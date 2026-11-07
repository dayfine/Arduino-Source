# Preserve established source layout in Uncrustify

User requested tuning the formatter to the existing main-tree conventions,
rather than treating the original bulk output as cleanup. Fork #244 now uses
`d8b038f93aec4fcc280568f744d0e983f7c4156b`; no application sources are in its diff.
Main trial remains pinned to `2a692970b6ac4480f2af405d0adc964192df04e0`.

## Changes and evidence

Added preservation regressions first: short functions/empty constructors,
manual overload spacing, multiline argument/closing-parenthesis layouts and
`class...` parameter packs failed against the earlier config. The tuned config
preserves those supported shapes, plus lambda bodies inside calls and attached
catch chains. All 21 tests pass locally and in CI.

The requested arbitrary leading declaration alignment cannot be preserved by
Uncrustify's configuration: it reindents leading spaces to the block level.
Tests document this gap and show that existing `*INDENT-OFF*` / `*INDENT-ON*`
markers preserve a deliberate table exactly. No application markers were added.
Inherited class/struct brace spacing also remains a documented limitation.
Do not claim that all existing manual alignment or all compact braces are fixed.

Keeping the first continuation column verbatim caused three main files to
change on a second pass, including nested lambda/table examples. Rejected that
setting and added stability regressions. The final configuration uses stable
four-space continuations and retains authored closing-parenthesis columns.

## Main comparison

| Metric | Before tuning | Final |
|---|---:|---:|
| Files checked | 3,169 | 3,169 |
| Files changed | 2,839 | 2,202 |
| Insertions | 54,021 | 26,399 |
| Deletions | 49,302 | 26,193 |
| Total changed lines | 103,323 | 52,592 |

Total line churn falls by 49.1%. The remaining patch still contains intentional
layout changes and known spacing gaps; it is not approved for bulk application.
All final outputs are stable after one formatting pass. No formatter diagnostics,
include-order changes, LF/CRLF changes or non-whitespace-byte changes were found.
Clang raw-token comparison passes for all 2,202 changed files, excluding comments
and whitespace. This is not preprocessing or a complete native app build.

CI at the exact PR head:

- Formatter source build + 21 tests: 42347829629 — passed.
- Code Health: 42347829628 — passed.
- Tabs: 42347829651 — passed.

No app/console operations. No changes to the user's story-select worktree or
other agents' worktrees. The isolated `~/Projects/wt/main-format-preview` tree
contains the uncommitted candidate output only.

## Artifacts

Current config SHA-256:
`cba6e3f77fe68474a9b6040774645a59cac17d63def0ec4ceb3cf600a622b947`.

- `/tmp/main-uncrustify-preview.patch`: SHA-256
  `e3fe34b5ca1a87631e0fe01f0eae885434ccd6f8d320cbba505b09c4152e5e0d`.
- `/tmp/main-uncrustify-audit.json`: per-file output checks.
- `/tmp/main-uncrustify-token-audit.json`: 2,202 equal token streams, zero issues.
- `/tmp/main-uncrustify-numstat.tsv`: per-file line counts.
- `/tmp/main-uncrustify-before-tuning.patch`,
  `/tmp/main-uncrustify-before-tuning-audit.json`, and
  `/tmp/main-uncrustify-before-tuning-token-audit.json` preserve the earlier
  published trial. This supersedes its current-artifact paths in the first audit.

Review commands:

```sh
git -C ~/Projects/wt/main-format-preview diff --stat
git -C ~/Projects/wt/main-format-preview diff -- Common/Cpp/Containers/Pimpl.h
```

The Pimpl preview now leaves short functions and parameter packs alone; the
remaining lines demonstrate the unsupported leading-overload alignment. A full
bulk migration should not proceed until remaining unwanted changes are resolved
or explicitly accepted, and then tested on the exact formatted revision.
