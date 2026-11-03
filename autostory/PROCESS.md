# AutoStory: internal process notes

Current implementation order: [FRLG demo work plan](FRLG_DEMO_WORK_PLAN.md)
(2026-10-28). The two demo slices and RNG handoffs replace the older fixed
Phase A PR sequence below; broader SV migration remains off the critical path.

Moved out of the upstream-facing design doc (fork PR #42,
`docs/design/AutoStory.md` §8) on 2026-10-25 after review: agent roles do
not belong in a doc that upstream reviewers read.

- All new AutoStory work goes up as fork drafts on `dayfine/Arduino-Source`.
  Nothing goes upstream without the user (AGENTS.md, PR_WORKFLOW.md).
- Per AGENTS.md, Codex owns new feature branches until their first fork PR
  and does the live validation on the Switch. Claude prepares PRs and their
  descriptions, rebases them and answers review comments.
- Phase A (FRLG pilot to Brock, about 10 PRs) comes first. Phase D (R0–R5,
  migrating SV AutoStory to a shared core) waits until the pilot runs and
  people use it. The existing worktree `Arduino-Source-autostory-r1`
  (`autostory/r5-dev-tests`) is phase D work. Park it; don't hand it upstream
  before the pilot.
- BDSP #1717: the review comment drafts are in
  `docs/design/AutoStory-1717-review-drafts.md` on `design/autostory`. Agents
  never post them. The user decides.
- Gate F3: log Switch hours per pilot checkpoint (development + validation)
  in run records here, so the go/no-go numbers are measured rather than
  estimated. The 3 h/checkpoint budget was proposed in the 2026-10-25
  revision and needs the user's confirmation.
