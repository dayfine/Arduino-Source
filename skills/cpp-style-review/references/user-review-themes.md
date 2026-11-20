# What the user actually flags in review

Distilled from dayfine's own review comments on fork PRs dayfine/Arduino-Source
#59, #61, #67 and dayfine/PokemonNavigation #3, #5, #13. Quotes are verbatim
(typos kept). They are the bar: a finding that matches one of these themes is
one the user would have raised themselves.

## 1. Unexplained behavior: "what does this do?"

- "what does this really do?" / "what does this do? deserves a comment?" (#59)
- "comment on what this does?" (PN#5), "need class comment" (PN#3, twice)
- "example input / output, and how to use this (example CLI command)?" (PN#5)

Check: a non-obvious statement, a watcher/wait whose return value is tested, a
new class or module with no comment saying what it is for. Suggest the comment
text, not just "add a comment".

## 2. Magic numbers: "why this value?"

- "why are these values picked", "why 50ms - what is this meant to do?",
  "what's this 1ms? can we add a parameteric comment?" (#59)
- "why 3" (#67), "waht are these numbers?" (PN#5)

Check: a literal duration, box coordinate, count or threshold in new code.
Fix: a named constant with a why-comment (`ELEVATION_GROUND`), or an
argument-name comment at the call site (`/* default_video_period */ 1ms`,
`ImageFloatBox(/* x */ 0.1, ...)`). Reuse an existing constant if the same
value already exists in the file.

## 3. Comments that are about the PR, not the code

- "a comment is helpful but it should not be so contexted for just this PR"
  (#59): describe what the game/screen does, not why this PR changed it.
- "that's on our branch, so again should not be mentioned that way" (PN#3),
  "should not mention these" (PN#5)

Check: comments or docs that name our branches, lab runs, PR numbers, "the old
code", "kept identical to before", fixtures or notes. They belong in the PR
description or commit message.

## 4. Output parameters and hidden mutation

- "any way to make this `online_license` mut ref obvious? can we avoid using
  this output parameter but instead return this value?" (#59)

Fix: return a small struct (`GameLaunch{started, online_license}`) or a result
enum (TotW #176).

## 5. Clever one-liners that hurt reading

- "can we not std::exchange? I find it hard to understand?" (#59)
- "not very readable" (#67)
- "why doesn't this one just do wait_until(...) <0 like the previous call
  site?" (#59): two spellings of the same check in one file.

Check: `std::exchange`, comma/ternary chains, `>= 0` vs `< 0` for the same
test, embedded data written as code. Prefer the plain form used elsewhere.

## 6. Named fields: "initializer list with name"

- "can we use initializer list with name?" (#59)
- "what is the type of the second arugment?" (#59)

Fix: designated initializers for aggregates (TotW #172); argument-name
comments where the type has a constructor.

## 7. Data structures over types

- "should this be a map keyed by an enum? feels like it should not be a struct
  (type) but just a data strcuture" (#67)
- "should this be a enum / mapping (data structure instead of type)?",
  "there should be a two way mapping replacing the swtich here and below?
  static constexpr map?" (PN#3)
- "we should define a type for this" (#67): the reverse, for a raw
  `std::vector<std::string>` grid used as a model.
- "should these be part of the map data?" (#67): game data in code.

Check: a struct with one field per case, or paired switches that encode the
same table both ways; game coordinates/flags hard-coded instead of living in
the data file; a primitive carrying a domain meaning with no type.

## 8. Encoding separate from the model

- "imo we should not expose the character used -- that's encoding and it's a
  separate concern" (PN#3)
- "the encoding might change, e.g. if we want to mark the facing ..." (PN#3)
- "can we make these more readable somehow? ... like a structured type /
  schema in json" (PN#5)

Check: a model type whose public API exposes file characters, string codes or
serialization details.

## 9. Placement and layering

- "This is in the wrong place", "should PokemonFRLG_RngProgram not be sorted?"
  (#61, include order)
- "why is not just in ..._RngProgram.cpp?" (#61): a helper with one caller
  belongs next to it, not in a shared module.
- "how game specific is this?", "I feel like this should be in a different
  module" (PN#3), "this is game specific, right? should be structured as so",
  "this probably doesn't belong here?" (PN#5), "make it clear this is 2D
  movement" (PN#13)

Check: game-specific code in shared code (or a shared module named as if
general), single-use helpers exported, includes out of IncludeOrder.md order.

## 10. Size and nesting

- "too long and too nested" (PN#3), "nested-ness" (PN#5)

`devtools/code_health` measures the limits; flag it in review only when the
change makes a function noticeably harder to follow, and suggest the split.

## 11. Testability through seams

- "is this possible to unit test this file?", "can we introduce it in a
  separate PR as base and test some of the existing behavior?" (#59)

Check: new decision logic tangled with console I/O. Suggest the seam (inject
the game steps through a small interface, as `WildBattleExitActions`), and
that the seam plus tests of existing behavior go in a base PR.

## 12. Small API hygiene

- "maybe const std::string&?" (PN#3): parameter passing.
  Since 2026-11-02 the user wants `std::string_view` by value for read-only
  string parameters and literal-id loops (AGENTS.md "Strings"); flag
  `const std::string&` there.
- "should we return 0 expliclty?" (#61): implicit/fall-through returns.

---

Themes 13-26 below: distilled 2026-11-19 from the user's ~195 inline comments on
fork PRs #45, #61, #107, #152, #165, #171, #172, #207, #293, #312, #320, #325,
#335, #357-#359, #484, #572, #585, #590, #644, #648, #659 (Nov 2026). Most
frequent first. Apply them to code the user has NOT looked at yet: they said
"i haven't looked at all files in 644, not to mention other PRs".

## 13. Justify every piece: "do we really need this?"  (most frequent)

- "is this actually reusable in some sense?" (x5, #644), "is this really
  shareble? there is very little going on" (#648), "do we really need these
  helpers?" (#312), "do we need this? I hope not?" (#61), "is this useful?",
  "is this necessary?", "delete" / "remove" (#107, #335, #648)
- "how is this different from X?": StoryStepJson vs AuthoredCheckpoint,
  ZonePose vs DeclaredPose, ScreenObservation vs ScreenEvents, StoryStep vs
  checkpoint, `variant` vs args, `x` vs `attempts_per_checkpoint == 1` (#312, #648)
- "why both `failed_name` & `failure`?", "if failed this == failed_event;
  otherwise == events size" (#648): derived/duplicate fields
- "is it possible to have `completed` with failure, or incomplete with no
  failure?" (#648): make invalid states unrepresentable

Check: single-use helper or file -> inline it next to its caller. Two types or
files for one concept -> merge. A field derivable from another -> drop. A
config knob nobody varies -> constant. A bool + optional pair that can
disagree -> one optional/variant.

## 14. Domain vocabulary, one name per concept

- "row -> checkpoints", "rows -> checkpoint", "Do not use Segment" (#644);
  "we don't necessarily care about segments ... the prefix is just grouping" (#648)
- "what's authored really means? can we drop it from the name?" (#648),
  "do we really need Pokemon in front of each file name?" (#312)
- "poor name, context?" (#644), "`make` is a terrible name" (#335),
  "what is `slug`, really?" (#172), "in_place -> attempts_so_far?",
  "head -> prefix?" (#45), "expected_game -> game", "FLAG -> BOOLEAN?",
  "StoryArg or StoryActionArg?", "StoryCheckLevel?", "CheckpointTestingStatus?",
  "CheckpointRunResult?", "failed_event_index?" (#312, #648)
- "name `wins == 3` to be `only_lost_to_boss`" (#172): name conditions

Check: AutoStory words are checkpoint, event/step, action, arg, backend,
screen. No row, segment, authored, slug, make, head. A name says its unit/
domain (`_index`, `_ms`, `Level`, `Status`). Enum values read as a scale
(TESTING THOROUGH DEFAULT FAST SPEEDRUN).

## 15. Types, not strings: enums, constants, EnumStringMap

- "we should have constants for all these, or types (enums)" (#644),
  "shuld this be enum?" (#644 MenuReadings), "should we use some EnumStringMap
  here?" (#644), "is there some magical way to derive names out of enum
  class?", "we should just define a static constexpr map" (#648),
  "should this be typed?" (#648 failure codes)
- "prefer returning string ... ban char* / char[] in new code in favor of
  std::string / string_view" (#312), "should this be a string_view?" (#572),
  "std::string() -> \"\"" (#172)

Check: string literals compared or switched on (action names, menu
readings, failure codes, screen names) -> enum + `EnumStringMap`, or named
constants. Enum<->name tables: one `EnumStringMap`, not a switch per
direction. Read-only string params: `std::string_view`. No `char*`.

## 16. MoveOnlyFunction and function interfaces

- "Moveonlyfn througuout if possible", "moveonlyfn + make some aliases?"
  (#644), "Could this be a function interface? MoveOnlyFunction?",
  "moveonly fn?" (#648 x3)

Check: `std::function` in new code -> `MoveOnlyFunction` (Common/Cpp), with a
named alias for each signature. A one-method interface class -> a function.

## 17. Directory layout: one place per kind

- "make a detectors/inferences subdirectory, and one file per detector",
  "under detectors", "put these under actions?" (#335), "imo these also
  belongs to a file under actions", "we should have a file that just collects
  all the actions" (#644)
- "should this be under Data? ... Json modules moved under a subdirectory",
  "is this just I/O?", "moved under `Types`" (#312), "put this under a testing
  subdirectory" (#648), "AutoStory/Options?" (#572)
- "separate IO from domain logic" (#335, "as with other PRs")
- "add_tests_AutoStoryTests header ... to avoid keep editing this file" (#572),
  "these should have their own tests under AutoStory?" (#165), "this
  intrusion means we should add a new test file" (#171)

Check: Detectors/ (one detector per file), Actions/ (plus one registry file
that lists every action), Data/ (JSON/file I/O only), Types/, Testing/
(fakes), Options/. Parsing never mixed with domain logic. A module registers
its own tests through one `add_tests_*` header.

## 18. Headers: small surface, public first

- "too many symbols in header (please add a style guide / lint rules)" (#312),
  "why exposing this here?", "do we need 2 public methods, why?" (#648),
  "does this need to be public?", "public twice?" (#572), "hide" (#590)
- "no `extern` please" (#648), "expose a factory in header, implement in .cc"
  (#648), "should this be included in the .cc file? then we can keep the
  header unchanged" (#325)
- "declare these first", "define the internal templates after the public
  `run_checkpoints`" (#648); "put this unnamed namespace on the top" (#207, #171)
- "do not define long methods in line, define them separately. and consider
  if they really have to be methods" (#644)
- "a bit concerned that the PoseType template is leaking everywhere" (#648)

Check: header = the few things callers use. Helpers -> unnamed namespace at
the top of the .cpp. Methods that use no state -> free functions. No method
bodies longer than a few lines inside the class. Templates/type params don't
spread through unrelated APIs.

## 19. Group state: "tooooo many members"

- "tooooooooooooooo many members and it makes no sense. why? abstract please
  -- map? vector? struct?" (#644 SceneMachine)

Check: a class with many parallel members/flags -> group them into structs,
a table (vector/map of rows), or per-phase objects.

## 20. Comments: fewer, shorter, right

- "redundant comment when we name this correctly", "much of this is too much
  details that should be deleted", "all comments after this can be dropped"
  (#648), "redundant comment" (#572 x2)
- "this comment is bad -- it should just ...", "comment is bad", "this
  comment is hard to understand" (#572, #648), "return nullopt means success?"
  (#648): say the contract (what nullopt means), not the history
- Still wants a why for real puzzles: "add a comment saying that this is a bug
  and we gonna fix it next" (#171), "`!=` is always hard to understand,
  please comment" (#172), "explain the type of files in comment" (#312)

Check: a comment that restates the name -> delete. Header comments: one or
two lines of contract (inputs, return meaning, failure). No essays.

## 21. Call sites and signatures

- Literal args: "parameter comment" (#45, #572 x4), "any constants values in
  a function that takes more than 1 or 2 parameters should have parameter
  comments" (#590) -> `/*name=*/0`
- "prefer default arguments over overloads (style guide + lint)" (#312),
  "can we combine both overloads with default optional events?" (#312, #45)
- "if using output parameter, put it last" (#45), "put different parameters
  on a new line" (#572)
- "how long is this? in seconds if possible" (#644): durations as
  `Seconds`/`Milliseconds`, in the unit a reader thinks in
- "should this return a vector, one for each error?" (#312)

## 22. Initializer lists and named fields

- "can we use initializerlist here instead of emplace?" (#644), "intialize
  list this whole thing" (#590 x2), "we should use field names in these" (#359),
  "make this a struct so cases[] can have field names" (#171)
- "`= {};` is redundant? explicit initialization should only be beneficial
  for primitives?" (#648; we keep `= {}` only for Windows clang-cl designated
  init, see memory windows-clang-designated-init)

Check: a table built by repeated `emplace`/`push_back` -> one braced
initializer. Positional aggregates -> designated fields.

## 23. Tests

- Shape: "define Input and Expectation inside `StoryLogTestCase`" (#45),
  `BossStateTestCaseInput`/`Expectation` pair (#171); see memory test-case-shape
- "drop this unit test for this one" (#590), "delete" (#648 StoryFiles_Tests):
  no tests of trivial code
- "is this the case that should fail? add a comment saying that this is a bug"
  (#171)

## 24. PR scope: the change goes where it belongs

- "this change should be part of #45", "the log check here should be made in
  #45" (#325), "a prework PR that should go with #45" (#312), "644 have many
  changes that should go into its parent" (2026-11-19)
- "maybe we can get this in a separate PR / module? ... Gen8ToGen9Languages"
  (#312), "is this part merged upstream?" (#312)

Check: a shared-file edit in a child PR -> move to the PR that owns the file
(memory pr-owns-its-files). Generic pieces get their own small PR/module with
a specific, correct name.

## 25. Prefer library idioms

- "is there a span / view type in C++ 23" (#207), "is there a StrJoin
  function that avoid repeated append?" (#325), "std::format ... lint against
  string concat using +" (#45), "strformat + raw string?" (#61), "multi line
  [raw string] to make it readable" (#648)
- "consistent way of deriving the debugprinting ... traits as in OCaml / Rust,
  same for equality" (#648): one pattern for print/== across types
- "check google C++ style / TotW" (#207)

## 26. Formatting nits the linter doesn't catch

- Blank line above/below functions (#207, #45, #171), `} // namespace` (#207),
  `if (auto e = ...; !e.empty())` init-statements (#320)

## Lint candidates the user asked for

char*/char[] in new code; overloads where a default arg works; header symbol
count; string `+` concat; output param not last; literal args without
`/*name=*/` (exists: param_comment). Raise these with the code_health owner.
