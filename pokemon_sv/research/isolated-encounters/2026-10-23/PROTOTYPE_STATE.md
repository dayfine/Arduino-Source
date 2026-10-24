# Isolated hunt prototype — entry point

Read [IMPLEMENTATION_QUEUE.md](IMPLEMENTATION_QUEUE.md) for current code/build,
implemented flow, validation and remaining gaps. Read
[FIELD_VALIDATION.md](FIELD_VALIDATION.md) before borrowing the Switch.

Current pushed head: f2347d3d1d9a17614b0ff5746daccb615d7bbdd5 on
prototype/sv-isolated-hunt. Full application build and all six native test groups
pass; incremental lint passes. Options, type sandwiches, travel, bounded roaming,
refusal handling, shared encounter/catch/save and recovery are wired together.
Actual routes/habitats/sandwich run remain unverified pending field calibration.

The release instance is currently running Sword/Shield Max Lair, not SV/AZP.
It was not interrupted. A UI-only prototype instance with no devices assigned
was inspected and closed. No isolated-hunt PR has been created. No upstream PR
publication is authorized. Notes stay on agents.

Historical AZ controls and original handback evidence:
[2026-10-24 RESULT.md](../../../runs/isolated-2026-10-24/RESULT.md).
