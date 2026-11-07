# Format edited lines only

Use this mode for routine development. It avoids the broad reindentation and
known declaration-spacing changes in the full-file formatter preview.

The tool needs Python 3, Uncrustify 0.83.0 and Clang (for a raw-token check).
It does not build or run SerialPrograms or connect a console.

From your code worktree, preview first:

```sh
python3 /path/to/tools/devtools/format/format_changed.py --repo .
```

While the tools PR is under review, the local checkout path is:

```sh
python3 ~/Projects/wt/incremental-cpp-format/devtools/format/format_changed.py --repo .
```

Then apply the reviewed proposal:

```sh
python3 ~/Projects/wt/incremental-cpp-format/devtools/format/format_changed.py --repo . --write
```

No directory change is necessary: `--repo ~/Projects/wt/bdsp-my-fix` selects
that checkout explicitly. The tool always uses the configuration beside its own
script, so the target branch need not merge the tools PR first.

## Scope and behavior

Default comparison is **HEAD against the working tree**, including staged and
unstaged edits. No commit is required. New files must first be tracked with
`git add`; untracked, deleted and renamed files are excluded. If a rename also
contains edits, commit the rename separately before using the default local mode.
Only `.h`, `.c`, `.cpp` and `.tpp` files in Common/SerialPrograms are considered.

For a whole branch, compare with its merge base with a chosen parent:

```sh
python3 /path/to/format_changed.py --repo . --base origin/main
```

Choose the actual parent for a stacked PR; do not compare a large story chain
with main when you only intend to format its top layer. The tool does not fetch.

- Runs Uncrustify on each selected file to obtain context, then accepts only
  same-line whitespace edits on lines Git marks added/modified.
- Keeps every original leading column, including manual declaration alignment,
  and every original newline. Untouched lines remain byte-identical.
- Skips formatter hunks that change line count, preprocessor directives and
  their continuations, and lines containing class/struct/union keywords. Those
  skips keep block reflows and known declaration-brace changes out of the patch.
- Checks the candidate with Clang's raw lexer before writing: significant token
  kind/spelling must match, including quoted/raw string contents. This does not
  preprocess includes or prove every macro-invocation behavior equivalent;
  review the displayed patch. Macro definitions are protected; invocations in
  ordinary code are not universally identifiable without preprocessing.
- Verifies all candidates first and refuses a stale source snapshot. It never
  stages files, commits, switches branches or changes the index. After --write,
  review and stage the formatting yourself.

This mode intentionally does **not** fix indentation, insert braces, expand
single-line control bodies, reflow blocks or assert complete style compliance.
Use existing lint/review for those. Skipped selected edits are reported on stderr;
stdout is the eligible patch. An unchanged result can mean the remaining
formatter suggestions were deliberately outside this mode's scope.

`--check` performs the same preview without writing and returns 1 if eligible
formatting remains; 0 means no eligible changes, and 2 indicates a tool, lexer,
version or source-snapshot error. Preview alone returns 0 without writing.
There is no automatic hook or source-format CI gate in this PR.

```sh
python3 /path/to/format_changed.py --repo . --check
```

Use `--uncrustify /path/to/uncrustify` / `--clang /path/to/clang` or environment
variables `UNCRUSTIFY` / `CLANG` for non-PATH installations.

## Tests

```sh
python3 -m unittest discover -s devtools/format -v
```

Tests call the real formatter and lexer, and use temporary Git repositories for
preview/check/write, staged-index preservation, explicit bases, unusual file
names, stale snapshots and refusal-before-write behavior. Application tests
remain native in their owning modules.
