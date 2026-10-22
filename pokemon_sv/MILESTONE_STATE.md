# BBQ milestone state tracker

Last reconciled: 2026-10-21 PDT, against the local lab branch and GitHub PR heads.
This is the authoritative operational state; [FOLLOW_UPS.md](FOLLOW_UPS.md) is
the issue index, and [VALIDATION_WORKFLOW.md](VALIDATION_WORKFLOW.md) defines the
loop, ownership, acceptance paths and ROI order.

Codex owns this tracker, implementation, the lab branch and run evidence. Claude
owns existing PR branches/descriptions and receives verified commits/results.

## Current checkpoint

- Active candidate: **M7a — reduce redundant quest-board reads safely**.
- Notes/rules now live on `agents`, worktree
  `/Users/difan/Projects/Arduino-Source-agents`; old notes branch retired from use.
- M4–M6 are **DONE for this milestone sequence**, per the user's clarification.
  Claude continues their upstream PR refinement; do not restart their live
  acceptance campaign as a new V0 milestone. This records milestone completion,
  not a claim that all PRs merged or every listed rare path was observed.
- Lab: `bbq/validation-stack`, tested production SHA `57f330fc9`, worktree
  `/Users/difan/Projects/Arduino-Source-bbq-lab`.
- Integrated: upstream main `db9ef1772`, M6 `a1b685f9e` with M4/rounds/planner/
  verified-reroll ancestors, M5 `f7e3ba6f0` (lab cherry-pick `075376f2f`),
  #1636 `07f03d2fe`, #1639 `f62d994dd`, #1626 `548ac951c`.
- Lab integration test correction: `8aa1821b4`; no production change in that commit.
- Full arm64 build: **PASS**. Registered native C++ tests: **55 PASS, 0 FAIL**.
- Binary SHA-256: `9bc7b7b0da2396993615384a815fdb160e79ca53cbfc1ae893a73edbf1626603`.
- Isolated `bbq-lab` profile prepared; hatching disabled. Lead Primarina Lv.100,
  full HP; Central Plaza and starting board verified. Starting BP **13,690**.
- First M7 live trial: **INCONCLUSIVE for reuse**, 18:49:05–19:49:03 PDT:
  six completions; 75 three-slot reads; zero four-slot reuse triggers.
- Preflight error: unsupported hatch-egg/skip JSON row loaded as auto-10/skip.
  User cleared exclusions and restarted at 19:49:04. New run continues under
  changed settings; three completions and 13,770 BP observed by 20:08.
- Read-only five-minute observer records the restarted run. No stop timer owns
  it. Old observer exited on a historical-state bug, now corrected locally;
  deadline stopping remains unverified. User retains control of the newer run.
- Run record: [M7 trial](lab/runs/2026-10-21-m7-live/RESULT.md) on `agents`.

The user stopped Flying Trial and returned manually after automation missed the
handoff. The new shallow AX helper successfully pressed Screenshot and Start;
deadline Stop still needs outcome verification. M7's starting board has only
three quests, so live reuse needs a later eligible four-slot observation.

## Queue and implementation state

| Order | Candidate | Implementation / integration | Live verification | Next action |
| --- | --- | --- | --- | --- |
| 1 | M7a: safe quest-board observation reuse | IMPLEMENTED 57f330fc9; full build and 55 native tests passed | First hour reuse INCONCLUSIVE; changed-settings restart running | Observe eligible four-slot reroll path before claiming reuse verified |
| 2 | M8a: hatch failure party restoration | DOCUMENTED; no feature commit | NOT STARTED | Implement restoration/restart contract through native action seam |
| 3 | M11a: failed scrub must not enter rinse | DOCUMENTED; no feature commit | Baseline failure/success evidence only | Implement bounded failure/recovery and transition gating |
| 4 | M12a: Pride Rock target acquisition | DOCUMENTED; no feature commit | Baseline missing-battle evidence only | Capture target/arrival/framing and choose bounded recovery |
| 5 | M13: nested navigation bounds | DOCUMENTED; no feature commit | Baseline prolonged recovery only | Bound total recovery and expose exhaustion |
| 6 | M7 follow-ups: incidental accounting and scheduling | DOCUMENTED; separate from first M7 slice | Baseline incidental completion/delayed red quest | Re-score after M7a; keep independent changes reviewable |
| 7 | M9a: target exit/heal policy | DOCUMENTED; no feature commit | Mixed-lead baseline only | Compare correct-lead route, then reduce avoidable work |
| 8 | M8b: startup egg box/inventory | DOCUMENTED; no feature commit | NOT STARTED | Verify eggs/box selection without party changes |
| 9 | M10a: Pickup | DOCUMENTED; no active route | NOT STARTED | Establish what counts and measure a bounded pickup route |
| 10 | M10b/c: Flying-type catch/photo | DOCUMENTED; disabled routes exist | NOT STARTED | Establish demand and why routes are disabled |

