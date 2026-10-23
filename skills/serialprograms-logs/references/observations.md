# Observed archives, inspected 2026-10-20

Source directory: `/Users/difan/Library/Application Support/SerialPrograms/`.

| File | Actual first–last timestamps | Physical lines |
| --- | --- | ---: |
| SerialPrograms-20261017-101045350858.log | Oct 14 17:07:29.174500 – Oct 17 10:10:45.346557 | 604030 |
| SerialPrograms-20261020-032023983416.log | Oct 17 10:10:45.350070 – Oct 20 03:20:13.936042 | 592888 |

Timestamps have no explicit timezone. Do not add UTC or convert without confirming application behavior.

The first archive contains extensive Dynamax Adventure output and no `Quest:` messages. The second begins in the same adventure output, then includes BBQ runs late October 19–20. Both are around 50 MiB, so avoid dumping broad tag matches into the conversation.

Second archive, file-wide literal message counts (not unique task outcomes):

- 75 `Quest:` starts: Sneak up 40; Tera-self 15; Travel 7; Sandwich 4; Auto Battle 4; Make TM 3; Wild Tera 2.
- 54 `Did not enter battle` messages: 13 mention Kleavor, 39 say `Did target spawn?`, and 2 use other wording. Do not attribute these solely by the most recent quest-start message: fallback execution can cross task boundaries.
- 35 `Quest completed successfully.` and 25 `Quest did not complete successfully.` messages.
- 25 retry-exhaustion/skip messages.
- 98 radar-ball fallbacks across the whole file, including before the first BBQ run. Do not label all 98 BBQ errors.
- One `Saving and resetting.` message. These archives alone do not demonstrate the repeated-save bug.

Useful source locations in the second archive:

- Line 564730: first BBQ banner; immediately fails because console type was unspecified. STOPPED at 564737.
- Line 564747: BBQ start at Oct 19 23:40:00.
- Lines 564822–564825: quest OCR, including Traditional Chinese normalization, `(sneak-up)`, `(pickup-10)`, `(photo-polar)` and `no matches` for the fourth read.
- Lines 567639–567649: Oct 19 23:59:25 fatal picnic menu-navigation error, followed by error-report creation. Stream history was not enabled, so no stream history was saved.
- Line 568049: BBQ start Oct 20 00:08:25; stop at 573323 (00:47:47).
- Line 576241: BBQ start Oct 20 01:22:16; continues beyond the archive boundary, with last visible stats reaching 21 completed quests.
- Lines 592155–592587: Sneak-up attempts at 03:14:19 and 03:16:00; both fail battle entry; retry exhaustion at 03:17:42. Travel quest then succeeds at 03:19:08; Sneak-up starts again at 03:19:23 (592810). This is direct evidence that skipping does not prevent later-round retries.

For issue 264: `Quest: Sneak up.` is tagged `[Console 0]`, whereas repeated `Let's Go Kill Sound found, score ...` messages are tagged `[Program]`. Color/severity cannot be recovered reliably from these plain-text tags. Existing logger APIs accept message and color, while overlay messages use a separate `add_log` API; tags are insufficient to infer what appeared on-screen.

## Task-duration analysis

Measure an outer task from `[Program]: Current quest exists on list. Doing quest.` to the next `[Program]: Quest completed successfully.` or `Quest did not complete successfully.`. Internal `Quest:` messages include retries, so do not treat each as an independent outer task. Reset pending attribution on a new program banner or STOPPED. A stop or archive ending with a pending task is censored; exclude it from completed-task success ratios and report its time separately if needed. Some photo/catch functions do not emit the same `Quest:` prefix, so keep unlabelled tasks unclassified until inspecting their actual messages/OCR.

Using these boundaries in the Oct 20 archive:

| Outer task, first Quest label | Completed outer calls | Reported successes | Total minutes | Failed-call minutes |
| --- | ---: | ---: | ---: | ---: |
| Sneak up | 21 | 4 | 64.87 | 58.00 |
| Tera-self | 6 | 1 | 25.47 | 22.84 |
| Travel 500 meters | 7 | 7 | 9.68 | 0 |
| Make TM | 3 | 3 | 2.11 | 0 |
| Auto Battle 10/30 | 4 | 4 | 22.11 | 0 |

These counts exclude interrupted outer tasks and do not imply unbiased success probabilities. All four completed Sneak-up successes occur before the final run. The final run (01:22:16 to archive end 03:20:13, about 118 minutes) contains 17 completed Sneak-up batches, all failures, consuming 58 minutes. The latest visible quest-completion stat is 21. Roughly 49% of this observed run is spent on those failed batches. Removing that time alone gives an optimistic throughput multiplier around 118/(118-58) = 1.97, before reroll costs, replacement-quest time and changed quest distribution.

Five completed Tera-self batches fail, each about 272–277 seconds (two internal attempts); one earlier call succeeds in about 158 seconds. Other Tera tasks were interrupted. Thus both routes demonstrably worked earlier but later failed persistently; logs do not establish whether spawn depletion, loading, orientation, route drift or another condition caused it.

For conditional reroll economics use net opportunity rate r (BP/min), saved time D, reroll cost C and menu time t: net gain approximately r*(D-t)-C, assuming replacement work can sustain r. At the user's estimated 300–400 BP/hour, a 10 BP reroll costs 1.5–2 minutes of opportunity; a 50 BP reroll costs 7.5–10 minutes. These are counterfactual estimates, not measured gains. Preserve red-quest bonus progress and account for replacement/reroll chains; the log does not directly measure net BP.

Issue 264 evidence posted with explicit user authorization: https://github.com/PokemonAutomation/ComputerControl/issues/264#issuecomment-6296691270

## Continuation through the current log

`SerialPrograms.log` continues the same BBQ session after the Oct 20 archive
and stops at 08:17:27. Ignoring the current file would miss nearly five hours.
The complete reconstruction and source-value assumptions are in
`analysis/bbq-2026-10-20/report.md`, with a slug reward catalog and audit CSVs.

The board stalls after 05:05:19 on photo-savanna / pickup-10 / sneak-up, with no
further inferred rewards for 3.2 hours. The fixed Pride Rock navigation route is
shared by photo-savanna, catch-any and sneak-up. Catch-any succeeds immediately
before the stall. This is a useful shared-dependency hypothesis, not proof of
spawn depletion: catch and Sneak-up both try date skipping to respawn targets.

`Photo: Normal/Fire` is a shared route label that also handles photo-savanna.
`Catch: Any/Normal/Fire` likewise covers multiple requested quests. Join task
labels to the OCR board and processing decisions rather than assuming labels
uniquely identify rewards. `Photo not taken.` means the expected prompt was not
detected; it does not distinguish missing targets, camera state and a detector
failure. Board transitions and a successful control quest help discriminate
general application failure from a set of routes failing together.
