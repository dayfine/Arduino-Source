# Incremental BBQ workplan

Companion to the [improvement plan](BBQImprovementPlan.md) and
[overnight analysis](../analysis/bbq-2026-10-20/report.md).

## Working agreement

Implement one milestone at a time, with a separate local commit for each
behavior change. Keep unrelated route/scheduler changes out of that commit.
Record the tested revision, settings, evidence and result before advancing.
Use focused automated checks for state/policy changes and Switch 1 trials for
capture, menu, timing and spawn behavior. Compilation alone does not validate a
route. If a milestone regresses another quest, resolve or revert it before
moving on. Do not bundle the later experiments into the initial stall fix.

M0 is complete; see the [baseline findings](../analysis/bbq-2026-10-20/m0/findings.md).
M1 diagnostics and their Switch acceptance check are complete; see the
[validation record](../analysis/bbq-2026-10-20/m0/m1-validation.md).
Later behavior milestones remain pending.

## Stage 1 — Make blocked boards recover

| ID | Small change | Done when / validation |
| --- | --- | --- |
| M0 | Establish development baseline. | **Complete.** Local Switch 1 baseline: 53m47s, 10 completions, inferred 330 net BP (~368 BP/h). Default profile was used after reopening. Camera Support was corrected before the run; see M0 findings for comparability limits. |
| M1 | Add task-level diagnostic logging and BP snapshots. | **Complete.** Logs exact slug, numbered attempts, route/verification duration, board/BP, outcome and retry-exhaustion action. Catch entry-timeout and verification-failure images save locally. Validated repeated live 6750 readings, native 6750→6740 frames across one confirmed 10-BP reroll, and four production-reader regression cases at 1080p/720p; restoring the old crop fails all four cases. Navigation, retry and spending policies are unchanged; OCR/image I/O adds diagnostic overhead. Landing-phase images and broader route phase detail remain follow-ups. |
| M2 | Fix save/reset milestone handling. | A newly reached completion milestone saves once; repeated no-progress rounds and the initial zero count do not trigger repeated saves. Verify the state transition with a focused check. |
| M3 | Guard battle handling on confirmed entry. | An entry timeout cannot fall through into Tera/attack commands. A normal encounter still follows its existing battle/shiny-handling path. Exercise both paths. |
| M4 | Persist the outstanding quest's retry budget across rounds. | With one retry configured, the same outstanding quest gets two attempts total. Rereading the board does not reset the budget; confirmed completion/replacement does. Existing skip fallback cannot reattempt the exhausted quest next round. |
| M5 | Honor reroll policy for unsupported quests. | Pickup appears in configuration with unsupported status; its selected action works. Unsupported catch/photo-flying policies are no longer silently ignored. Verify replacement against the next board and BP change; bound unclear/failed confirmations. |
| M6 | Add failed-quest fallback using the verified reroll path. | An exhausted quest follows its configured reroll/skip/stop action. Blue and red costs are handled explicitly; insufficient BP or repeated uncertainty stops/reports instead of repeatedly charging. The replacement receives a fresh attempt budget. |
| M7 | Add board-level no-progress recovery. | The photo-savanna / pickup / sneak-up board cannot loop indefinitely after individual policies run. Refresh OCR and capture evidence before recovery. An absent/unreadable bonus slot is not blindly rerolled. Demonstrate bounded recovery or a clear stop. |

**Stage gate:** perform a controlled recovery run followed by a longer run.
Verify that completions resume, BP spending reconciles, failure limits persist,
and no flat-count save/reset loop remains. Report any residual zero-return
windows and their causes. Keep navigation unchanged during this comparison.

## Stage 2 — Re-measure and select economically

| ID | Small change | Done when / validation |
| --- | --- | --- |
| M8 | Refresh the quest velocity/ROI analysis after recovery fixes. | Update per-slug batch outcomes, time, gross/spent/net BP and 10/30/60-minute returns. Keep small samples, censored tasks and bonus-progress assumptions visible. Identify the next measured bottleneck. This is an analysis milestone, not a scheduler rewrite. |
| M9 | Trial one interpretable quest-priority rule, if M8 supports it. | Compare one rule (for example fast available photos/TM before a dedicated Travel task) with the previous policy. Include red-quest progress and reroll costs. Retain only with improved observed throughput and no new starvation. |

**Stage gate:** decide which reliable-but-slow tasks need optimization and which
failing routes should be repaired or rerolled. Do not infer automatic blacklist
rules from a single night's pooled success fractions.

## Stage 3 — Remove measured overhead, one transition at a time

| ID | Small change | Done when / validation |
| --- | --- | --- |
| M10 | Skip an unnecessary heal/recharge on one verified path. | Start with the no-battle case. Logs show the skipped work and retained position/resource assumptions; successful battle recovery still heals when required. |
| M11 | Remove one verified duplicate flight. | The next route starts from an equivalent position/camera anchor and reaches its destination reliably. Measure time saved; retain re-anchoring when a heal or movement changed the start state. Repeat as separate later commits for other transitions. |
| M12 | Trial spawn-loading wait on one failing route. | Compare entry outcomes and full completion time with/without the bounded Switch 1 delay. Keep fallback active; do not apply an unmeasured blanket delay to all quests. |
| M13 | Trial the Pride Rock photo camera-reset correction. | Capture arrival/heading/target evidence and compare the single camera change. Establish whether it improves actual photo completion; absence of `Photo not taken` alone is insufficient. |

Radar-ball failures and fly-point OCR verification are separate diagnostic
follow-ups if these trials show incorrect orientation/destinations. First
reproduce each fault; then scope one detector/navigation change and its own
acceptance test. They are not prerequisites for proving the reroll fixes.

**Stage gate:** update the baseline and identify how much time remains in target
acquisition versus unavoidable travel. Preserve the shared route anchors while
removing overhead.

## Stage 4 — Improve target acquisition beyond fixed routes

| ID | Small change | Done when / validation |
| --- | --- | --- |
| M14 | Add bounded local target acquisition to one quest route. | Verified arrival, spawn wait, camera/lock-on attempts and battle confirmation have explicit outcomes and a shared timeout. The route returns to fallback rather than searching forever. |
| M15 | Replace Tera-self's single-Kleavor dependency. | An ordinary nearby wild target can satisfy the quest on Switch 1, with confirmed entry and preserved shiny handling. Compare success rate and full completion time against the prior route. |
| M16 | Apply the proven acquisition approach to one additional catch/Sneak-up route. | Preserve the chosen type or positional requirement. Measure that route independently; do not simultaneously migrate every catch/photo function. Repeat successful migrations in later commits. |

Only after these results, consider objective combinations, transitions between
nearby tasks and a bounded item-pickup prototype. Each becomes a separately
specified experiment; general terrain navigation is not required for the
initial throughput improvement.

**Final evaluation:** compare repeated long runs against the recovered baseline,
including net BP/hour, median/tail task duration, zero-return windows, failures
and reroll expenditure. The goal is sustained improvement; a faster successful
catch or a peak interval is not sufficient.

## Evidence recorded for each milestone

Record the commit, exact scope, automated checks, Switch/settings context,
elapsed duration, net BP/result, source log range and any unresolved behavior.
If gameplay validation is deferred, say so explicitly and keep that part of the
milestone open. Summarize meaningful results on #263; use #264 for changes to
program-event visibility beyond this BBQ instrumentation.
