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
- "should we return 0 expliclty?" (#61): implicit/fall-through returns.
