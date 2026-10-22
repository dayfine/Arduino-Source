# BBQ farmer follow-up milestones

Implementation/run loop and ranked queue: [VALIDATION_WORKFLOW.md](VALIDATION_WORKFLOW.md).

| Milestone | Work | Validation needed | Status |
| --- | --- | --- | --- |
| [M7](M7.md) | Quest-board certainty and reuse; investigate the recorded five-read reroll sequence | Native C++ tests, local build, comparative live timing/board reads | Investigation documented |
| [M8](M8.md) | Restore/verify party after hatch failures; check egg inventory and select an egg box at startup | Native C++ tests, local build, real box navigation and hatch/no-egg run | Requested; hatch-timeout restoration defect documented |
| [M9](M9.md) | Improve Auto Battle target yield, avoid unnecessary plaza/healing cycles, and bound stalled attempts | Native C++ tests, local build, recorded cave visit and comparative live timing | Observed; documented |
| [M10](M10.md) | Implement Pickup and investigate/validate disabled Flying-type capture/photo routes | Native C++ tests, local builds, separate live validation for each route | Requested; documented |
| [M11](M11.md) | Recover from stalled Pokémon washing; investigate picnic furniture obstruction and avoid rinse after scrub failure | Native C++ tests, local build, controlled obstructed/clear-view live runs | Observed; requested; documented |
| [M12](M12.md) | Verify/recover shared Pride Rock target acquisition for catch, sneak-up and Savanna photo | Native C++ tests, local build, recorded target-present/absent route trials | New finding from daily audit; documented |
| [M13](M13.md) | Bound nested plaza/map recovery; preserve cancellation and expose navigation failure to the farmer | Native C++ tests, local build, bounded navigation-failure trial | New finding from daily audit; documented |

See the [2026-10-21 run audit](RUN_AUDIT_2026-10-21.md) for all eleven BBQ starts,
completion velocity, route outcomes, positive evidence and prioritized follow-ups.
The [BP ledger](BP_LEDGER_2026-10-21.md) adds reconstructed reward income,
reroll expense and net BP/hour, including incidental completion adjustments.

These are planning notes, not implemented changes. Existing milestones are
tracked in their GitHub PRs; these follow-ups must be evaluated against the latest
integrated architecture before code work starts.
