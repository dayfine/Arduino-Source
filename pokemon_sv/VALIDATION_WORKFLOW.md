# BBQ milestone implementation and live validation loop

Status: operational workflow documented; isolated testing branch created.
Its integrated baseline has not yet been built, tested or run. This is a
workflow document, not an installed agent skill.

Track current implementation, integration, live evidence and PR readiness in
[MILESTONE_STATE.md](MILESTONE_STATE.md); update it after every loop transition.

## Branches and ownership

- Testing branch: `bbq/validation-stack` on dayfine/Arduino-Source.
- Isolated worktree: `/Users/difan/Projects/Arduino-Source-bbq-lab`.
- Planning/evidence branch: `bbq/m7-board-read-notes`; milestone notes, daily
  audits, BP ledgers and this workflow live under `tests/pokemon_sv/`.
- Release branches: one branch per feature/PR. Never open a feature PR directly
  from the testing branch, which accumulates unrelated experimental changes.
- **Codex owns** the testing branch, notes branch, implementation/live validation
  loop and run records (`RESULT.md`).
- **Claude owns** existing PR branches and descriptions: refining code, rebasing
  as parents merge, answering review comments and incorporating VERIFIED results.
  Codex fetches these revisions into the lab; it does not push to existing PR
  branches. Changes for an existing PR are handed over as lab commits plus their
  RESULT.md, rather than pushed over Claude's work.
- For a new feature, Codex may create the dedicated release branch and open its
  draft PR. Ownership then passes to Claude for further PR changes and review.

The initial lab baseline combines upstream main `db9ef1772` with M6
`a1b685f9e` (including M4 and its prerequisites), then cherry-picks M5
`f7e3ba6f0` as lab commit `075376f2f`. The integration merge is `9d5d232fe`.
`tests/pokemon_sv/lab/STACK.md` in the lab branch records its components and
verification status. Record exact SHAs, not only moving branch names.

Current dependency map, checked on GitHub 2026-10-21:

| Change | PR | Required ancestors |
| --- | --- | --- |
| Injectable farmer rounds | #1632 | None |
| Quest planner | #1640 | None |
| Persistent failed-quest state (M4) | #1642 | #1632 |
| Verified reroll | #1659 | #1640 |
| Unsupported-quest policies (M5) | #1644 | #1659, therefore #1640 |
| Exhausted-quest actions (M6) | #1645 | #1642 and #1659, including their ancestors |
| Battle-entry guard (M3) | #1636 | None |
| Endless-reroll guard | #1639 | #1632 |
| Save/reset milestones | #1626 | #1632 |

Before the next M7 lab build, also integrate **#1636, #1639 and #1626**. The initial lab
baseline described above does not yet contain them. #1636 is independent and
guards both Tera routes; #1639 and #1626 change the round loop exercised by every
run. Include them to collect relevant evidence and approximate the intended
merged program. Record their exact imported SHAs in STACK.md and build/test the
combined baseline; integration alone is not live verification.

The user clarified that M4–M6 are done for the current milestone sequence.
Do not add a separate V0 campaign to repeat them. The next feature is **M7a**,
then **M8a**. Baseline integration/build/regression checks are preparation for
M7's run; existing PR coverage paths below remain reference material for Claude
and for regressions naturally encountered during later trials.

These are existing implementations to reuse, not recreate. Other diagnostic or
sibling PRs are not automatically included. Recheck GitHub heads before each
run and extraction, since rebases move them. Heads checked on 2026-10-21:
#1645 `a1b685f9e`, #1644 `f7e3ba6f0`, #1659 `5b86beecb`, #1636 `07f03d2fe`,
#1639 `f62d994dd`, #1626 `548ac951c`. Historical run records retain their original
SHAs even after the PR heads move.

## Repeat this loop for one candidate at a time

