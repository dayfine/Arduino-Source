# AutoStory PR progress

Live status of [PR_PLAN.md](PR_PLAN.md). Update whenever a PR changes state.

Legend: ✅ merged upstream · 🔵 open upstream · 🟢 fork draft, CI green · 🟡 fork draft,
CI pending or red · 🛠 branch in progress · ⬜ not started · ❌ closed

Last updated: 2026-11-01 05:40 (collector 1 open as #114, CI green; FRLG collector waits for the user's look at #114).

```
base/upstream-main
├── 🟢 nav/shared  collector 1 = fork #114 ............... 4 commits, CI green on Mac/Windows Clang/Ubuntu
│     S1 823f5318d MoveOnlyFunction (#72, +334) · S2 3209523a9 grid + leg planner (#66, +735)
│     S3 c246d3d56 ScreenMachine (#108, +333) · S4 f3a8943a3 story events (#79, +1204)
│     std::format pass applied inside S2/S4 (9 sites). Sources to close once accepted:
│     #72 (87071ad59) · #66 nav/core (dc9f1682f) · #108 (6c8c83473) · #79 (70e10070c)
├── ⬜ frlg/mvp  collector 3 ............................. source 🟡 #67 frlg/brock-mvp (CI green)
└── ⬜ base/bdsp-1717 → ⬜ bdsp/mvp  collector 2 ........ sources #68, #109, #110, #111,
      lab branch bdsp/event-sandgem-center (7b24d871e; Ubuntu gcc and Windows clang red,
      three pre-existing failures listed in PR_PLAN.md)

Open fork PRs that stay as they are: #107 lab tooling (channel work landing), #103 lab/main,
#81, #62, #61, #86, #100, #78, #56, #85.
To close as superseded once each collector is accepted: #72 #66 #108 #79 → #114;
#67 → frlg/mvp; #68 #109 #110 #111 → bdsp/mvp.
```

## Upstream queue (publish in this order; two open at a time)

| # | PR | Source | State |
|---|---|---|---|
| 1 | S1 Common: MoveOnlyFunction | #72 | 🟢 #114 S1 |
| 2 | S2 Pokemon navigation: grid types and leg planner | #66 | 🟢 #114 S2 |
| 3 | S3 ScreenMachine + StepTrace | #108 | 🟢 #114 S3 |
| 4 | S4 Pokemon navigation: story checkpoints as events | #79 | 🟢 #114 S4 |
| 5–9 | F1–F5 FRLG overworld navigation | #67 | ⬜ |
| 10+ | B1–B5 BDSP AutoStory MVP | #68 and the lab branch | ⬜ blocked on upstream #1717 |

Already upstream from this work: #1798 (AGENTS.md docs-site note), #1791, #1754 (BBQ). That is
three open; the shared queue starts when one of them merges or is closed.
