# M0 findings — Switch 1 local baseline

## Result and provenance

The unchanged local farmer completed **10 quests in 53m47s**, from October 20,
2026 12:26:55.253150 to 13:20:42.394144 PDT. The user stopped it during an
in-flight photo attempt; that attempt is censored, not a failure.

The board ledger infers **360 BP gross, 30 BP rerolls, 330 BP net**, approximately
**368 net BP/hour**. No starting/ending BP balances were recorded, so these are
inferred rewards, not reconciled balance measurements. Three requested blue
rerolls were followed by changed target slugs. See [events](ledger/events.csv),
[tasks](ledger/tasks.csv), [boards](ledger/boards.csv), and
[10/30/60-minute windows](ledger/windows.csv). Partial windows are marked.

Source: default-profile `SerialPrograms.log`, original physical lines
46059–55947. CSV source line numbers refer to the bounded extraction
`/private/tmp/bbq-m0.log`, retained outside version control. The extraction covers
12:26:50 through the STOPPED record. Regenerate its ledger with
`skills/serialprograms-logs/scripts/bbq_ledger.py`, the existing
`quest-rewards.json`, and output `m0/ledger`.

Build source: a7d15b941 (no gameplay changes; untracked macOS helpers do not
alter farmer logic), v0.71.2-user, local arm64 build. The program reopened using
the default profile after the earlier development window disappeared. Camera
Support was corrected from On to Off before this run. These differ from the
overnight run, so this is a new baseline, not a controlled comparison of one fix.
Configuration details and startup uncertainty are in [run notes](run-notes.md).

## Where the time went

Times cover dispatch through verification/recovery for finished outer batches,
including retries. They exclude between-task board management and the stopped
final photo. Samples are small and correlated.

| Requested quest | Successful / finished batches | Total task minutes | Inferred gross BP |
| --- | ---: | ---: | ---: |
| Catch-any | 1 / 4 | 20.23 | 20 |
| Auto-battle 10 | 3 / 3 | 14.59 | 60 |
| Swimming photo | 2 / 3 | 4.90 | 60 |
| Sour sandwich | 1 / 1 | 3.47 | 150 |
| Travel 500 | 1 / 1 | 1.50 | 20 |
| Coastal photo | 1 / 1 | 1.38 | 30 |
| Make TM | 1 / 1 | 1.16 | 20 |

Catch-any consumed **38% of the whole run**. Three failed two-attempt batches
consumed 14m43s; the fourth batch failed once, then caught successfully. Seven
of eight individual catch attempts logged failure to enter battle. Those are
repeated attempts on the same outstanding quest, not eight independent samples.
The eventual success rules out treating this route as categorically impossible.

Auto-battle completed reliably here, but averaged 4m52s per completion. It is a
second bottleneck, separate from failure recovery. The two successful swimming
photos each took about 74 seconds; an earlier two-attempt batch failed first.
The sour sandwich supplied 42% of gross earnings, illustrating how much a short
run's return depends on its red-quest mix.

## Diagnosis supported by source and logs

1. `process_and_do_quest()` initializes `quest_attempts` to zero on every call.
   Its skip after exhaustion lasts only for the current batch. The outer farmer
   processes the same board again and invokes it with a fresh budget. This
   directly explains the repeated catch batches.
2. Battle-entry timeouts identify an acquisition failure, but do not explain
   whether the target was absent, heading was wrong, or loading was late.
   Camera Support Off did not eliminate this failure in this run.
3. The catch path already guards battle handling with its encounter-watcher
   result. Do not assume every catch timeout executes battle commands. Review
   Tera/Sneak-up paths independently before changing them.
4. Pickup remains unsupported. This run does not prove that rerolling pickup
   works; unsupported-policy handling needs its own milestone.

## Decision and execution order

Keep the existing exclusions and navigation for the next comparison. Start M1
with exact requested slugs, numbered attempts, route/verification durations,
board/BP snapshots, and local failure screenshots. Read BP from the already
captured quest panel; add no panel-opening sequence. Treat OCR failure as
unknown, and verify the BP crop against the visible balance on the Switch.

Then follow the workplan: M2 save milestone guard; M3 confirmed-entry guard;
M4 retry persistence; M5 unsupported reroll policy; M6 bounded failed-quest
fallback; M7 no-progress handling. Keep each behavior change separately
committed and checked. M4 alone prevents repeated attempts but can leave a
blocked board, so it is not the complete recovery solution.

Do not reroll all catches based on this sample. At the illustrative 400 BP/hour
baseline, 10 BP corresponds to 90 seconds. Avoiding a roughly five-minute failed
batch can justify a blue reroll, but replacement difficulty, menu time and lost
bonus progress still matter. Validate the reroll and spending path before using
it automatically. Leave automatic red rerolls to explicit policy.

M1 acceptance: one new run must show an exact slug and attempt limit, plausible
BP readings across a completion/reroll, durations, and a readable local entry
failure image. Compilation is necessary but does not satisfy that gameplay gate.
Proceed to later behavior milestones after this check; another unchanged hour
is not required to establish that the existing retry budget resets.
