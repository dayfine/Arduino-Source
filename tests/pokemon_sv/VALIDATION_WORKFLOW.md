# BBQ milestone implementation and live validation loop

Status: operational workflow documented; isolated testing branch created.
Its integrated baseline has not yet been built, tested or run. This is a
workflow document, not an installed agent skill.

## Branches and ownership

- Testing branch: `bbq/validation-stack` on dayfine/Arduino-Source.
- Isolated worktree: `/Users/difan/Projects/Arduino-Source-bbq-lab`.
- Planning/evidence branch: `bbq/m7-board-read-notes`; milestone notes, daily
  audits, BP ledgers and this workflow live under `tests/pokemon_sv/`.
- Release branches: one branch per feature/PR. Never open a feature PR directly
  from the testing branch, which accumulates unrelated experimental changes.
- Existing PRs remain owned/refined on their existing branches. Fetch revisions
  into the lab; do not overwrite another agent's branch or push stale history.

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

These are prerequisites, not new implementations to recreate. M3 (#1636),
diagnostic PRs and other sibling changes are not automatically in this baseline.
Integrate and verify them explicitly if a trial needs them. Recheck GitHub
dependencies before extraction because these PRs are still being refined.

## Repeat this loop for one candidate at a time

1. **Select and specify.** Take the highest-ranked eligible queue item below.
   Record its hypothesis, exact acceptance criteria, prerequisites, observed
   baseline and anticipated BP/time benefit. Split large milestones into slices
   such as M8a (party restoration) and M8b (startup egg-box selection).
2. **Implement one slice.** Add a feature commit on the testing branch, with
   native C++ tests exercising the production seam. Keep necessary follow-up
   corrections as adjacent commits belonging to that slice. Do not add another
   independent feature until the current candidate has an evaluated result.
   Existing implemented PRs can enter as verification-only candidates.
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
6. **Run for 60 minutes.** Record actual start and a deadline. Extend to at most
   120 minutes only if conditions remain useful and more exposure can resolve a
   specific uncertainty. Start a separate deadline observer that survives the
   sampling turn and checks the exact app/profile/run identity. A written plan
   alone is not an armed timer. Never let a broad timer stop a different program.
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
10. **Extract and publish.** For a verified slice, make a dedicated PR branch as
    described below, build/test that branch, then create or update its draft PR.
    Link the exact tested revision, native cases, live evidence and dependencies.
    Update the queue and stack manifest, then select the next candidate.

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
- Reuse an existing appropriate PR rather than opening a duplicate. Coordinate
  revisions with whoever is refining that branch; no force-push over their work.
- Verify the release branch's diff, native tests and full build. If extraction
  changes behavior or removes runtime dependencies, repeat the relevant live
  validation. A lab run validates that integrated SHA, not an arbitrary cherry-pick.
- Create the PR as a **draft**, with a `Live verification: VERIFIED` section and
  remaining dependency/review work. Do not conflate passing a trial with merging
  or automatically changing someone else's existing draft state. Mark ready for
  review only after dependencies and branch-specific checks are satisfied.

PR descriptions must state the problem/result, exact dependency chain, feature
delta, native coverage, run duration, triggered acceptance paths, BP income/
expense/net, other issues and remaining limits. Link the evidence. Do not claim
untested branches or unobserved rare paths passed.

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

Correctness gates precede speed optimization. Within that constraint, rank by
expected marginal net BP/hour improvement, confidence and implementation plus
validation effort. Estimated saved time is a proxy until measured BP results
exist. Re-score after every run; do not sum overlapping opportunities.

| Order | Candidate | Evidence / expected return | Confidence and effort |
| --- | --- | --- | --- |
| 1 | V0: integrate and verify existing M4/M5/M6 | Final baseline spent 69% in failed dispatches; persistent exhaustion/fallback prevents repeating the same failed batch indefinitely | High confidence in bounded retries; net earning gain needs measurement. Existing code; build/native checks plus 1–2h live trial. |
| 2 | M8a: restore/verify party after hatch failures | Correctness gate: one timeout bypassed restoration; wrong lead coincided with 10–12min Auto Battle versus 4min after repair | Confirmed defect. Roughly 0.5–1.5 engineering days plus focused and farming validation. |
| 3 | M11a: do not enter rinse after scrub timeout; bound recovery | Six failed wash attempts today each incurred an unnecessary roughly one-minute stage; small, well-supported change before speculative positioning work | High confidence in removing that wait; roughly 0.25–0.5 day plus live exposure. Completion improvement unproven. |
| 4 | M12a: observe and recover target acquisition | Shared Pride Rock routes consumed 66min of the final 128min run without success; largest remaining route opportunity | Medium/low root-cause confidence; roughly 1–3 days. Capture one complete approach before choosing a fix. Benefit overlaps V0. |
| 5 | M13: bound nested navigation recovery | One return consumed 8m50s inside a catch; outer quest budgets cannot intervene | Confirmed control-flow gap, uncertain frequency; roughly 0.5–1 day plus focused validation. |
| 6 | M7a: reuse verified observations and reconcile whole-board changes | Main runs spent 24.25min in 210 measured reads; some are necessary. One incidental photo completion missed the counter | High evidence for redundant reads/accounting gap; roughly 1–2 days. Target only redundant reads; no assumed 24min saving. |
| 7 | M9a: stop at defeat target; avoid unnecessary heal/reset | Final correct-lead Auto Battles already took about four minutes; earlier 10–12min traces are confounded | Medium confidence in marginal gain; roughly 1–2 days. Compare using the correct lead first. |
| 8 | M8b: startup egg inventory and box selection | Prevent wrong-box/no-egg attempts and manual setup; today's main correctness issue is already M8a | Requested but frequency/throughput benefit unmeasured; roughly 1–2 days. |
| 9 | M10a: implement Pickup | Eight Pickup rerolls in the longer runs cost 80 BP, about 19 BP/hour; successful pickup could also earn rewards but takes route time | Unknown route cost/success; roughly 1–3 days. Reroll savings alone do not prove better net BP/hour. |
| 10 | M10b/c: enable Flying-type catch/photo | No observed frequency/cost baseline today; existing disabled routes require investigation | Low current ROI certainty; estimate after observing demand and route behavior. |

Effort ranges are planning estimates, not measured delivery promises. M7's
observation certainty may be a prerequisite for parts of other slices and can
move earlier when required. A failed candidate stays in the history with its
evidence, is corrected or reverted before the next comparison, and is never
treated as a verified dependency just because it compiled.
