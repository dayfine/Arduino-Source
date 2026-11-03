# MoveOnlyFunction test diagnostics: std::format

User requested applying the demonstrated `std::format` replacement for
`expected_int` after publishing upstream #1810.

Ready for Claude to apply to fork #72 and upstream #1810:

- Local branch: `common/move-only-format`.
- Commit: `ebee94847`, based on upstream PR head `95e3e6479`.
- File: `Common/Cpp/MoveOnlyFunction_Tests.cpp`.
- Adds `<format>` and replaces the concatenation with
  `std::format("{}: expected {}, got {}", what, expected, actual)`.
- Preserves CRLF and diagnostic text.
- Validation: full test translation unit passed
  `clang++ -std=c++23 -Werror -Wall -Wextra -Wno-unused-parameter -I . -fsyntax-only Common/Cpp/MoveOnlyFunction_Tests.cpp`.
- CRLF-aware `git diff --check` passed. No full build or runtime test rerun.
- PR branches were not modified or pushed, per ownership rules.
- Squash into the upstream commit to preserve the requested single-commit PR.

## Uploaded

The user subsequently requested “upload to 1810”, explicitly authorizing the push.
Squashed the change into `bda38f7b0` on our own `common/move-only-format` branch,
then pushed to `origin/upstream/move-only-function` with an explicit lease on
the previous PR head `95e3e64798885e842683c151afe7e86f93621c37`.
PR #1810 retains one commit. Compiler and CRLF-aware whitespace checks passed
again before pushing. Fork #72 was not changed.
