# AutoStory PR progress

Live status of [PR_PLAN.md](PR_PLAN.md). Update whenever a PR changes state.

Legend: ✅ merged upstream · 🔵 open upstream · 🟢 fork draft, CI green · 🟡 fork draft,
CI pending or red · 🛠 branch in progress · ⬜ not started · ❌ closed

Last updated: 2026-11-01 (after the user's four decisions; shared collector being built).

```
base/upstream-main
├── 🛠 nav/shared  collector 1 ........................... S1–S4 being applied; CI to follow
│     sources: 🟡 #72 (87071ad59) · 🟡 #66 nav/core (dc9f1682f, build CI green, code
│     health red until main is merged in) · 🟢 #108 (6c8c83473) · 🟡 #79 (70e10070c)
├── ⬜ frlg/mvp  collector 3 ............................. source 🟡 #67 frlg/brock-mvp (CI green)
└── ⬜ base/bdsp-1717 → ⬜ bdsp/mvp  collector 2 ........ sources #68, #109, #110, #111,
      lab branch bdsp/event-sandgem-center (7b24d871e; Ubuntu gcc and Windows clang red,
      three pre-existing failures listed in PR_PLAN.md)

Open fork PRs that stay as they are: #107 lab tooling (channel work landing), #103 lab/main,
#81, #62, #61, #86, #100, #78, #56, #85.
To close as superseded once each collector is accepted: #72 #66 #108 #79 → nav/shared;
#67 → frlg/mvp; #68 #109 #110 #111 → bdsp/mvp.
```

## Upstream queue (publish in this order; two open at a time)

| # | PR | Source | State |
|---|---|---|---|
| 1 | S1 Common: MoveOnlyFunction | #72 | 🛠 in nav/shared |
| 2 | S2 Pokemon navigation: grid types and leg planner | #66 | 🛠 in nav/shared |
| 3 | S3 ScreenMachine + StepTrace | #108 | 🛠 in nav/shared |
| 4 | S4 Pokemon navigation: story checkpoints as events | #79 | 🛠 in nav/shared |
| 5–9 | F1–F5 FRLG overworld navigation | #67 | ⬜ |
| 10+ | B1–B5 BDSP AutoStory MVP | #68 and the lab branch | ⬜ blocked on upstream #1717 |

Already upstream from this work: #1798 (AGENTS.md docs-site note), #1791, #1754 (BBQ). That is
three open; the shared queue starts when one of them merges or is closed.
