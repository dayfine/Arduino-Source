# C++ formatter proposal

The root `.clang-format` is an opt-in draft, tested with **clang-format 21.1.8**.
No source files are reformatted and no source-format CI gate is enabled.

**Known incompatibility:** stock clang-format emits `if (condition) {`,
`} else {`, `namespace Name {` and `class Name {`. The repository requires
compact braces such as `if (condition){` and `}else{`. LLVM 21 has no style
option to express that spacing. This proposal does not change AGENTS.md's
rules: do not enable format-on-save or apply its output without reviewing and
restoring the required brace spacing. Exact automatic formatting needs a
separate decision: a tested compatibility tool, or an explicitly approved
style change. No regex postprocessor or silent style change is included here.

The configuration supplies four-space indentation, no tabs, type-attached
pointers/references, unindented namespaces and access labels, attached braces
and multiline control flow. It preserves include order (including the own-header
last rule and registration-order exceptions), existing LF/CRLF style, and comment
text. There is no forced column limit. It does not insert/remove braces or add
namespace comments; the existing code-health checks still cover those contracts.
It can change other whitespace and wrapping, so review every proposed diff.

Install the pinned tool in an isolated environment, for example:

```sh
uv tool install 'clang-format==21.1.8'
clang-format --version
```

From the repo root, preview a specific file without modifying it:

```sh
clang-format --style=file SerialPrograms/Source/CommonFramework/Main.cpp > /tmp/Main.formatted.cpp
git diff --no-index -- SerialPrograms/Source/CommonFramework/Main.cpp /tmp/Main.formatted.cpp
```

A diff exits 1 when changes are present; that is expected. Use `--lines=START:END`
for an edited region, while reviewing neighboring changes clang-format may make.
Do not bulk-format the repo or third-party files. `--dry-run --Werror` is not a
repository-style validator while the known brace-spacing incompatibility exists.

Run the configuration checks (not application tests):

```sh
python3 -m unittest discover -s devtools/format -v
```

Without a persistent install, the equivalent is:

```sh
CLANG_FORMAT='uv tool run --from clang-format==21.1.8 clang-format' \
  python3 -m unittest discover -s devtools/format -v
```

Tests run the real formatter on small C++ examples, check line endings, includes,
indentation, comments and idempotence, and explicitly demonstrate the brace gap.
They do not assert the existing source tree is already formatted.

Reference: [LLVM 21 style options](https://releases.llvm.org/21.1.0/tools/clang/docs/ClangFormatStyleOptions.html).