1. **Select and specify.** Take the highest-ranked eligible queue item below.
   Record its hypothesis, exact acceptance criteria, prerequisites, observed
   baseline and anticipated BP/time benefit. Split large milestones into slices
   such as M8a (party restoration) and M8b (startup egg-box selection).
2. **Implement one slice.** Add a feature commit on the testing branch, with
   native C++ tests exercising the production seam. Keep necessary follow-up
   corrections as adjacent commits belonging to that slice. Do not add another
   independent feature until the current candidate has an evaluated result.
   Existing implemented PRs can enter as verification-only candidates. Follow
   the repository conventions below from the first commit, rather than leaving
   style/module cleanup for PR handoff.
3. **Build and test.** Run relevant registered native C++ tests and build the
   full local arm64 ComputerControl app. No Python/source-extraction tests.
   Store build/test logs and the executable's SHA-256 beside the run record.
   Pin the candidate SHA; no source/binary/configuration changes mid-run.
4. **Prepare the console.** Use a dedicated `bbq-lab` profile and its own stats
   file, never the user's farming profile. Snapshot settings without credentials.
   Confirm controller ownership and correct camera/audio. Stop the previous
   program normally before taking control; when the user is using Flying Trial,
   preparation/build work can proceed but live control waits for handoff.
   Open the specific locally built app and verify its executable/profile path.
5. **Establish starting state.** Reach verified overworld, return/fly to Central
   Plaza, and confirm position. Record lead identity/level/HP, relevant party/egg
   state, complete quest board/progress and visible starting BP. Until M8a is
   fixed, disable hatch attempts in unrelated trials. Do not enable wild-Tera
   trials without the required battle-entry guard. Choose policies and enough
   quest capacity for the timed run; capture their actual active values.
   Validate edited JSON enum slugs against the option's actual dropdown database,
   then check the loaded UI. Unknown slugs can silently retain a row's default.
   Hatch is intentionally absent from Quest Exclusions: disable hatch attempts
   through NUM_EGGS and OUT_OF_EGGS, never by injecting a hatch-egg policy row.
   Snapshot disk settings and loaded values separately when they differ.
6. **Run for 60 minutes.** Record actual start and a deadline. Extend to at most
   120 minutes only if conditions remain useful and more exposure can resolve a
   specific uncertainty. Start a separate deadline observer that survives the
   sampling turn and checks the exact app/profile/run identity. A written plan
   alone is not an armed timer. Never let a broad timer stop a different program.
   Determine current state from the latest state transition, not whether STOPPED
   ever appeared. Pin the run's start event as well as PID/path/profile; a manual
   restart invalidates the old timer and starts a separate evidence record.
7. **Observe every five minutes.** Read logs and capture relevant frames. Check
   the feature trigger/outcome, completions, failed attempts, recovery duration,
   rerolls, expected income/expense/net BP and signs of new issues. Record
   observations with timestamps. On a long-running agent turn, use short waits
   so steering remains possible. Set the next check explicitly; do not claim to
   monitor after ending a turn without a persistent observer.
8. **Stop early when continuing has no value.** Stop normally on party corruption,
   repeated identical no-progress behavior, unresolved UI/location state,
   uncertain payment, or an already-demonstrated regression. A 15-minute window
   without completion triggers investigation; it is not an automatic failure
   when a known long action is legitimately progressing. Bound total duration
   and record the stop reason. A crash or early stop is retained as an outcome,
   not discarded from velocity statistics.
9. **Audit and decide.** Stop via the app's normal Stop action, confirm stopped,
   capture final board/party/BP and preserve the scoped log. Reconcile expected
   rewards and reroll spending against the visible balance delta. Report both
   the target feature's result and unrelated findings. Classify as VERIFIED,
   FAILED, or INCONCLUSIVE; a feature whose trigger never occurred remains
   inconclusive even after two hours of successful farming.
