# M1 diagnostic run — initial validation

Started 2026-10-20 13:45:18.212747 PDT, local build 6214dc51d.
At 14:03:06 the run had lasted 17m48s and logged three requested-task completions.
Final STOPPED record: 14:04:10.198517, **18m52s** elapsed. Three requested-task
completions; catch-any's second attempt was interrupted and is censored.
Post-stop window inspection shows the Switch HOME screen, so no live quest-panel
balance was captured yet. The BP crop cannot be validated from that screenshot.

- Exact slugs, numbered attempts, limits, route/verification timings, and retry-exhaustion logs are present.
- Local screenshot writes report success. The catch-any entry-timeout image at 14:02:41 was opened and is a readable 1920x1080 overworld frame on the rock plateau, outside battle. It does not alone prove whether the intended target spawned.
- Raw BP OCR is **incorrect**: 66701, 66901, 67101, 67301, 67501. The user's measured checkpoint is **6710 after two completions**, matching the second completion at 13:54:31.786659. This suggests contamination of the crop (extra glyphs); do not blindly strip digits or use these readings as balances.
- The first two requested completions were make-tm and auto-10 (20 BP each), with no logged rerolls before the checkpoint. Under the existing ledger assumptions, starting BP was **6670**. This is inferred from the user's checkpoint, not validated OCR.
- During Sneak-up, the board changed from catch-any/sneak-up/pickup-10 to catch-any/sneak-up/auto-10 and raw BP rose. That suggests an incidental pickup completion. The requested Sneak-up stayed on the board and exhausted two attempts. Requested-task stats therefore undercount total board completions; reconcile actual board changes and balances.

Enough data is collected to end this diagnostic trial. M1 remains partially validated: BP extraction needs correction and direct screenshot comparison before reliable balance accounting. Keep reroll/retry behavior unchanged until that check. No need to wait for another full baseline hour.

## BP crop correction and next-run preflight

After stopping, the game was resumed from HOME and its quest panel opened without
starting the farmer. Window capture `/private/tmp/bbq-bp-panel.png` visibly shows
**6750 BP**, with catch-any, sneak-up and auto-10 outstanding. This is 80 BP above
the inferred 6670 start, consistent with three requested completions plus the
incidental 20-BP pickup completion. No reroll is logged in this run.

The old normalized crop ends at x=0.957 and includes the left vertical stroke of
the B suffix. The narrowed crop ends at x=0.954 (width 0.088 rather than 0.091),
preserving all four digits and excluding that stroke. Both BP read sites use the
corrected crop. Inspection of the old/new image crops confirms this; offline
Tesseract reads the narrowed screenshot crop as 6750. This is a different OCR
backend from the live PaddleOCR path, so live confirmation remains required.
Temporary crop evidence: `/private/tmp/bbq-bp-old.png` and `bbq-bp-new.png`.

The next run is a short BP validation, not another hour-long baseline: confirm
the first board reads 6750, then one completion or reroll yields the expected
balance change. Stop early if BP is still wrong. The farmer's spending, retry
policy and navigation remain unchanged. The preflight screenshot is a starting
checkpoint only if no manual play occurs before the run.