M7 is next following the user's clarification; M8a follows. Remaining ordering
uses estimated marginal ROI and correctness constraints. Re-score after each
evaluated run; overlapping savings are not additive. Baseline observations are
not feature verification. M7 follow-ups need not delay M8a.

## Existing PR state and recorded coverage limits

All listed PRs were OPEN at the last GitHub reconciliation. The coverage notes
below preserve previously identified gaps; they do not reopen M4–M6 or block
M7 with a separate verification campaign. Claude owns remaining upstream work.

| PR | GitHub state | Lab integration | Previously identified coverage paths |
| --- | --- | --- | --- |
| #1636 battle-entry guard | Ready for review | INTEGRATED | Unconfirmed Tera entry skips handler; confirmed entry takes handler |
| #1639 endless-reroll guard | Draft | INTEGRATED | Native guard scenarios; live only if naturally encountered |
| #1626 save/reset milestones | Draft | INTEGRATED | Reached/crossed milestone saves once; no duplicate save without a new milestone |
| #1659 verified reroll | Draft | INTEGRATED via M6 | Wrong initial cursor corrected; fourth-slot arrow; matching replacement/spend |
| #1642 persistent failed quests | Draft | INTEGRATED via M6 | Exhaustion persists across batches; full-panel removal permits a new instance |
| #1644 unsupported policies | Draft | INTEGRATED as 075376f2f | Pickup reroll with other runnable work; single unsupported fallback |
| #1645 exhausted-quest action | Draft | INTEGRATED | Reroll paid/replaced; Stop follows normal configured ending; Skip memory |

Dependencies: #1642 → #1632; #1659 → #1640; #1644 → #1659;
#1645 → #1642 + #1659; #1639 and #1626 → #1632; #1636 independent.
An already-ready PR is not automatically live-verified; a draft PR can have
verified evidence while dependency/review work remains.

## Execution steps (1–5 completed; live trial in progress)

1. Fetch/recheck heads and integrate #1636, #1639 and #1626 in the lab worktree.
   Resolve integration conflicts there and update STACK.md with exact SHAs.
2. Implement M7a as a focused feature commit: carry a verified replacement board
   into scheduling instead of immediately reading it again. Define observation
   certainty/invalidation; keep fresh pre-payment validation and post-payment
   replacement verification. Handle unknown slots conservatively.
3. Run native tests for reuse, intervening mutations, stale/unknown boards and
   retry-state preservation; build the full arm64 app and record its hash/logs.
4. Prepare isolated `bbq-lab` settings/stats and an M7 RESULT.md. Measure board
   reads by purpose, dispatch delays and net BP/hour; keep hatch attempts disabled until
   M8a is fixed. When the controller is available, open the exact local app,
   fly/return to Central Plaza, verify party/board and capture starting BP.
5. Arm an identity-checked deadline observer and start a 60-minute M7 trial. Check
   every five minutes; extend to 120 minutes only for a concrete evidence gap.
   Distinct policies/rare paths can need separate trials; ordinary success does
   not verify paths that never triggered.
6. Stop normally, capture ending state/BP, audit feature behavior and new issues,
   reconcile the BP ledger, and classify each path. Retain failed/interrupted
   runs and queue new findings. No feature trigger means INCONCLUSIVE.
7. Extract/build/test M7's dedicated release branch with its exact dependencies,
   open a draft and hand it to Claude with RESULT.md. Any corrections to existing
   PRs go to Claude as lab commits. Pure patch-equivalent rebases
   retain live evidence under the workflow's range-diff rule.
8. Re-score the queue and implement M8a next, unless M7 uncovers a prerequisite
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
| 2026-10-21 | User clarified M4–M6 are done; removed redundant V0 campaign and selected M7a next, then M8a | User clarification; no additional implementation/build/live result claimed |

| 2026-10-21 18:49 PDT | M7 implemented and full build/55 native tests passed; live run started with timer/checkpoints | Feature 57f330fc9; starting 13,690 BP; lab live RESULT.md |

| 2026-10-21 20:10 PDT | First M7 hour inconclusive; invalid hatch exclusion blocked Auto 10, user restarted after edits | Zero reuse / 75 three-slot reads; 6 completions; +80 observed BP across both runs at 20:08; lab RESULT |

| 2026-10-21 | Notes/rules moved to agents; step 10 aligned with fork workflow; failed-reroll queue delay tracked as M7b | AGENTS.md; M7.md evening example; M7a does not fix M7b |