10. **Handoff or publish.** For an existing PR, hand Claude the lab commits and
    RESULT.md; Claude updates its branch and short verification section. For a
    new feature, extract a dedicated branch as described below, build/test it,
    open its draft PR, then hand ownership to Claude. Record the exact tested
    revision, native cases, live evidence and dependencies. Update the queue
    and stack manifest, then select the next candidate.

## Repository conventions for every feature commit

- Native C++ tests are registered in `PokemonSV_Tests.cpp`. Use data-driven
  scenarios and designated initializers so values have field names; avoid
  if/else-if chains selecting individual test cases. Failure messages report
  what was expected and what was actually read.
- Match surrounding comment density and explain why. Use `virtual ... override`,
  nested namespace braces and CRLF line endings in source files.
- Include the real header instead of adding forward declarations. Declare and
  define a function in the same owning module; module dependencies point one
  way. Do not declare it in one module's header and define it in another.
- Use `UserSetupError` for setup/configuration problems. `SEND_ERROR_REPORT` is
  for program failures. Normal stop decisions return a result to the farmer;
  do not throw deep in quest code just to end the program.
- Reuse prerequisite implementations such as `run_bbq_rounds()`/`BBQGameActions`
  from #1632 and `reroll_quest()` from #1659. Do not copy them into a second PR.
- No files under `tests/` belong in a release branch's change: milestone notes,
  lab manifests, RESULT.md, fixtures and Python stay on notes/lab branches.
  Native tests under `SerialPrograms/Source/.../PokemonSV_Tests.cpp` are release
  code and remain with their feature.
- Defaults preserve today's behavior. Put an intentional behavior change in its
  own PR and make it explicit in the title.

## Verification is specific to the feature

- M4/M6: induce a real failed quest, continue through later batches, demonstrate
  retained exhaustion and the chosen Skip/Reroll/Stop behavior. Record paid
  target/replacement for Reroll. Ordinary first-attempt successes do not verify it.
- M8a: native injection of hatch failure after the outbound swap must restore
  the original party; a bounded live trial verifies actual swap/postcondition
  detection. Do not deliberately corrupt the user's party or wait ten minutes
  merely to manufacture a timeout. Cancellation/restart recovery needs its own
  evidence. A normal hatch alone cannot verify exception recovery.
- Route fixes: observe the problematic target/obstruction/no-progress condition,
  capture recovery or bounded failure, and verify a successful normal path.
- Performance changes: compare comparable starting state/policies and report
  end-to-end net BP/hour plus stage timings. Warm/cold OCR, lead changes,
  incidental completions and different quest mixes must remain visible.

Required paths for PRs currently in review:

| PR | Live acceptance paths |
| --- | --- |
| #1659 verified reroll | Menu opens beside a different quest than planned and the farmer corrects the cursor before paying; a fourth-slot reroll; intended replacement and matching BP spend. An out-of-eggs egg in slot four is a candidate trigger without withdrawing an egg. The fourth arrow box has not yet been seen live. |
| #1644 unsupported-quest rerolls | Pickup rerolled while another quest remains runnable; one-quest fallback when nothing is runnable, replacing one unconfigured unsupported quest rather than the whole panel. |
| #1642 failed-quest memory | A quest exhausts attempts and stays unattempted across later batches while present; after a fully known four-slot board proves its removal, a returning quest receives attempts again. |
| #1645 out-of-retries action | A Reroll-policy run with paid target/replacement evidence and a Stop-policy run following normal end-of-program handling, including configured go-home, time-fix and finish notification. Skip follows #1642. |
| #1636 battle-entry guard | An unconfirmed Tera battle entry skips the handler; a normal confirmed entry invokes it. |
| #1639 endless-reroll guard | Native tests normally suffice: the guard usually requires an unrealistic exclusions table to trigger. If it occurs naturally, preserve live evidence; ordinary farming does not verify it. |
| #1626 save/reset milestones | A reached or crossed completion milestone saves/resets once; subsequent rounds without another milestone do not repeat that save. Record active interval and exact completion/save events. |

