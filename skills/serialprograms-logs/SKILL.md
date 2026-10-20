---
name: serialprograms-logs
description: Inspect Pokemon Automation SerialPrograms logs, extract run boundaries and BBQ events, and distinguish program decisions from controller and inference noise.
---

# SerialPrograms logs

Read local logs without modifying the application's settings or files. On this Mac they live in `/Users/difan/Library/Application Support/SerialPrograms/`. Quote paths containing spaces. Logs can contain settings and personal information; keep raw logs outside the repository.

Use the streaming helper for a bounded overview, then narrow by timestamps and event patterns:

```sh
python3 skills/serialprograms-logs/scripts/read_logs.py '/path/to/SerialPrograms.log'
python3 skills/serialprograms-logs/scripts/read_logs.py '/path/to/SerialPrograms.log' --events --since '2026-10-20 00:08:25' --until '2026-10-20 00:47:48' --limit 100
```

The default summarizes the whole file; `--events` prints selected events with original physical line numbers. Add `--match 'pattern'` for an explicit regex, including OCR or low-level commands. Read [observations.md](references/observations.md) for known examples and interpretation traps.

For BP accounting, `scripts/bbq_ledger.py` accepts ordered logs, a quest reward JSON map (`quests` keyed by slug with `reward_bp` and `reroll_bp`), and an output directory. It exports board snapshots, inferred reward/reroll events, task timings, run boundaries and midnight-aligned 10/30/60-minute windows. See `analysis/bbq-2026-10-20/` in this repository for an example and its assumptions. The ledger infers rewards from board disappearances, not measured BP balances; same-slug replacements, OCR misses and manual activity can invalidate that inference. Windows retain partial observation and active-time fields; choose full windows explicitly for distribution summaries. Photo/catch task logs use `Photo:` and `Catch:` rather than `Quest:`.

## Parsing and filtering

- Files are UTF-8 with an optional BOM and CRLF. A timestamped record generally starts `YYYY-MM-DD HH:MM:SS.ffffff - [tag]: message`. Messages may contain HTML and untimestamped continuation lines. Preserve continuations with their parent record and source line numbers.
- Determine coverage from contents. Archive filenames reflect rotation time, not the start time. Runs may span files, and one archive may contain several programs and restarts.
- Locate `Starting Program:`, `Program State:`, stop requests and exceptions before attributing events to a run. `RUNNING` can precede the actual start banner; setup failures are not productive gameplay time.
- Tags are not severity levels. `[Console 0]` contains quest decisions and errors; `[Program]` also contains sound-detector noise. Do not use `[Program]` alone as an INFO filter.
- Suppress `issue_*()`, `wait_for_all()`, inference `Count = ... Mean = ...`, rendering/conversion timing, and repeated sound-candidate messages for an initial overview. Retain them when investigating timing or detection.
- BBQ anchors: `Quest:`, `Processing quests.`, `Quest not possible`, `Current quest exists on list`, `Quest completed successfully.`, `Failed to complete a quest`, `Did not enter battle`, radar-ball fallback, saves, exceptions and current stats. Include `String Match Result:` only in a narrowed quest-reading interval: other programs use it for unrelated OCR too.
- Quest OCR includes original text, normalized text, candidate, stable slug and score. Prefer slugs such as `(pickup-10)` over translated strings. `no matches` alone does not establish a missing bonus quest: inspect the slot detector and screenshot; an absent fourth slot may be normal.
- Count messages as messages, not unique quests, battles or rewards. Recursive fallbacks may log another `Quest:`; a quest may disappear incidentally. Stats repeat during audio callbacks and reset across runs. Deduplicate unchanged stats, segment by run and verify completion attribution before calculating success rates.
- Measure attempt time from quest start through verification/recovery. Preserve stopped or rotated attempts as censored, rather than marking them failures. Net BP/hour requires actual reward and reroll accounting, not just quest completion counts.

Update this skill with verified new formats and useful filters; separate source-confirmed behavior from hypotheses based on log wording.
