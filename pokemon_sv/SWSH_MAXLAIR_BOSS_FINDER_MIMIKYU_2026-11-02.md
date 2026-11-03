# Max Lair Boss Finder: Mimikyu misidentification and boss loss

User reported a Boss Finder crash after losing the final battle, leaving
three caught Pokemon rather than four. Candidate branch:
`swsh/maxlair-boss-identification`, commit
`6a8636142807439a739370f2c42f90ac9d8fa95a`, based on fork `origin/main`
`ddbfe7f2f`. Code worktree: `/Users/difan/Projects/Arduino-Source-maxlair-fix`.
[Draft fork PR #170](https://github.com/dayfine/Arduino-Source/pull/170).
No running application or console was modified.

## Evidence

Local log:
`~/Library/Application Support/SerialPrograms/SerialPrograms-20261101-153713815683.log`.
At **2026-11-02 06:15:50 PST**, line 778793:

```text
InternalProgramError: Unrecognized boss slug: mimikyu-disguised
Location: ...EndBattleDecider_BossFinder::get_filter(const std::string &) const
```

The preceding state reports three wins and initially records both opponent
and boss as `mimikyu-disguised`. Later reports correctly read opponent `kyurem`
while retaining boss `mimikyu-disguised`. The caught screen checks three
summaries, finds no shiny, then throws in the Boss Finder lookup. This is
consistent with the user's report of losing to the boss; three catches are
normal for that outcome. The originating incorrect visual read is not
reproduced here; a stale frame or wrong name read remains an inference.

Read-only inspection of the local Packages JSON confirms Mimikyu is in
`MaxLairRentals.json`, not `MaxLairBosses.json`; Kyurem is in the boss database.
Boss Action rows are built from the boss database, so adding Mimikyu to that
table would conceal the invalid state.

## Change

- Extract battle-stage validation/inference into
  `GlobalState::infer_boss_from_opponent()`: require a single rental before
  three wins and a single boss after three wins. Incompatible reads trigger
  the existing summary retry. A valid boss read repairs an invalid stored boss.
- Only a valid known boss may override the summary read.
- `GlobalState::caught_boss(count)` supplies a boss catch policy only for four
  catches and a valid boss slug. A three-catch loss passes an empty slug to
  Boss Finder's existing continue/take-non-boss-shiny branch. The encountered
  boss remains in state for the entrance's Save Path handling.
- Standard and Strong Boss deciders ignore this slug argument; their configured
  shiny/reset policies are unchanged. Their calls still execute after a loss.

## Verification

- A temporary native C++ replay linked against the production state source
  reproduces three failures with the previous inference/caught-boss behavior:
  accepting Mimikyu at the fourth battle, failing to repair it with Kyurem,
  and passing a boss catch policy with three catches. The candidate reports
  **0 regression failures**. This focused replay substitutes database membership
  (`kyurem` is a boss); the registered CI tests use the real resource database.
- Ten native unit cases registered in `PokemonSwSh_Tests.cpp`: rentals, invalid
  fourth-battle reads, repair, known-boss loss, boss catch, early boss, empty and
  ambiguous reads, preservation on rejection, and invalid boss with four catches.
- Apple Clang C++23 checks with `-Wall -Wextra -Werror` pass for state source,
  both changed program callers and the test translation unit. The replay links
  and runs; these caller checks are syntax checks, not a full local app build.
- Incremental code health: **0 failures**; linter's own 77 tests pass.
- Whitespace check passes with `cr-at-eol` (repository source uses CRLF).
- Full build/native tests **passed** on all three platforms at the candidate SHA:
  [Linux](https://github.com/dayfine/Arduino-Source/actions/runs/41501517131),
  [Mac](https://github.com/dayfine/Arduino-Source/actions/runs/41501520937),
  [Windows Clang](https://github.com/dayfine/Arduino-Source/actions/runs/41501525067).
- Linux: 32,456 passed; Mac: 20,344 passed; Windows Clang: 26,529 passed.
  Zero failures on all three.
  All ten new regression cases pass against the real database in CI.
- Fork PR code-health and no-tabs checks passed.
- No live Switch validation yet. A useful live check is a Boss Finder loss:
  three summaries must finish normally, preserve an optional non-boss shiny,
  return to the entrance and retain the correct boss identity for saving its path.

No upstream PR is authorized or created. Keep any new PR as a draft on the fork.