Track each acceptance path separately. A useful partial run can verify one path
while leaving another INCONCLUSIVE; do not label the whole PR fully verified
when a required path has not been observed. Avoid manufacturing destructive
state merely to satisfy a checklist.

## Extract a feature into its own PR branch

Maintain a mapping: milestone → lab commit(s) → tested SHA/run IDs → release
branch/commit(s) → prerequisite PRs → validation status. Feature commits contain
the feature/tests only; lab manifests, experiment settings and unrelated fixes
are not release content.

- **Independent feature:** create `bbq/<feature>` in a separate worktree from
  current upstream main, then `git cherry-pick -x <feature-commit...>`.
- **Dependent feature:** create it from the smallest exact prerequisite branch
  head, cherry-pick only the feature, and state `Depends on #...; merge those
  first; feature delta is <commits>`. A feature that relies on another change
  cannot be made independent simply by omitting that dependency's files.
- **Multiple prerequisite stacks:** use a dedicated integration base containing
  only required ancestors, then the feature commit(s), as current M6 does. Keep
  all dependencies explicit and rebase onto main as they merge. Do not copy
  the entire lab branch or conceal dependencies in a giant squash commit.
- Reuse an existing appropriate PR rather than opening a duplicate. Claude
  applies handoff commits to existing PR branches; Codex does not push there.
- Verify the release branch's diff, native tests and full build. Check the diff
  against its intended base for files under `tests/`; none may be introduced.
  Keep the notes and lab folders on their existing fork branches, or outside
  the repository if preferred; never cherry-pick their commits into release.
- A pure rebase does **not** require another live run. Record the tested feature
  base/head separately from the full integrated lab SHA, then compare:

  ```sh
  git range-diff <tested-base>..<tested-sha> <new-base>..<pr-head>
  ```

  If the feature's production-code patch is unchanged, retain the earlier live
  result and record the comparison. Test-only merge-conflict resolution in
  `PokemonSV_Tests.cpp` does not itself require a live rerun; rerun native tests
  and the build. Feature code changes need relevant live revalidation. A change
  to a relied-on runtime dependency or its removal must also be assessed for
  changed behavior; range-diff is evidence of patch equivalence, not proof that
  arbitrary different runtime environments are equivalent.
- Create the PR as a **draft**, with a `Live verification: VERIFIED` section and
  remaining dependency/review work. Do not conflate passing a trial with merging
  or automatically changing someone else's existing draft state. Mark ready for
  review only after dependencies and branch-specific checks are satisfied.

Keep PR descriptions short: problem/result, dependencies and relevant native
validation. Claude adds a short **Live verification** section from RESULT.md:

- Tested SHA (and rebase equivalence when applicable).
- Duration.
- Acceptance paths triggered.
- Result, including any unverified required paths.
- One line of BP income, expense and net.

Link RESULT.md for the ledger, five-minute observations, other issues and full
run details. Those details belong in the run record, not pasted into the PR.
Codex uses this same concise format when initially opening a new draft PR.
Do not claim untested branches or unobserved rare paths passed.

## New issues and run records

For each new finding, search existing milestones/PRs first. Augment a matching
milestone; otherwise allocate the next M-number. Add reproduction/context,
timestamped example, observed versus suspected cause, proposed bounded change,
native/live acceptance criteria and an ROI estimate. Push the notes/queue update.
Creating a milestone note does not mean creating a duplicate implementation PR.

Use `tests/pokemon_sv/lab/runs/<date>-<milestone>-<sequence>/RESULT.md` for a
reviewable run summary. Keep raw video/large artifacts in ignored build-prep
storage; commit concise scoped fixtures/selected frames without secrets.

Every run record contains:

- Candidate/milestone, lab SHA, baseline SHA, prerequisite SHAs, binary hash,
  build/test log paths, app/profile identity and active settings snapshot.
