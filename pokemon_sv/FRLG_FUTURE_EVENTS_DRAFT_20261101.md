# FRLG future story events draft

User requested a new pseudocode/Markdown PR extending the AutoStory table
beyond Brock, using the current table and Bulbapedia's FRLG walkthrough.

- Draft fork PR: https://github.com/dayfine/Arduino-Source/pull/126
- Branch: `docs/frlg-future-story-events`
- Base: `frlg/autostory-mvp` (#120), `44742ac63567afbc1873303f0ea3dd9876380c98`
- Commit: `bf6ba7229` (one Markdown file, no executable changes).
- Own worktree used: `/Users/difan/Projects/Arduino-Source-party-override`.
- File: `SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryFutureEvents.md`.

92 proposed checkpoint/event/optional-branch rows cover post-Brock through
first Hall of Fame, first Sevii visit, National Dex/Ruby/Sapphire quest,
Mewtwo access, League rematch and optional walkthrough branches. Symbolic
anchors, unsupported operations and unvalidated behavior are explicit. No
invented coordinates or runnable enum additions. Part 21 is a red link in
the walkthrough index; event-island access is reserved for separate research.

Validation: unique row IDs, balanced fences, table endings, relative links,
and staged whitespace check passed. No build or console run for docs only.
Full 60-species collection and later battle/team policies remain future design
work, not claimed implementation. Existing base table still stops at Brock.
