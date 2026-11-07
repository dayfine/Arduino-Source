# Residual formatter churn and incremental adoption

User requested separating real inconsistencies from intrusive formatter rules,
and a safe way to start formatting incrementally. Config remains fork #244 at
`d8b038f93`; new stacked fork draft #245 adds the edited-line command, tests and
instructions at `6904e7962f47024a3d11913bbc416bec781fa66b`.

## Residual main diff classification

Same immutable main snapshot `2a692970b6ac4480f2af405d0adc964192df04e0`, config
SHA `cba6e3f77fe68474a9b6040774645a59cac17d63def0ec4ceb3cf600a622b947`.
Compare original and formatted files with SequenceMatcher; for equal-length
replacement blocks, classify each removed/added line pair. These figures are
line pairs, not total added/deleted lines, and do not classify unequal-length
hunks as individual lines. No claim of an exact good/bad percentage is made.

| Category | Matched line pairs | Interpretation |
|---|---:|---|
| Trailing whitespace only | 3,029 | Real low-impact cleanup |
| Only missing space after if/for/while/switch | 191 | Real documented style inconsistency; lower bound because mixed changes fall elsewhere |
| Added space before class/struct/union brace | 2,236 | Intrusive; disagrees with predominant compact-brace style |
| Leading indentation/alignment | 11,164 | Mixed; includes unwanted lambda dedentation and deliberate overload-alignment loss, as well as uneven legacy indentation |
| Other horizontal spacing | 4,824 | Mixed operator/pointer/comma spacing and combined edits; cannot label all as defects |
| Other rearrangement/mixed pairs | 54 | Requires individual review |

There are also 901 unequal-line-count replacement hunks. Those are not included
in the matched-pair counts. Full classification/examples are in
`/tmp/main-uncrustify-classification.json`; producer is
`/tmp/classify-format-diff.py`. The remaining full-file patch is not recommended
for bulk application. The source is not uniformly inconsistent: strong existing
brace/short-function conventions conflict with several formatter behaviors.

## Incremental command

`~/Projects/wt/incremental-cpp-format/devtools/format/format_changed.py` uses
Uncrustify 0.83.0 with that tools checkout's config and Clang's raw lexer.
It works against another checkout without merging tooling there.

```sh
python3 ~/Projects/wt/incremental-cpp-format/devtools/format/format_changed.py --repo /path/to/code
python3 ~/Projects/wt/incremental-cpp-format/devtools/format/format_changed.py --repo /path/to/code --write
```

Preview is the default. Only same-line whitespace edits on Git-added/modified
tracked C/C++ lines under Common/SerialPrograms are eligible. Keep all leading
columns and original newlines. Skip reflows, preprocessor directives and their
continuations, and lines containing class/struct/union keywords. Raw-token
kind/spelling comparison rejects identifier/operator merges and string/raw-string
changes. It does not preprocess includes or prove all macro-invocation semantics.
All candidates are verified before writing; stale source snapshots are refused.

No commits are needed for existing tracked edits. The index is never modified:
HEAD-to-working-tree comparison includes staged and unstaged changes. Untracked,
renamed and deleted files are excluded; new files require git add first.
`--base` uses the selected parent's merge base to include committed branch edits;
`--check` checks this narrow policy, not complete style compliance. No hook or
source-format gate is enabled. This intentionally does not fix indentation,
insert braces, or expand control blocks.

## Validation

35 tests pass locally: 21 inherited configuration checks and 14 new tests using
real Uncrustify/Clang plus temporary Git repositories. New cases verify line
scope, surrounding/leading alignment, CRLF, preprocessor protection, skipping
reflows/declarations, index preservation, explicit bases, unusual file names,
raw-string/identifier/operator protection, stale snapshots, and refusing every
write if a later candidate fails verification.

Read-only CLI smoke against the active Arduino checkout succeeded with zero
eligible changes and no writes. No app, console, save or device operations.
Configuration CI (source build + same tests): 42348880038.
Code Health: 42348880102. Tabs: 42348880054.

Full usage/scope is in the new PR's `devtools/format/INCREMENTAL.md`. Prefer this
explicit preview/write loop for initial use. Do not equate successful narrow
formatting with native compilation, full style compliance or semantic proof.
