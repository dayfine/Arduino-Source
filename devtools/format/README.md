# C++ formatting

Use **Uncrustify 0.83.0** with the root `.uncrustify.cfg`. This replaces the
initial clang-format proposal: Uncrustify can produce the repository's compact
`if (condition){`, `}else{` and `namespace Name{` spelling directly.
No regex postprocessor or change to AGENTS.md's style rules is needed.

## Install and preview

On macOS:

```sh
brew install uncrustify
uncrustify --version
```

Check for `Uncrustify-0.83.0` (Homebrew's release appends `_f`). CI builds that
release from a SHA-256-verified source archive. If your package manager supplies
a different version, install the pinned release before comparing formatter output.

From the repo root, preview a specific file without changing it:

```sh
uncrustify -q -c .uncrustify.cfg -l CPP \
  -f SerialPrograms/Source/CommonFramework/Main.cpp -o /tmp/Main.formatted.cpp
git diff --no-index -- SerialPrograms/Source/CommonFramework/Main.cpp /tmp/Main.formatted.cpp
```

A diff exits 1 when changes are present; that is expected. Apply to an explicit
file only after reviewing the preview:

```sh
uncrustify -q -c .uncrustify.cfg -l CPP --replace --no-backup \
  SerialPrograms/Source/CommonFramework/Main.cpp
```

To check an explicit file without writing it:

```sh
uncrustify -q -c .uncrustify.cfg -l CPP --check \
  SerialPrograms/Source/CommonFramework/Main.cpp
```

Format edited files in a separate commit from behavioral changes. Preserve
unrelated changes and do not format vendored or generated files. The repository
contains legacy formatting differences; adopting the configuration does not
require reformatting all existing sources. This PR's CI tests the configuration,
not source-wide compliance. Do not enable a broad format-on-save hook yet.

## What this configuration covers

- Four spaces, no indentation tabs, unindented namespaces/access labels and
  type-attached pointers/references.
- Compact control/namespace braces and spaced control parentheses. Existing
  standalone class brace spacing and nested-template `>>` spelling are preserved. Existing braced
  single-line `if`/`for`/`while` bodies expand onto multiple lines.
- Existing LF or CRLF style, include order (including own-header-last and
  registration-order exceptions), comment text and unlimited line width.
- No brace insertion/removal, statement removal, include sorting or generated
  namespace comments. Missing-brace and namespace/API documentation requirements
  remain the responsibility of review and existing code-health checks.

Formatting still changes indentation, spacing and some line breaks. C++ macros,
conditional compilation and newer syntax need review; a successful formatter
exit does not prove semantic equivalence or a successful native build. Main-tree
trials are performed in an isolated snapshot and reviewed before any formatting
commit is proposed. Known limitations are recorded in the fork PR/run record:

- Class/struct brace spacing is not fully enforced: standalone declarations
  retain their existing spacing, and inherited declarations can gain a space
  before `{`, for example
  `struct Derived : public Base {`. Uncrustify 0.83.0 does not expose a separate
  configurable spacing rule for this case. Restore the repository's compact
  spelling when reviewing output; this formatter is not a complete code-health
  compliance check.
- Function-definition brace line breaks are preserved to avoid moving a
  constructor's brace across comments or commented-out initializer entries.
- Missing braces stay missing. Formatting is not a substitute for the existing
  multiline-control-flow/braces lint rules.

## Configuration tests

These are tooling tests, not application tests. They call the real formatter:

```sh
python3 -m unittest discover -s devtools/format -v
```

For a non-PATH installation:

```sh
UNCRUSTIFY=/path/to/uncrustify python3 -m unittest discover -s devtools/format -v
```

Coverage includes exact brace spelling, indentation, pointers, includes,
line-ending preservation, comment/long-line preservation, templates/designated
initializers, raw strings, macro continuations/token pasting, constructor-comment placement and idempotence.
Regressions also preserve conversion-operator tokens and expose the
inherited-class spacing gap explicitly.

Reference: [Uncrustify 0.83.0 options](https://github.com/uncrustify/uncrustify/blob/uncrustify-0.83.0/documentation/htdocs/default.cfg).
