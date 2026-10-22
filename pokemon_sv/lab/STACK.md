# BBQ validation stack

Branch: bbq/validation-stack
Worktree: /Users/difan/Projects/Arduino-Source-bbq-lab

Workflow and prioritized queue:
https://github.com/dayfine/Arduino-Source/blob/agents/pokemon_sv/VALIDATION_WORKFLOW.md

## Baseline components, 2026-10-21

| Component | Exact revision | Lab integration |
| --- | --- | --- |
| Upstream main | db9ef1772ff59053a166060673cbde3b6c8e23ac | Branch starting point |
| M6 and required M4/rounds/planner/verified-reroll stack | a1b685f9e255bd8de34eed566a998322900da271 | Merge 9d5d232fe |
| M5 unsupported-quest policy feature | f7e3ba6f0743b1e72286c3a8ee214d0f6494fd17 | Cherry-pick 075376f2f |
| Tera battle-entry guard #1636 | 07f03d2feaaf8d233d34b9273704aeaa4a50d3ad | Merge d50a2bb65 |
| Endless-reroll guard #1639 | f62d994dd83a4e82efbeb6da4fb6bbfb149ec187 | Merge 7abb517f6 |
| Save/reset milestones #1626 | 548ac951c4aaf65d30f189bd1cfbcae233f6f09b | Merge 294876bad |
| Save regression adapted to M4/M6 policy | 8aa1821b4 | Lab integration test correction |
| M7 observation reuse | 57f330fc9 | Feature commit; native tests/build passed |

PR dependencies: #1645 requires #1642 → #1632 and #1659 → #1640.
#1644 requires #1659 → #1640. These are existing PR revisions, not new features
verified by earlier runs of a different binary.

Integration conflicts were resolved in test registration and the shared round
loop. Full arm64 app build: **PASS**. Relevant registered native tests:
**55 PASS, 0 FAIL**, including six new M7 scenarios. M7 live validation:
**INCONCLUSIVE** for first-hour reuse: 75 three-slot reads, zero reuse triggers.
User stopped/restarted at 19:49 PDT with changed exclusions; new run continues.
Starting 13,690 BP; observed 13,770 BP at 20:08 across both runs.
Active candidate: M7a, safe reuse of verified quest-board observations, then M8a.
The user clarified M4–M6 are done; do not repeat them as a separate V0 campaign.
The remaining prerequisite PRs are now imported and built. Use the isolated
bbq-lab profile and an M7-specific preflight/run record.

Binary SHA-256: 9bc7b7b0da2396993615384a815fdb160e79ca53cbfc1ae893a73edbf1626603.
Local evidence: build-prep/build.log, rebuild.log, native-tests.log.
Run preparation: runs/2026-10-21-m7-preparation/RESULT.md (relative to this directory).

The branch is an accumulating experiment stack, never a feature PR source.
Append each milestone's feature SHA(s), tested integrated SHA, run IDs/outcome,
release branch/PR and exact dependencies. Keep release commits free of lab-only
settings and records. No existing feature/reviewer branch was rewritten.

Active trial: [M7 live RESULT](runs/2026-10-21-m7-live/RESULT.md).

This manifest and run records are maintained on `agents` from now on.
All build-prep evidence paths refer to the lab worktree above. Historical
records on the code stack are retained as provenance, not updated there.

## M7b candidate ready for next session

Feature ea047a930: end a batch after an exhausted-quest reroll and replan.
The testing stack now contains this commit. Full isolated arm64 build PASS;
58 native tests PASS, including three new red-to-green regression scenarios.
Build worktree: /Users/difan/Projects/Arduino-Source-m7b.
Live M7b result: NOT STARTED. See
[runs/2026-10-21-m7b-preparation/RESULT.md](runs/2026-10-21-m7b-preparation/RESULT.md).
The running old lab executable remains M7a 57f330fc9, despite source HEAD moving.
M7a still needs its optimized four-slot planner-reroll path verified.

## M7a/M7b final acceptance, 2026-10-21 22:12 PDT

Supersedes earlier active-run statuses above. Tested integrated ea047a930; full
arm64 / 58 native PASS. Both intended live paths observed in the
[M7 combined result](runs/2026-10-21-m7b-live/RESULT.md). Normal early Stop;
35m53s phase two, 200 earned − 50 spent = +150 BP, ending 14,250. Game saved
at Central Plaza, full Lv100 Primarina lead. Limited stability duration; no
claim of completed one-hour trial or deadline-stop verification.

Release extraction branches pushed: bbq/reuse-quest-board at 8dbace6c7
(M6 + M5 parents; full build / 41 native PASS); bbq/replan-after-reroll at
30e84abef (M7a + save milestones parents; full build / 46 native PASS).
Production feature patch IDs match tested lab features; see linked result.
BP run statistics requested as independent M14, documented but not implemented.

## Fork PR handoff

- M7a: [dayfine/Arduino-Source#12](https://github.com/dayfine/Arduino-Source/pull/12),
  draft, head bbq/reuse-quest-board (8dbace6c7), base bbq/exhausted-quest-actions
  (fork #10), with M5 fork #9 also merged.
- M7b: [dayfine/Arduino-Source#13](https://github.com/dayfine/Arduino-Source/pull/13),
  draft, head bbq/replan-after-reroll (30e84abef), base bbq/reuse-quest-board
  (fork #12), with save milestones fork #4 also merged.
- Claude owns both PR branches/descriptions from creation onward. Codex does
  not push refinements to them. Neither draft is upstream-ready while parents
  remain unmerged. Existing PR branches were not pushed.
