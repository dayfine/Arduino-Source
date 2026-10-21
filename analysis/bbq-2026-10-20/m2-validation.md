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


## Switch 1 smoke check

Revision `465f867af`, standalone M2 build; M1 diagnostics were not included.
Run: **18:01:53.482444–18:16:06.525427 PDT**, **14m13s**. Traditional Chinese,
1920×1080 NV12/60fps, wired PABotBase2, existing exclusions and routes,
NUM_RETRIES=1, SAVE_NUM_QUESTS=1 (temporarily reduced from 50). NUM_QUESTS=1000
was retained; the test was manually stopped. The original save interval 50 and
quest limit 1000 were restored after closing the idle app.

- Starting native frame visibly shows **6740 BP**, photo-swim / sneak-up /
  auto-10, all at zero progress.
- Photo-swim completed at 18:04:05.434977. Despite an earlier `Photo not taken`
  message, the native board frame at 18:04:11 shows **6770 BP** and replacement
  by tera-self-defeat, confirming the 30-BP reward.
- Sneak-up failed both attempts: entry timeouts at 18:05:18.974959 and
  18:07:12.165731, final verification failure at 18:08:03.165778.
- Auto-10 completed at 18:14:07.803526 after multiple clearing passes. The
  program's completion count advanced from 0 to 2 across this batch.
- Exactly one `Saving and resetting. Completed quests: 2` event occurred, at
  **18:14:18.744123**, followed by save-menu detection, closing/relaunching the
  game, and `Entered game!` at **18:15:20.111915**. At **18:15:25.115838** the
  stats were **Quests Completed: 2 — Saves: 1**.
- The next batch rerolled tera-self-defeat into auto-10 under the existing
  selected policy, then began sneak-up. That attempt was interrupted when the
  short trial stopped; it is censored, not a completed failure.

The **physical save/restart smoke check passed**, including one save after a
batch crossing multiple interval-one milestones. A full no-progress round after
that save was not reached within this short trial; live stall coverage remains
open. Deterministic state-transition tests cover zero and repeated stalled counts.
No conclusion about improved farming throughput follows from this run.

The two verified rewards total 50 BP. One selected blue reroll would spend
10 BP, implying net +40 BP and a final balance of 6780; that final balance was
**not captured visibly**, so it remains inferred. The last directly inspected
balance is 6770. Final position was an overworld grass plateau after the stopped
navigation, outside battle.

Evidence: [stopped app showing 2 completions / 1 save](m2/stopped-app.png),
[photo reward checkpoint](m2/photo-reward.png), and
[focused event excerpt](m2/events.log). Full local source log:
`/private/tmp/bbq-m2-live-run.log`; original application log under
`~/Library/Application Support/SerialPrograms/SerialPrograms.log`.
