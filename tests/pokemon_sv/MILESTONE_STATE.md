# BBQ milestone state tracker

Last reconciled: 2026-10-21 PDT, against the local lab branch and GitHub PR heads.
This is the authoritative operational state; [FOLLOW_UPS.md](FOLLOW_UPS.md) is
the issue index, and [VALIDATION_WORKFLOW.md](VALIDATION_WORKFLOW.md) defines the
loop, ownership, acceptance paths and ROI order.

Codex owns this tracker, implementation, the lab branch and run evidence. Claude
owns existing PR branches/descriptions and receives verified commits/results.

## Current checkpoint

- Active candidate: **V0 — integrate and verify the existing PR baseline**.
- Lab: `bbq/validation-stack`, HEAD `cb06dd640`, worktree
  `/Users/difan/Projects/Arduino-Source-bbq-lab`.
- Integrated: upstream main `db9ef1772`, M6 `a1b685f9e` and its M4/rounds/planner/
  verified-reroll ancestors, plus M5 `f7e3ba6f0` (lab cherry-pick `075376f2f`).
- Still to integrate before V0: #1636 `07f03d2fe`, #1639 `f62d994dd`,
  #1626 `548ac951c`. Recheck moving heads before import.
- Combined full build: **NOT RUN**. Combined native tests: **NOT RUN**.
- Lab profile/preflight: **NOT PREPARED**. Live run: **NOT STARTED**.
- No timer/observer is armed and no live run is scheduled. Controller handoff is
  needed before taking control from the user's Flying Trial program.

Earlier tests/runs of other revisions are evidence in the notes; they do not
verify this combined baseline or substitute for the acceptance paths below.

## Queue and implementation state

| Order | Candidate | Implementation / integration | Live verification | Next action |
| --- | --- | --- | --- | --- |
| 1 | V0: existing PR baseline | PARTIALLY INTEGRATED | NOT STARTED | Import three remaining PRs; build/native tests; prepare focused trials |
| 2 | M8a: hatch failure party restoration | DOCUMENTED; no feature commit | NOT STARTED | Implement restoration/restart contract through native action seam |
| 3 | M11a: failed scrub must not enter rinse | DOCUMENTED; no feature commit | Baseline failure/success evidence only | Implement bounded failure/recovery and transition gating |
| 4 | M12a: Pride Rock target acquisition | DOCUMENTED; no feature commit | Baseline missing-battle evidence only | Capture target/arrival/framing and choose bounded recovery |
| 5 | M13: nested navigation bounds | DOCUMENTED; no feature commit | Baseline prolonged recovery only | Bound total recovery and expose exhaustion |
| 6 | M7a: observation reuse/accounting | DOCUMENTED; no feature commit | Baseline redundant reads/incidental completion only | Design certainty/invalidation and native scenarios |
| 7 | M9a: target exit/heal policy | DOCUMENTED; no feature commit | Mixed-lead baseline only | Compare correct-lead route, then reduce avoidable work |
| 8 | M8b: startup egg box/inventory | DOCUMENTED; no feature commit | NOT STARTED | Verify eggs/box selection without party changes |
| 9 | M10a: Pickup | DOCUMENTED; no active route | NOT STARTED | Establish what counts and measure a bounded pickup route |
| 10 | M10b/c: Flying-type catch/photo | DOCUMENTED; disabled routes exist | NOT STARTED | Establish demand and why routes are disabled |

The order follows the workflow's estimated marginal ROI, with correctness
gates first. Re-score after each evaluated run; overlapping savings are not
additive. Baseline observations are not feature verification.

## Existing PR state and outstanding evidence

All listed PRs are OPEN as of this reconciliation. Full acceptance definitions
are in the workflow; each path has its own VERIFIED/FAILED/INCONCLUSIVE result.

| PR | GitHub state | Lab integration | Current-head live evidence still required |
| --- | --- | --- | --- |
| #1636 battle-entry guard | Ready for review | PENDING | Unconfirmed Tera entry skips handler; confirmed entry takes handler |
| #1639 endless-reroll guard | Draft | PENDING | Native guard scenarios; live only if naturally encountered |
| #1626 save/reset milestones | Draft | PENDING | Reached/crossed milestone saves once; no duplicate save without a new milestone |
| #1659 verified reroll | Draft | INTEGRATED via M6 | Wrong initial cursor corrected; fourth-slot arrow; matching replacement/spend |
| #1642 persistent failed quests | Draft | INTEGRATED via M6 | Exhaustion persists across batches; full-panel removal permits a new instance |
| #1644 unsupported policies | Draft | INTEGRATED as 075376f2f | Pickup reroll with other runnable work; single unsupported fallback |
| #1645 exhausted-quest action | Draft | INTEGRATED | Reroll paid/replaced; Stop follows normal configured ending; Skip memory |

Dependencies: #1642 → #1632; #1659 → #1640; #1644 → #1659;
#1645 → #1642 + #1659; #1639 and #1626 → #1632; #1636 independent.
An already-ready PR is not automatically live-verified; a draft PR can have
verified evidence while dependency/review work remains.

## Immediate execution steps

1. Fetch/recheck heads and integrate #1636, #1639 and #1626 in the lab worktree.
   Resolve integration conflicts there and update STACK.md with exact SHAs.
2. Run relevant native C++ tests and build the full arm64 app. Record logs and
   binary hash. Do not proceed to console control with a failed build/test.
3. Prepare isolated `bbq-lab` settings/stats and a V0 RESULT.md. Choose the
   acceptance paths to trigger; keep unrelated hatch attempts disabled until
   M8a is fixed. When the controller is available, open the exact local app,
   fly/return to Central Plaza, verify party/board and capture starting BP.
4. Arm an identity-checked deadline observer and start a 60-minute trial. Check
   every five minutes; extend to 120 minutes only for a concrete evidence gap.
   Distinct policies/rare paths can need separate trials; ordinary success does
   not verify paths that never triggered.
5. Stop normally, capture ending state/BP, audit feature behavior and new issues,
   reconcile the BP ledger, and classify each path. Retain failed/interrupted
   runs and queue new findings. No feature trigger means INCONCLUSIVE.
6. Hand Claude verified results and any corrective lab commits for the existing
   PRs. For a new feature, extract/build/test its dedicated release branch and
   open a draft, then hand over PR ownership. Pure patch-equivalent rebases
   retain live evidence under the workflow's range-diff rule.
7. Re-score the queue and implement M8a next, unless V0 uncovers a prerequisite
   regression that must be fixed before further trials.

## Update discipline

After every integration, build, run, handoff or PR state change, update the
relevant row and append a dated checkpoint. A verified entry must link feature
commit(s), exact tested SHA, RESULT.md, triggered paths and release PR/branch.
Keep implementation, live result and PR readiness separate. No notes/run files
under tests/ may be included in a release branch's diff.

| Date | Checkpoint | Evidence |
| --- | --- | --- |
| 2026-10-21 | Tracker initialized; V0 partially integrated, no combined build/run yet | Lab HEAD cb06dd640 and GitHub PR metadata reconciled |