- Hypothesis and per-path acceptance checklist; control/baseline comparability.
- Start/end/deadline, lead/party/egg state, starting/ending board and visible BP.
- Five-minute observations, feature triggers and outcomes, attempt/dispatch
  counts, completion gaps, rerolls and verified payment/replacement evidence.
- Income ledger by quest identity, reroll expense, expected net, observed delta,
  discrepancy, total duration and net BP/hour. Label reconstructed amounts.
- VERIFIED/FAILED/INCONCLUSIVE, new findings/linked milestones, release branch/
  PR/dependencies, and the next action. Preserve failed and interrupted samples.

## Queue ordered by estimated return

Current sequence starts with M7a, then M8a, following the user's clarification
that M4–M6 are already done. For the remaining candidates, rank by
expected marginal net BP/hour improvement, confidence and implementation plus
validation effort. Estimated saved time is a proxy until measured BP results
exist. Re-score after every run; do not sum overlapping opportunities.

| Order | Candidate | Evidence / expected return | Confidence and effort |
| --- | --- | --- | --- |
| 1 | M7a: safe reuse of verified quest-board observations | Main runs spent 24.25min in 210 measured reads; first remove the immediate scheduling reread after a verified reroll while preserving certainty and retry state | Baseline evidence exists; native tests/build and a 1–2h M7 run. Integrate prerequisite PRs as preparation, not a repeated M4–M6 campaign. |
| 2 | M8a: restore/verify party after hatch failures | Correctness gate: one timeout bypassed restoration; wrong lead coincided with 10–12min Auto Battle versus 4min after repair | Confirmed defect. Roughly 0.5–1.5 engineering days plus focused and farming validation. |
| 3 | M11a: do not enter rinse after scrub timeout; bound recovery | Six failed wash attempts today each incurred an unnecessary roughly one-minute stage; small, well-supported change before speculative positioning work | High confidence in removing that wait; roughly 0.25–0.5 day plus live exposure. Completion improvement unproven. |
| 4 | M12a: observe and recover target acquisition | Shared Pride Rock routes consumed 66min of the final 128min run without success; largest remaining route opportunity | Medium/low root-cause confidence; roughly 1–3 days. Capture one complete approach before choosing a fix. Benefit overlaps V0. |
| 5 | M13: bound nested navigation recovery | One return consumed 8m50s inside a catch; outer quest budgets cannot intervene | Confirmed control-flow gap, uncertain frequency; roughly 0.5–1 day plus focused validation. |
| 6 | M7 follow-ups: whole-board accounting and scheduling | One incidental photo completion missed the counter; a newly recognized red quest waited for old work | Separate reviewable slices; re-score after M7a. They need not delay M8a. |
| 7 | M9a: stop at defeat target; avoid unnecessary heal/reset | Final correct-lead Auto Battles already took about four minutes; earlier 10–12min traces are confounded | Medium confidence in marginal gain; roughly 1–2 days. Compare using the correct lead first. |
| 8 | M8b: startup egg inventory and box selection | Prevent wrong-box/no-egg attempts and manual setup; today's main correctness issue is already M8a | Requested but frequency/throughput benefit unmeasured; roughly 1–2 days. |
| 9 | M10a: implement Pickup | Eight Pickup rerolls in the longer runs cost 80 BP, about 19 BP/hour; successful pickup could also earn rewards but takes route time | Unknown route cost/success; roughly 1–3 days. Reroll savings alone do not prove better net BP/hour. |
| 10 | M10b/c: enable Flying-type catch/photo | No observed frequency/cost baseline today; existing disabled routes require investigation | Low current ROI certainty; estimate after observing demand and route behavior. |

Effort ranges are planning estimates, not measured delivery promises. M7's
observation certainty may be a prerequisite for parts of other slices and can
move earlier when required. A failed candidate stays in the history with its
evidence, is corrected or reverted before the next comparison, and is never
treated as a verified dependency just because it compiled.
