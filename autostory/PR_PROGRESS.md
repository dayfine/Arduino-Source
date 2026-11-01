# AutoStory PR progress

Live status of [PR_PLAN.md](PR_PLAN.md). Update whenever a PR changes state.

Legend: ✅ merged upstream · 🔵 open upstream · 🟢 fork draft, CI green · 🟡 fork draft,
CI pending or red · 🛠 branch in progress · ⬜ not started · ❌ closed

Last updated: 2026-11-01 07:10 (S1 MoveOnlyFunction merged upstream as #1810; nav/shared gets a game-agnostic Pose commit and merges the new base in; frlg/mvp CI running; #115 and #110 merged into the BDSP lab branch; #111 carries #115's two missed fixes, #112 next).

```
base/upstream-main
├── 🟢 nav/shared  collector 1 = fork #114 ............... S2–S4 + Pose commit 19a030374 + base merge
│     S1 823f5318d MoveOnlyFunction (#72) ✅ upstream #1810 · S2 3209523a9 grid + leg planner (#66, +735)
│     S3 c246d3d56 ScreenMachine (#108, +333) · S4 f3a8943a3 story events (#79, +1204)
│     std::format pass applied inside S2/S4 (9 sites). Sources to close once accepted:
│     #72 (87071ad59) · #66 nav/core (dc9f1682f) · #108 (6c8c83473) · #79 (70e10070c)
├── 🛠 frlg/mvp  collector 3 (on nav/shared) ............. F0–F6 being cut from 🟢 #67 frlg/brock-mvp
└── ⬜ base/bdsp-1717 → ⬜ bdsp/mvp  collector 2 ........ sources #68, #109 and the lab branch
      bdsp/event-sandgem-center: ✅ #115 ci-fixes and ✅ #110 plan cap merged; 🟡 #111 decomp
      grids (merging the base in); 🟡 #112 00.01–00.03 lines waits for #111

Open fork PRs that stay as they are: #107 lab tooling (channel work landing), #103 lab/main,
#81, #62, #61, #86, #100, #78, #56, #85.
To close as superseded once each collector is accepted: #72 #66 #108 #79 → #114;
#67 → frlg/mvp; #68 #109 #110 #111 → bdsp/mvp.
```

## Upstream queue (publish in this order; two open at a time)

| # | PR | Source | State |
|---|---|---|---|
| 1 | S1 Common: MoveOnlyFunction | #72 | ✅ upstream #1810, merged 2026-11-01 |
| 2 | S2 Pokemon navigation: grid types and leg planner | #66 | 🟢 #114 S2 |
| 3 | S3 ScreenMachine + StepTrace | #108 | 🟢 #114 S3 |
| 4 | S4 Pokemon navigation: story checkpoints as events | #79 | 🟢 #114 S4 |
| 5–9 | F1–F5 FRLG overworld navigation | #67 | ⬜ |
| 10+ | B1–B5 BDSP AutoStory MVP | #68 and the lab branch | ⬜ blocked on upstream #1717 |

Already upstream from this work: #1810 (S1), #1798 (AGENTS.md docs-site note), #1791 all merged;
#1754 (BBQ) still open. One slot is free for the shared queue (S2 next), the user publishes.

Lab branches (no PRs): `lab/main` = main + #107; `lab/bdsp` = bdsp/event-sandgem-center + #115 +
lab/main; `lab/frlg` = frlg/brock-mvp + lab/main. Rebuilt on the collectors when they exist.
