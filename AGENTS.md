# AGENTS.md

Instructions for AI coding agents working on this repository. People should read
[CONTRIBUTIONS.md](CONTRIBUTIONS.md); everything there applies to agents too.

## Before you start

- Read [CodingAgentContext/ProjectOverview.md](CodingAgentContext/ProjectOverview.md) and
  [CodingAgentContext/AutomationProgramPatterns.md](CodingAgentContext/AutomationProgramPatterns.md).
- Look at a neighboring program or detector and follow its structure, naming and comment style.
- Programs drive a real game through a controller. Many changes can only be confirmed on a
  Switch; a unit test that passes doesn't show that a program works. Say what was and wasn't
  checked on hardware.

## Build

- CMake + Qt6, C++23. CMake clones the `Packages` repo (resources, firmware) next to this one
  if it isn't there.
- Warnings are errors on every CI build: `-Wall -Wextra -Wpedantic -Werror` with gcc/clang, and
  `/W4 /WX` with MSVC and clang-cl. The Windows clang-cl build warns about things macOS and Linux
  don't, e.g. `-Wmissing-designated-field-initializers`: give struct fields used with designated
  initializers a default (`= {}`).
- New `.cpp`/`.h` files must be added to `SerialPrograms/cmake/SourceFiles.cmake`.

## Source files

- C++ files use CRLF line endings and the repo stores them as-is (`.gitattributes` is `* -text`).
  Keep CRLF when editing; don't let a tool rewrite a whole file with LF.
- No tabs in `Common/` and `SerialPrograms/` (`.h .c .cpp .tpp`); CI checks this.
- Keep the module dependency direction one-way. Include the real header rather than forward
  declaring across modules.

## Tests

- Run all unit tests without the GUI: `SerialPrograms --command-line-test-mode` (exit code 0
  means all passed). From the GUI: the "Unit Test Runner" program, which can filter by name.
- Image-based tests load files from the
  [CommandLineTests](https://github.com/PokemonAutomation/CommandLineTests) repo, checked out
  next to this one. Some need downloaded resources (e.g. `PaddleOCRDetection`).
- A test is a class deriving `UnitTest` (`Common/Cpp/TestRunners/UnitTest.h`), registered in its
  module's `add_tests(UnitTestDatabase&)`. See `SerialPrograms/Source/Tests/CommandLineTests.h`.
- Add tests for logic you change when it can be tested without a console, but keep them in
  proportion to the change.

## Pull requests

- Build and run the tests yourself before opening a PR. Don't open PRs or reply to reviews on
  someone's behalf without them reading it first.
- One fix or feature per PR. Put pure moves and refactors in their own PR, separate from behavior
  changes.
- Keep the description short and written or edited by the author: what was wrong, what changed,
  how it was tested (including on a Switch). Say that an AI tool helped.
- New contributors: at most two open PRs, drafts included (see CONTRIBUTIONS.md).
