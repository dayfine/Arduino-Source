# BBQ known issues

Open problems seen on the Switch or in review, newest evidence first. Each names the run that
showed it and the planned PR (see [PR_PLAN.md](PR_PLAN.md)) that fixes or risks it. Update after
every run; move fixed ones to "Fixed" with the run that proved it.

| ID | Quest / area | Issue | Evidence | Fix / owner PR | Severity |
|---|---|---|---|---|---|
| KI-1 | photo-fly | Back-off is blind: it walks back and re-locks whether or not a Geodude is near, so photos show rock and bush | R23: 1/5 attempts, 2 quests rerolled | G4: lock on first, back off only when locked and close | High (≈6 min/2 h) |
| KI-2 | photo-dark | Alolan Muk in frame, partly in the box, but the photo doesn't count; likely too small / off-centre | R23: 0/2, rerolled (22:39, 22:40) | G3a: aim or lock on before the photo | Medium |
| KI-3 | photo-swim | Photo shows empty water; nothing swimming in frame | R23: 0/2, rerolled (22:13, 22:14) | not planned yet; needs a look-around or a different spot | Medium |
| KI-4 | Pride Rock (5 quests) | Glide misses the rock some of the time; caught by the landing check and re-flown (+~1 min each) | R23: 2 misses then landed; rain: 6/7 fell | G1 (landing check); rain: KI-5 | Low now |
| KI-5 | all glides | Rain shortens glides (Pride Rock, Rabsca) | lab 2026-10-26: Rabsca 7,000 ms short 2/2 in rain | follow-up: rain detector → longer glide or defer (`RETRY_LATER`) | Medium |
| KI-6 | lock-on catches | Lock-on takes the nearest Pokémon; wrong species wastes a battle (Deerling, Sawsbuck at the Geodude spot) | lab: Geodude 1/3 with retries | A7 follow-up: read the lock label, switch target | Medium |
| KI-7 | relaunch | After "no overworld" at relaunch the fly retry pressed A on the map for 2 min | lab console 2026-10-26 | C2b / C4 recovery bounds | Low |
| KI-8 | Auto Battle | Let's Go kill-sound scores sit at 0.928–0.929 against a 0.93 threshold; a small drift changes kill counts | R23 log 22:02–22:04 | E chain: check before touching the threshold | Low |
| KI-9 | pickup-10 | Never completes passively (0 in 103 draws) | R10–R22 | rerolled on sight (F14) | n/a (by design) |
| KI-10 | settings | `RETRY_LATER` exists but is off in the run profile (`NUM_RETRIES` 1, reroll) | profile bbq-r19 | follow-up with KI-5 | Low |
| KI-11 | code | `Terarium.cpp` keeps a global `away_from_plaza` | lab/r10 | C4 position object | Review risk |
| KI-12 | code | `BlueberryQuests.cpp` 2.4k, `BlueberryCatchPhoto.cpp` 1.4k, `PokemonSV_Tests.cpp` 3.4k lines on lab; limit 1,000 | lab/r10 | P1–P3 | Review risk |
| KI-13 | detector | `MapDestinationMenuDetector` reads Polar snow as the destination menu; upstream AutoStory 28–30 already works around it by zooming out | R17: Polar Classroom 1 direct flights 0/4 | A1 | Medium |

## Fixed (kept for regression checks)

| ID | Issue | Fixed by | Proved in |
|---|---|---|---|
| KF-1 | Photo mode moves the camera to the player, so the Pyroar was off-frame | Pyroar aim inside photo mode (lab b133298ea) | lab 3/3; R23 photo-savanna 3/4 |
| KF-2 | Lock-on can miss silently and send the lead out in Let's Go | recall with R + retry (lab f20d06c7b) | R23 catch-rock 1/1 |
| KF-3 | Scripts called `central` at Central → "Unable to fly" stalls | console `central` no-op at Central | lab |
| KF-4 | Psychic photo while still riding | dismount first (lab f20d06c7b) | needs a run |
| KF-5 | Fatal error report crashed the app when the exception had no stream | A2 | 5 crashes on 2026-10-23, none since |
