# BBQ PR progress

Audited 2026-11-06 against live GitHub PR state/base metadata and the diffs of
all 34 originally open BBQ-related fork PRs. Inventory: **43 fork PRs: 32 open, 10 closed,
1 merged notes PR**. Selection: head branch `bbq/*` or title containing BBQ /
Blueberry, excluding later audit/status documentation PRs (including #239). Shared dependencies are listed separately below. This is an inventory
and dependency audit, **not renewed CI/live-console approval**. Old build counts
and readiness badges are not carried forward.

[PR_PLAN.md](PR_PLAN.md) remains the broader design/stage plan; its 2026-10-26
PR graph is historical. Previous tracker versions remain in Git history. Merge
parents into children; do not follow old rebase/force-push instructions. No agent publishes upstream.

## Active dependency tree (parent cleanup pending)

Numbers below are active fork code PRs. #51 is closed because its tests-file
change merged upstream (#1754); it is no longer a prerequisite in the active
plan. Some remaining children still name the retained `bbq/tests-file` branch in GitHub
metadata (shown in the inventory), and their owners must merge current main and
retarget those bases. The tree below shows the required parent relationships
after that mechanical cleanup, not a claim that the live bases already changed.

```text
main
 ├→ #19                            target-specific evidence
 ├→ #23                            catch grace / heal policy
 ├→ #37 → #38 → #53 → #8
 │                     ├→ #2 → #3 board/attempt/reroll logging, then screenshots
 │                     └→ #6 → #5 → #4 → #10
 │                                        ├→ #25 → #16 → #52
 │                                        │      └→ #73
 │                                        └→ #12 → #13 → #22 → #26
 ├→ #15 → #17 → #18 → #21 → #31 → #32   Auto Battle
 ├→ #24 → #30                            navigation/return bounds
 ├→ #29                                 Make TM resume
 ├→ #33                                 egg verification/restoration
 ├→ #180                                detector image tests
 └→ #35                                 Kleavor look-around; old closed parent also needs cleanup
```

## Complete fork inventory

| PR | State | Current base | Scope / disposition |
|---|---|---|---|
| [#1](https://github.com/dayfine/Arduino-Source/pull/1) | closed | `main` | Upstream #1636 merged; fork closed. |
| [#2](https://github.com/dayfine/Arduino-Source/pull/2) | open | `bbq/verified-reroll` | Canonical diagnostics, includes #27; native Linux/Windows CI passes. |
| [#3](https://github.com/dayfine/Arduino-Source/pull/3) | open | `bbq/diagnostics-and-baseline` | Updated for consolidated #2; local failure screenshots only. #19 overlap remains. |
| [#4](https://github.com/dayfine/Arduino-Source/pull/4) | open | `bbq/endless-reroll-guard` | save once when completion milestones advance |
| [#5](https://github.com/dayfine/Arduino-Source/pull/5) | open | `bbq/persistent-quest-retries` | stop instead of rerolling the quest panel forever |
| [#6](https://github.com/dayfine/Arduino-Source/pull/6) | open | `bbq/verified-reroll` | don't retry a failed quest every round while it stays on the panel |
| [#7](https://github.com/dayfine/Arduino-Source/pull/7) | closed | `bbq/tests-file` | Replaced by fork #180 after upstream #1759 closed. |
| [#8](https://github.com/dayfine/Arduino-Source/pull/8) | open | `bbq/pin-stuck-rounds` | check which quest a reroll pays for, and that it was replaced |
| [#9](https://github.com/dayfine/Arduino-Source/pull/9) | closed | `bbq/verified-reroll` | Superseded by #25. |
| [#10](https://github.com/dayfine/Arduino-Source/pull/10) | open | `bbq/save-reset-milestones` | let a quest that fails all its retries be rerolled or stop the program |
| [#12](https://github.com/dayfine/Arduino-Source/pull/12) | open | `bbq/exhausted-quest-actions` | reuse verified reroll panels for selection and first dispatch |
| [#13](https://github.com/dayfine/Arduino-Source/pull/13) | open | `bbq/reuse-quest-board` | replan after rerolling an exhausted quest |
| [#14](https://github.com/dayfine/Arduino-Source/pull/14) | closed | `review/1632-base` | Closed review-annotation trial. |
| [#15](https://github.com/dayfine/Arduino-Source/pull/15) | open | `bbq/tests-file` | run the Auto Battle loop against injectable game steps, with a per-pass log |
| [#16](https://github.com/dayfine/Arduino-Source/pull/16) | open | `bbq/reroll-impossible-quests` | show BP earned, spent, net and BP/hour in the stats |
| [#17](https://github.com/dayfine/Arduino-Source/pull/17) | open | `bbq/auto-battle-seam` | stop Auto Battle at the kill target and skip the last heal (behavior change) |
| [#18](https://github.com/dayfine/Arduino-Source/pull/18) | open | `bbq/auto-battle-stop-at-target` | give up Auto Battle after 3 empty passes or a time limit (behavior change) |
| [#19](https://github.com/dayfine/Arduino-Source/pull/19) | open | `bbq/tests-file` | Target-specific evidence/options; not a replacement for #2. |
| [#20](https://github.com/dayfine/Arduino-Source/pull/20) | closed | `main` | Closed; future navigation work remains in PR_PLAN. |
| [#21](https://github.com/dayfine/Arduino-Source/pull/21) | open | `bbq/auto-battle-bounds` | heal between Auto Battle passes only when HP is below 50% (behavior change) |
| [#22](https://github.com/dayfine/Arduino-Source/pull/22) | open | `bbq/replan-after-reroll` | reuse verified quest panel reads after a failed-quest reroll (C9) |
| [#23](https://github.com/dayfine/Arduino-Source/pull/23) | open | `bbq/tests-file` | stop fleeing from catch targets, skip the heal after no-battle attempts (behavior change) |
| [#24](https://github.com/dayfine/Arduino-Source/pull/24) | open | `main` | check Central Plaza arrivals; a failed quest fly fails the attempt, not the program |
| [#25](https://github.com/dayfine/Arduino-Source/pull/25) | open | `bbq/exhausted-quest-actions` | reroll quests it cannot do as soon as they appear, red or blue (behavior change) |
| [#26](https://github.com/dayfine/Arduino-Source/pull/26) | open | `bbq/panel-read-reuse` | chain no-trip quests without round actions between (M15 step 1b) |
| [#27](https://github.com/dayfine/Arduino-Source/pull/27) | closed | `bbq/verified-reroll` | Consolidated into #2; closed Nov 6 after native Linux/Windows validation. Branch retained. |
| [#28](https://github.com/dayfine/Arduino-Source/pull/28) | closed | `bbq/exhausted-quest-actions` | Closed; broader quest-policy work remains planned. |
| [#29](https://github.com/dayfine/Arduino-Source/pull/29) | open | `main` | start Make TM at the last craftable TM |
| [#30](https://github.com/dayfine/Arduino-Source/pull/30) | open | `bbq/plaza-arrival-check` | bound the return to Central Plaza (behavior change) |
| [#31](https://github.com/dayfine/Arduino-Source/pull/31) | open | `bbq/auto-battle-heal-threshold` | Keep counting/stop behavior and its logs together. |
| [#32](https://github.com/dayfine/Arduino-Source/pull/32) | open | `bbq/auto-battle-kill-count` | near the Auto Battle target, stop when the quest leaves the board (C10, behavior change) |
| [#33](https://github.com/dayfine/Arduino-Source/pull/33) | open | `main` | Contains inline #2/#3 copies plus lab artifacts; narrow to egg behavior. |
| [#34](https://github.com/dayfine/Arduino-Source/pull/34) | closed | `main` | Closed startup-only proposal. |
| [#35](https://github.com/dayfine/Arduino-Source/pull/35) | open | `bbq/battle-entry-guard` | look around for another target when Kleavor is absent |
| [#36](https://github.com/dayfine/Arduino-Source/pull/36) | closed | `bbq/kleavor-look-around` | Closed catch-any proposal. |
| [#37](https://github.com/dayfine/Arduino-Source/pull/37) | open | `bbq/tests-file` | run rounds against injectable BBQGameActions |
| [#38](https://github.com/dayfine/Arduino-Source/pull/38) | open | `bbq/save-milestone-tracker` | separate the quest plan from the controller actions |
| [#49](https://github.com/dayfine/Arduino-Source/pull/49) | merged | `agents` | Merged notes PR, not a code prerequisite. |
| [#51](https://github.com/dayfine/Arduino-Source/pull/51) | closed | `main` | Upstream #1754 merged Nov 3; redundant fork PR closed Nov 6, branch retained for children. |
| [#52](https://github.com/dayfine/Arduino-Source/pull/52) | open | `bbq/bp-status` | check the BP stats against the rerolls at the end of a run |
| [#53](https://github.com/dayfine/Arduino-Source/pull/53) | open | `bbq/quest-planner` | pin the ways the round loop gets stuck today |
| [#73](https://github.com/dayfine/Arduino-Source/pull/73) | open | `bbq/reroll-impossible-quests` | fail the egg quest when the box has no egg, and check for one at start (behavior change) |
| [#180](https://github.com/dayfine/Arduino-Source/pull/180) | open | `main` | Active replacement for closed #7/#1759. |

## Upstream and shared dependencies

- PokemonAutomation/Arduino-Source#1628: BP crop, **merged Oct 28**.
- PokemonAutomation/Arduino-Source#1631: quest identity, **merged Oct 28**.
- PokemonAutomation/Arduino-Source#1636: confirmed Tera entry, **merged Oct 26**.
- PokemonAutomation/Arduino-Source#1754: BBQ test file, **merged Nov 3**; fork #51 closed Nov 6 after confirmation.
- PokemonAutomation/Arduino-Source#1759 and fork #7: detector tests **closed**; active replacement #180.
- Fork #50: shared streamless error-report fix, **closed**; upstream #1743 **merged**.
- Upstream #1625/#1626/#1629/#1632/#1639/#1640/#1641/#1642/#1644/#1645/#1659:
  **closed handoffs**, not merged implementations; corresponding work remains in fork PRs above.
- No current open upstream BBQ PR was found among dayfine's PRs at this audit.
  Upstream publishing limits and authorization remain governed by AGENTS.md,
  not by historical queue text.

## Logging audit and consolidation

All open diffs were checked, including logs added in non-logging PRs.

| PRs | Log/evidence scope | Ownership decision |
|---|---|---|
| #2 | Board, attempt start/return/result, timings, battle-entry timeout, exhaustion | Canonical general diagnostic logger and tests. |
| #27 | Slot + quest line before every single-slot reroll, including failures | Code and exactly-once outcome tests are published and native-tested in #2; #27 is closed. |
| #33 | Inline copies of board/attempt/timeout logs and snapshots | Remove duplicate diagnostics from the egg PR; #2/#3 own these features. Egg verification/restoration stays in #33. |
| #3, #19 | Snapshot/path records versus detailed target acquisition evidence | Keep distinct feature scope; deduplicate the shared catch-entry timeout capture when integrating. #19 is not a replacement for general attempt logs. |
| #15, #31, #32 | Per-pass Auto Battle log, kill accounting and board-completion confirmation | Keep with the algorithms/results they explain; #31 changes counting and stop behavior, not just formatting. |
| #6, #8, #10, #12, #13, #22, #25, #26, #37, #38 | Stop/reroll verification, scheduler/board reuse and controller decisions | Keep decision/outcome logs with the feature; avoid a second generic attempt/board/reroll emitter. |
| #16, #52 | BP ledger/reconciliation | Keep with BP accounting. |
| #23, #24, #29, #30, #35, #73 | Catch/arrival/heal/TM/return/target/egg decisions | Keep with their specific behavior and tests. |
| #4, #5, #17, #18, #21, #51, #53, #180 | No new production logging emission found in the reviewed delta | No logging consolidation needed. |

The #2/#27 consolidation is now published in #2, based on verified-reroll #8.
#3 has merged that parent and retains only local screenshot evidence. Native Linux/Windows validation passes at the heads below; remaining platform
checks continue. Other behavior branches retain their own
explanatory logs. Closed PRs are not assumed merged; upstream readiness remains
separate from this inventory.

## Outstanding work from this audit

- Strip duplicate diagnostics from #33 and narrow its unrelated historical lab
  notes/analysis artifacts before handoff; preserve source history/evidence.
- #3 is developer/bug-report evidence, currently captured automatically. Decide
  lab-only versus explicit opt-in (default off) before normal-user release; keep
  it separate from #2. Resolve its #19 catch-timeout capture overlap at integration.
- Refresh #35's obsolete closed parent in its owning chain.
- Refresh #51's children onto the main branch containing upstream #1754 in their
  owning chains. #51 itself is now closed; its branch was retained, not deleted.
- Recheck CI/tests at the final heads after changes. No new console run is authorized
  or claimed by this documentation audit.

## Published logging consolidation

- #2 branch `bbq/diagnostics-and-baseline`, head
  `57002fb3aeb785f7cd5c4146f23f30dfe8247bd9`, now targets #8's
  `bbq/verified-reroll` branch. Merge `141193220` includes #27 head
  `6e9871d582495f3f6cc7ab8beed83d564da10bc0`; the follow-up initializes the
  default expected color required by Windows Clang's warning-as-error policy.
- `BBQLogger` owns the reroll formatter and all general diagnostic events.
  Nine reroll outcome tests retain their exactly-once assertions. Six explicit
  logger cases live in `PokemonSV_BBQLogger_Tests.cpp` beside the formatter.
  Duplicate legacy status messages are replaced, and exhaustion stays red.
- #3 branch `bbq/failure-screenshots`, head
  `5db585ba3682e94984ee84f19b5860e441065bd9`, has merged #2. The snapshot adapter
  augments the canonical timeout/result events rather than duplicating them.
  The caller keeps retry/counting decisions. Both screenshot regressions now
  live beside the snapshot source as input/expectation table rows.
- Both PR descriptions reflect their current scope and dependencies.
  Intended-base code health passes for both. #2 native CI passes on
  [Linux](https://github.com/dayfine/Arduino-Source/actions/runs/42306026274) and
  [Windows Clang](https://github.com/dayfine/Arduino-Source/actions/runs/42306026142).
  [#3 Linux Clang](https://github.com/dayfine/Arduino-Source/actions/runs/42306032346)
  explicitly passes all nine reroll, six logger and two screenshot cases.
  Remaining platform checks continue.
- #27 is closed as redundant after that native validation; its branch is
  retained. It is no longer an active dependency or an unlanded candidate.
- No hook bypass, force-push, upstream mutation or console run was needed.
  Earlier exploratory branches and the #33 candidate below remain local;
  they are not the published branches to review.

### #33: diagnostic-only removal prepared; egg-code findings remain

- Branch `integration/bbq-egg-log-dedup`, commit `d7191a26e`, in
  `/Users/difan/Projects/wt/bbq-egg-log-dedup`.
- Removes 45 copied diagnostic lines from three production files, retaining
  hatch/restore behavior. Intended-base code health still reports 3 egg-code
  findings: large source file, long hatch routine, undocumented namespace close.
- Still needs the owning PR's code-health fixes and native validation. Its
  unrelated historical lab notes/analysis artifacts also need scope review;
  preserve source history/evidence rather than silently deleting it.

The #2/#27 consolidation is published and native-tested, #27 is closed, and #3
has merged the update. The #33 cleanup and #3 distribution decision remain separate. No console run, upstream publication or hook bypass is claimed.
