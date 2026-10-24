# Where BBQ run time goes (R11-R13)

Measured 2026-10-23 over R11, R12, R12b and R13: 344 minutes, 91 completed quests.
Tools: `tools/flights.py`, `tools/board_reads.py`, and the return timing below.

| Activity | Count | Mean | Total | Share of run |
|---|---:|---:|---:|---:|
| Return to Central Plaza (map, find the Pokemon Center icon, fly, banner check) | 116 | 29.7 s | 57 min | **17%** |
| Fly out to a quest (map cursor move from the plaza, fly) | 112 | ~18 s | 34 min | 10% |
| - of which Chargestone (Auto Battle passes, catch-any, Tera-self court) | 48 | 16.9 s | 14 min | 4% |
| Quest board reads (OCR of 3-4 slots) | 254 | 3-7 s | 25 min | 7% |
| Day skip after a battle (Home, date, roll, resume) | ~69 | ~13 s | 15 min | 4% |
| Heal + re-fly after a battle (before b46660d3a: every battle) | ~25 | ~20 s | ~8 min | 2% |
| Save and reset every 10 quests | 7 | ~70 s | 8 min | 2% |

Travel is about **27%** of the run. One return, traced (R12b 18:50:35): map open 4 s,
**10 s finding the Pokemon Center icon**, fly 13 s, banner 0 s, then two board reads in a
row: "after attempt" (4 s) and, right after it with nothing in between, "before dispatch"
(4 s).

## Inefficiencies and fixes

1. **Every quest returns to the plaza (17%).** Fly straight to the next quest's fly point
   instead (fork #20 flies to a named fly point from anywhere), and retry at the spot.
2. **Auto Battle flies a round trip between passes** even at full HP (about 20 extra round
   trips, ~47 s each, ~16 min). Staying at the court needs a test: the round trip may be
   what respawns the area.
3. **The board is read twice in a row** after a trip quest: "after attempt" then "before
   dispatch" (25 of 103 reads in R12b). Reusing the after-attempt read, as no-trip quests
   already do, saves ~4 s per quest.
4. **Finding the Pokemon Center icon takes ~10 s** per return.
5. **Day skip after every battle (4%)** is only needed to respawn a fixed target.
6. Done in b46660d3a: heal every third battle; most BP per second first; retry later.
