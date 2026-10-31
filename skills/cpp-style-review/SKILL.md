---
name: cpp-style-review
description: Review a C++ PR diff in PokemonAutomation/Arduino-Source work (fork dayfine/Arduino-Source, also dayfine/PokemonNavigation) for judgement-level style and design issues a linter can't catch - sentinels, ownership and lifetimes, output and bool parameters, comment content, magic numbers, naming, globals, seams and testability, data over types, layering. Use when asked for a style review, a "would the user flag this" pass, or a pre-handoff readability check on a PR number or branch. Complements /code-review, which hunts correctness bugs.
---

# C++ style review

Finds what the user would flag in review, before they have to. It does **not**
hunt correctness bugs (that is `/code-review`) and does **not** re-check what
`devtools/code_health`, CI's clang-query, `-Werror` or the tabs job already
enforce. Run both skills on a PR that is about to be handed over.

Arguments: a PR number (fork `dayfine/Arduino-Source` unless `-R owner/repo`
names `dayfine/PokemonNavigation`) or a branch name, plus optional `--comment`.
Default output goes to the caller only.

## Step 1: Load the rules (every run, never from memory)

```sh
git -C ~/Projects/Arduino-Source fetch origin agents
git -C ~/Projects/Arduino-Source show origin/agents:AGENTS.md
```

Read its "C++ style" section and "Code conventions for PR branches". (Until
fork PR #93 merges, read `origin/notes/cpp-style:AGENTS.md` instead.) If the
fetch fails, say the rules may be stale. Then read
[references/user-review-themes.md](references/user-review-themes.md): the
user's own recurring comments. Upstream `AGENTS.md`/`CONTRIBUTING.md` and the
surrounding code outrank the draft.

## Step 2: Get the diff and its context (read-only)

```sh
gh pr view <N> -R dayfine/Arduino-Source --json title,headRefName,baseRefName,headRefOid,files
gh pr diff <N> -R dayfine/Arduino-Source
git -C ~/Projects/Arduino-Source fetch origin pull/<N>/head   # for context reads
git -C ~/Projects/Arduino-Source show FETCH_HEAD:<path>        # whole file at the tip
```

For a branch: `git diff origin/main...origin/<branch>` (or its parent branch for
a stacked PR; the PR's `baseRefName` says which). Never check out, commit or
push. Review **only added/changed lines** and the lines around them needed to
judge them (the enclosing function, the declaration in the header, the other
call site of the same helper). Legacy code the diff merely touches is out of
scope unless the change makes it worse.

## Step 3: Walk the checklist on each changed hunk

Review-only items from the draft rules, plus the user's themes (T# = theme
number in the references file):

1. **Sentinels** - `-1`, `""`, `0` meaning "none"/"failed" in a new API, or a
   caller testing one (`wait_until(...) >= 0` with no comment). Fix:
   `std::optional`, a result enum, or at least a named comparison (TotW #171,
   #163).
2. **Ownership and lifetimes** - views/references kept past the call, smart
   pointers passed without ownership transfer, `unique_ptr` for "maybe a value",
   reference/const members in copyable types (TotW #180, #188, #123, #177).
3. **Output parameters** - a non-const `T&` the function writes as a result,
   or state threaded through calls. Fix: return a struct/enum, or an object
   with methods (TotW #176; T4).
4. **Bool parameters** - new `bool` params, defaulted `bool`, two or more
   bools; bare `true`/`false` at a call site of an existing one. Fix:
   `enum class` or options struct; `/*name=*/false` (TotW #94).
5. **Comment content** - missing why on non-obvious code (T1); literals with
   no reason (T2); comments about this PR, our branches, lab runs or "same as
   before" (T3); function comments not directly above their function, missing
   start/end screen and failure behavior; commented-out code; banners.
6. **Naming fit** - repo naming (`m_`, `UPPER_SNAKE`, `snake_case`); a name
   that says what, not how; a module/file name that hides that it is
   game-specific or 2D-only (T9).
7. **Globals and knobs** - mutable namespace-scope state, `extern` flags set
   from options. Fix: pass options down in a struct (TotW #45, #103).
8. **Seams and testability** - new decision logic mixed with console waits and
   presses. Fix: inject the steps through a small interface with a scripted
   fake (`WildBattleExitActions`), seam plus tests in a base PR (T11).
9. **Data over types** - per-case structs, paired switches encoding one table,
   game data hard-coded instead of in the data file; or the reverse, a primitive
   that deserves a type (T7). Encoding leaking into a model's API (T8).
10. **Readability** - `std::exchange`, dense ternaries, two spellings of the
    same check (T5); positional args with no names (T6, TotW #172); deep
    nesting the change adds (T10).
11. **Placement** - single-use helper exported to a shared module, game code in
    shared code, include order (T9); forward declarations (strict ban).

## Step 4: Verify every finding before reporting it

For each candidate, re-read the code at the PR tip and confirm: the line is in
the diff (or the change made it worse); the rule really applies here (e.g. a
type with a constructor can't take designated initializers; an existing API's
`bool` gets a call-site comment, not a redesign); the suggested change compiles
in principle and matches surrounding style; it isn't already enforced by a
linter. If the PR already answered it in a review thread
(`gh api repos/<repo>/pulls/<N>/comments`), drop it or note the reply. Drop
anything you can't point to a line for. No speculative findings.

## Step 5: Report

Rank by severity, cap at about 10 findings; say how many minor ones you dropped.

- **blocking** - violates a hard repo rule or would be rejected upstream:
  forward declaration, mutable global, dangling view, game code in shared
  module, PR/branch talk in code comments, new sentinel API.
- **should-fix** - a theme the user has raised before (output param, bool
  params, unexplained magic number, missing why-comment, untestable new logic).
- **nit** - naming, wording, local readability.

```
## C++ style review - <PR title> (<repo>#<N> @ <short sha>)

### blocking
1. `<path>:<line>` - <what is wrong, one sentence>.
   Rule: <rule name> (TotW #<n> | user theme T<n>).
   Suggest: <concrete change, with a code snippet when short>.

### should-fix
...
### nit
...

Not checked here: correctness (run /code-review), code_health limits, CI.
```

Write in the user's plain review voice: a question or one sentence, no padding.

## Posting (only with `--comment`)

- Only on a **fork** PR (`dayfine/Arduino-Source`, `dayfine/PokemonNavigation`).
  **Never post, comment or review on any `PokemonAutomation/*` repo.**
  `~/.local/bin/gh` (first on PATH) guards upstream PR actions; never bypass it
  through Homebrew gh, `ALLOW_UPSTREAM_PR`, another client or direct API calls.
- Check the repo owner is `dayfine` before posting; otherwise stop and return
  the report.
- Post exactly **one** review comment (not approve/request-changes), the report
  body above: `gh pr review <N> -R dayfine/<repo> --comment --body-file <tmp>`.
  Then re-read the PR's reviews to confirm it landed.
