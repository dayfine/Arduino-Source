# M2 save/reset milestones

Revision: `465f867af`, independent branch `bbq/save-reset-milestones` based on
`ed29956a8`. M1 diagnostics are not included in this change.

Replaying the original production condition with interval 50 reproduced repeated
saves at counts 0 and 50, and a missed save when a batch moved from 49 to 52.
The counter is initialized once per program invocation, not each round. The
cause is the modulo-only condition at the end of each round.

The fix compares completion counts before and after the round and saves once
when integer division by the current nonzero interval increases. Stalls do not
save. Batches crossing one or more milestones save once. Changing the interval
while stalled does not trigger a save. The setting label now says completed
quests. Save/reset controller routines and their placement are unchanged.

Automated evidence:
- Registered `PokemonSV::BBQSaveMilestones` failed with the original condition
  at 0→0, interval 50, and passes with the corrected condition.
- Coverage includes exact and crossed milestones, stalled counts, disabled
  saving, live interval changes, interval one and maximum count.
- Native arm64 macOS application build and CRLF-aware whitespace checks pass.
- Temporary local logs: `/private/tmp/bbq-m2-tests.log` and
  `/private/tmp/bbq-m2-fixed-build.log`.

No gameplay run was performed for this revision; elapsed gameplay duration and
net BP are not applicable. The next Switch smoke check should use a short save
interval and verify one successful save/restart, followed by a no-progress round
without another save. This milestone does not fix stalled quest retries or
establish a farming throughput improvement.
