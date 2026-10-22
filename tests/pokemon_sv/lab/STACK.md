# BBQ validation stack

Branch: bbq/validation-stack
Worktree: /Users/difan/Projects/Arduino-Source-bbq-lab

Workflow and prioritized queue:
https://github.com/dayfine/Arduino-Source/blob/bbq/m7-board-read-notes/tests/pokemon_sv/VALIDATION_WORKFLOW.md

## Baseline components, 2026-10-21

| Component | Exact revision | Lab integration |
| --- | --- | --- |
| Upstream main | db9ef1772ff59053a166060673cbde3b6c8e23ac | Branch starting point |
| M6 and required M4/rounds/planner/verified-reroll stack | a1b685f9e255bd8de34eed566a998322900da271 | Merge 9d5d232fe |
| M5 unsupported-quest policy feature | f7e3ba6f0743b1e72286c3a8ee214d0f6494fd17 | Cherry-pick 075376f2f |

PR dependencies: #1645 requires #1642 → #1632 and #1659 → #1640.
#1644 requires #1659 → #1640. These are existing PR revisions, not new features
verified by earlier runs of a different binary.

Source integration completed without conflicts. Full build, native tests and
live validation: **PENDING**. No new candidate feature or live run has started.
Next candidate: V0, verify persistent exhaustion and configured fallback across
batches. Use an isolated bbq-lab profile and the workflow's preflight/run record.

The branch is an accumulating experiment stack, never a feature PR source.
Append each milestone's feature SHA(s), tested integrated SHA, run IDs/outcome,
release branch/PR and exact dependencies. Keep release commits free of lab-only
settings and records. No existing feature/reviewer branch was rewritten.
