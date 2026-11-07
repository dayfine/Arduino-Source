# Formatter configuration proposal

User requested a standalone new PR. Fork draft #244, branch
`tooling/clang-format-config`, commit `96dff99d8`, base fork main. Isolated worktree
`~/Projects/wt/clang-format-config`; existing Arduino checkout's unrelated
Pokemon_AutoStorySelect.h edit was left untouched.

Four files: root `.clang-format`, configuration tests/README under
`devtools/format`, and configuration-only CI. No C++ changes or automatic
source-format gate. Pinned/tested clang-format 21.1.8; six checks pass locally.
Actual non-mutating previews on UnitTest.h, Main.cpp and MaxLair Run_Battle.cpp
preserved includes and newline style and were idempotent.

Important review blocker: LLVM 21 cannot express compact brace spacing
(`if (...){`, `}else{`, `namespace Name{`). A minimal real formatter invocation
confirmed it inserts spaces there. README/config/PR all disclose the mismatch;
this does not authorize changing style rules or enabling format-on-save.
A tested compatibility tool or explicit style decision is required for exact
automatic adoption. No regex postprocessing added. Full app builds unnecessary.

CI configuration run: 42344154447 (pending at creation). Code health and tabs
also started. No upstream PR, comments to others, console or GUI operations.
