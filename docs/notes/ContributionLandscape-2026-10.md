# How PokemonAutomation/Arduino-Source takes contributions (October 2026)

A snapshot of how the upstream project reviews, tests and vests contributors,
and what that means for our work. Measured from git history on a fresh clone
of upstream `main` as of 2026-10-25. Review threads (who approved, comment
counts, time to merge) are not in git, so section 1 answers by proxy.

## 1. How reviews are done

The core doesn't review itself for most of its own work. In the last 12 months
there were 680 PR merges but ~1,400 direct pushes to `main` (Mysticial 873,
Gin 373). PRs are squash-merged; a "Merge pull request" commit is rare. So
review capacity is spent almost entirely on external contributors and on the
core's own larger refactors.

Core devs' PR descriptions are mostly auto-concatenated commit titles
(`fix build` × 5 is a real example, #1437). External contributors write real
descriptions; dayfine's are the most thorough in the recent log.

Latency is bimodal. Most PRs merge within a day of neighbouring PRs. A small
tail sits for months: #1101 (~75 days), #1287 (~114), #1319 (~82), #1063
(~167), all from non-core or semi-core accounts. CONTRIBUTING's "a week or
more" is honest about the median for outsiders and understates the tail.

## 2. Core vs. new contributors

- 56 authors all-time; 2,116 commits in the last year. Mysticial alone is 48%;
  the top three are 74%.
- Of 680 PR merges, **61 (9%) came from 16 people with fewer than 40 all-time
  commits.** Their median PR is 27 lines vs. 110 for core. Seven of the 61
  touched infra, all Linux platform fixes; none touched shared framework
  design.
- The October spike (139 merges, double any prior month) is largely
  `Pokemon Automation Dev Bot`: **51 PRs in 9 days, first appearance Oct 17.**
  It's an AI agent account the core runs, doing infra: the audio-pipeline
  rewrite, Qt removal, AGENTS.md, and CONTRIBUTING.md itself. So the
  maintainers have adopted exactly the throughput they're wary of from
  outsiders. The constraint was never AI; it's who verifies.

## 3. The vesting process

It was written down the weekend of Oct 24–25: CONTRIBUTING.md landed in #1700,
#1716 and #1720–1722. Rules: two open PRs for new contributors, drafts
included; the limit lifts when you're "a regular and trusted contributor";
pitch new programs in Discord first; infra gets "strict human review"; the AI
policy is understand-and-defend, no agent-opened PRs, short handwritten
descriptions, disclose the tool.

What's not written: how trust is measured, who decides, or who owns what.
There's no CODEOWNERS and no owner line in any program. Ownership is tribal
knowledge recoverable only from `git log` (FRLG: theAstroGoth 88 commits this
year, then Mysticial, Dalton, kichithewolf; SV AutoStory: jw098 117). Kuro's
"ownership + rot" model exists as a Discord statement, not in the repo.

## 4. Testing

- 391k source lines, ~16k test lines (4%). 129 `add_tests_` registrations; 92
  of 297 inference files have tests; ~1,000 fixture images in a separate
  2.3 GB repo.
- **42 of 47 test files were added in 2026**; the per-game test registries
  appeared in September during the `Move X` reorganisation. PRs touching tests
  went from 1/48 (Nov 2025) to 50/137 (Oct 2026).
- Nearly all of it is detector-vs-screenshot. Program *logic*, the flows that
  burn Switch hours, is essentially untested. The only fake-driven logic test
  in the entire repo is the `exit_wild_battle` one (#1663), and it merged in a
  day.
- CI: six platform builds with warnings-as-errors, the test binary, a tabs
  check, clang-query. No coverage, no flake tracking (one bot PR fixed a flaky
  `ListenerSet` test by hand).

So: not directionless, strongly trending up, but the coverage gap is precisely
the layer contributors' programs live in.

## 5. What's good long term, and what to move toward

The project's actual bottleneck is verification, not code volume; the bot
proves they'll merge 6 PRs a day when they trust the verifier. The
improvements that matter are the ones that let a reviewer read less:

1. **Make ownership legible.** A `CODEOWNERS` or an owner line per program
   directory turns kuro's rot policy into something enforceable (no owner →
   beta). This is a ~20-line doc PR that reifies something they already said,
   which is the easiest kind of change to accept. Pitch it in Discord per
   their own rule, and not as a first move.
2. **Make program logic reviewable without a Switch.** The seam from #1663
   (actions interface + scripted steps) is the pattern;
   `CodingAgentContext/AutomationProgramPatterns.md` is where it would become
   the house pattern. Propose it there after a few more instances exist, not
   before.
3. **Make the trust ladder explicit.** "Regular and trusted" could be "N
   merged PRs in an area, maintained through a release" → listed owner, limit
   lifts. Right now a newcomer can't tell what they're working toward, which
   is why the two-PR cap feels like a wall rather than a ramp.
4. **Make verification a PR artifact.** CONTRIBUTING already asks "say if you
   ran it on hardware"; a PR template with hardware-run and test checkboxes
   makes it the default instead of a courtesy.
5. **Core self-review is the gap nobody's naming.** 1,400 unreviewed direct
   pushes a year is how a two-maintainer project runs, and it's not something
   an outsider can fix. The realistic path is point 2: once program logic has
   fakes, the bot and the humans can both review it.

## For our work

Two merged PRs in three days, one of which introduced the only logic-test
pattern in the repo, is a better start than the design doc would have been.
theAstroGoth is the FRLG reviewer to build with. The AutoStory pilot to Brock
is a "new program" under their rules, so it goes to Discord first, and the
one-page version with the F3 go/no-go gate
(`docs/design/AutoStory-Proposal.md` on `design/autostory`) is what to bring.

PR numbers in this note are upstream
(`PokemonAutomation/Arduino-Source#N`).
