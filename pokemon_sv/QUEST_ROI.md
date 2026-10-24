# BBQ quest ROI

Per-quest measurements from every lab run (all `bbq-*` profiles), to decide which
quests to run and which to reroll. Updated 2026-10-23 after R12b. Regenerate with
`tools/quest_roi.py` over the profile logs.

**Camera Support matters.** Runs before R10 13:16 had in-game Camera Support ON;
the timed routes then often miss (sneak-up 3/33 vs 11/13 after). "Camera off" is the
current setup; the table uses it when there are 2+ attempts, else the older data.

- BP: quest reward (Bulbapedia table, see BP_LEDGER_2026-10-21.md). Every completion
  also fills the 10-completion bar (+110 per 10, ~+11 each), not included here.
- s/attempt: from "Doing quest" to the board check, including the trip back.
- BP/min of quest time = BP x success / (s per attempt / 60).

| Quest (route) | BP | Camera on | Camera off | s/attempt | Success | BP/min of quest time | From |
|---|---:|---:|---:|---:|---:|---:|---|
| Make a singleplayer sandwich | 130 | 8/8 | 6/6 | 198 | 100% | 39.4 | camera off |
| Swimming/Water/Polar | 30 | 24/39 | 15/16 | 62 | 94% | 27.2 | camera off |
| Steel | 30 | - | 1/1 | 76 | 100% | 23.7 | camera off |
| Canyon/Ghost/Ground | 30 | 8/10 | 3/3 | 78 | 100% | 23.1 | camera off |
| Coastal/Grass/Dragon | 30 | 10/12 | 1/1 | 75 | 83% | 20.0 | camera on |
| Steel | 40 | - | 1/1 | 136 | 100% | 17.6 | camera off |
| Ghost/Ground | 40 | 3/4 | 2/2 | 140 | 100% | 17.1 | camera off |
| In-flight/Psychic | 30 | 4/6 | 4/7 | 67 | 57% | 15.4 | camera off |
| Travel 500 meters | 20 | 41/41 | 13/13 | 87 | 100% | 13.8 | camera off |
| Normal/Fire | 40 | - | 1/1 | 186 | 100% | 12.9 | camera off |
| Hatch an Egg | 200 | 3/3 | - | 972 | 100% | 12.3 | camera on |
| Rock/Electric | 40 | 3/5 | 2/4 | 113 | 50% | 10.6 | camera off |
| Grass/Dragon | 40 | 4/6 | 5/7 | 164 | 71% | 10.5 | camera off |
| Fairy | 40 | 4/6 | - | 159 | 67% | 10.1 | camera on |
| Bug | 40 | 2/3 | - | 168 | 67% | 9.5 | camera on |
| Sneak up | 20 | 3/33 | 11/13 | 112 | 85% | 9.1 | camera off |
| Water/Ice | 40 | 1/1 | 3/5 | 180 | 60% | 8.0 | camera off |
| Defeat a wild tera | 20 | 3/3 | 1/1 | 159 | 100% | 7.5 | camera on |
| Psychic | 40 | 1/3 | - | 120 | 33% | 6.7 | camera on |
| Make TM | 20 | 32/33 | 12/17 | 139 | 71% | 6.1 | camera off |
| Give your pokemon a bath! | 100 | 2/8 | - | 292 | 25% | 5.1 | camera on |
| Any/Normal/Fire | 40 | 9/36 | - | 149 | 25% | 4.0 | camera on |
| Tera-self and defeat any wild | 20 | 29/51 | 7/15 | 152 | 47% | 3.7 | camera off |
| Any | 20 | - | 2/4 | 169 | 50% | 3.6 | camera off |
| Fighting | 40 | 1/4 | 1/1 | 180 | 25% | 3.3 | camera on |
| Auto Battle | 20 | 49/51 | 10/18 | 222 | 56% | 3.0 | camera off |
| Normal/Fire | 30 | 3/25 | 1/1 | 96 | 12% | 2.2 | camera on |
| Poison/Dark | 40 | 0/3 | 0/2 | 163 | 0% | 0.0 | camera off |
| Tera Raid | ? | 1/1 | 1/1 | 518 | 100% | ? | camera on |

Caveats: most rows have fewer than 10 attempts. The camera-off Auto Battle row
includes 8 pick-up-10 attempts run as Auto Battle in R10 (kills don't count toward
pickups); on the current build Auto Battle is 7/7 at ~240 s (5.0 BP/min). Tera-self
camera-off includes R10's canyon glide (1/6); at the Chargestone court it is 6/9 at
~140 s (5.7 BP/min). Make TM camera-off includes two stuck 12-minute scans fixed in
efa64d507; after the fix 2/2 at ~120 s (~10 BP/min). Catch Poison/Dark (Alolan Muk,
Coastal glide) has **never** succeeded: 0/5.

## Run or reroll

The program averages ~700 BP/h (~11.7 BP/min) on the current build, including
travel, saves and rerolls. A reroll costs 10 BP (50 for red) plus ~25 s, about
10 + 25/60 x 11.7 = ~15 BP-equivalent for blue. So, for a quest with reward R
(+11 bar bonus), success rate p and time t minutes per attempt:

    run if  (R + 11) x p  -  t x 11.7  >  -15     (blue; use -55 for red)

| Quest | (R+11)p - 11.7t | Decision |
|---|---:|---|
| Catch Poison/Dark | 0 - 32 = -32 | reroll |
| Catch Any (court) | 15.5 - 33 = -17 | borderline reroll |
| Auto Battle (current) | 31 - 47 = -16 | borderline reroll |
| Tera-self (court) | 21 - 27 = -7 | run |
| Make TM (fixed) | 31 - 23 = +8 | run |
| Sneak up | 26 - 22 = +4 | run |
| Photo in-flight | 23 - 13 = +10 | run |
| Photos (swim/canyon/steel) | ~39 - 13 = +26 | run |

## Proposal: exclusions from a threshold

Instead of hand-picking exclusion rows, a "Reroll quests below N BP/min" option
(default ~5) would reroll any quest whose measured BP/min is under N. The
per-quest numbers would ship as a table in the program (like the existing
`expected_seconds_per_success()` in PokemonSV_BlueberryQuests.cpp, which already
orders quests by cost), with explicit exclusion rows still overriding it. Quests
the program cannot do stay "always rerolled".

## Marginal effects and the moving threshold

`tools/reroll_threshold.py` is the renewal-reward model behind the rule above. A
draw of quest i earns B_i and takes T_i; a reroll earns -C and takes TAU and gives
a new draw. The long-run rate of a reroll set S is

    lambda(S) = sum_i q_i B_i(S) / sum_i q_i T_i(S)

Rerolling i beats attempting it iff B_i - lambda* T_i < -(C + lambda* TAU), where
lambda* is the rate of the *optimal* policy, not the current one. Rerolling a group
raises lambda, which raises the bar for every other quest, so the set is found by
iterating (Dinkelbach) until it stops changing. The cost side of a group shows up
as rerolls per kept quest = Q/(1-Q), where Q is the share of draws rerolled.

Current-build inputs (2026-10-23):

```
baseline (reroll only impossible): 11.37 BP/min = 682 BP/h of quest time
add to reroll set        q  BP/min  delta  value - lambda*t
catch-poison-dark     0.01   11.74  +0.36       -76.5 vs -14.7
catch-any             0.05   11.67  +0.30       -28.5 vs -14.7
auto-10               0.11   11.36  -0.01       -14.5 vs -14.7
tera-self-defeat      0.09   11.14  -0.23        -9.5 vs -14.7
catch-normal-fire     0.02   11.25  -0.12        -0.8 vs -14.7
sneak-up              0.07   10.70  -0.67        +5.6 vs -14.7
catch-typed           0.14    9.87  -1.50        +5.7 vs -14.7
make-tm               0.08   10.58  -0.79        +7.0 vs -14.7
travel-500            0.12    9.79  -1.59       +14.5 vs -14.7
photo-savanna         0.04   10.84  -0.53       +15.5 vs -14.7
photo-other           0.03   10.83  -0.54       +22.8 vs -14.7
photo-swim            0.07    9.99  -1.38       +28.3 vs -14.7

optimal reroll set: ['auto-10', 'catch-any', 'catch-poison-dark'] (+ impossible)
rate 12.22 BP/min; P(draw rerolled) = 0.32; rerolls per kept quest = 0.47
```

Alone, auto-10 is neutral (-0.01 BP/min); once catch-any and Poison/Dark are
rerolled, the rate rises and auto-10 falls below the new bar. The optimal set
(auto-10, catch-any, Poison/Dark, plus the impossible quests) rerolls 32% of blue
draws, 0.47 rerolls per kept quest, for +0.85 BP/min (+7%, ~+50 BP/h) over
rerolling only the impossible ones. Quest time only: saves, round trips and reds
are left out, and most inputs have fewer than 10 attempts.

## Monte Carlo (current build, 2026-10-23)

`tools/mc_current.py`: 2-hour runs on the inputs above, reds after every 10 completions, 33 s of overhead per completion fitted to R11/R12/R12b (660/725/676 BP/h).

```
scenario                                     median   p10   p90  (BP/h per 2-hour run, 3000 sims)
current: wash + impossible rerolled             690   546   808
+ Poison/Dark rerolled                          704   559   817
+ optimal set (also catch-any, auto-10)         725   597   837
  + Make TM look-ahead (120 -> 95 s)            731   595   847
  + photo-flying doable (guess 50%, 120 s)      734   601   856
  + Poison/Dark route fixed (guess 60%)         734   597   853
```

The spread between 2-hour runs (p10-p90, about +-130 BP/h) is wider than any single change, so one run cannot confirm a +15-35 BP/h item; compare several.

## Small samples (tools/reroll_confidence.py)

Each success rate is a Beta(1+successes, 1+failures) posterior. For each quest the model samples the rate, recomputes the optimal rate lambda* and the margin of attempting over rerolling; P(run better) is the share of samples where attempting wins. A quest between 0.2 and 0.8 is undecided at its sample size, so one or two more attempts are worth more than acting on the point estimate. Times are taken as known; their spread is not modelled yet.

```
quest                  n  ok  mean  P(run better)  margin 10-90%
travel-500            15  15  0.94           1.00   +25.3 ..  +28.2   run
make-tm               17  12  0.68           0.95    +2.3 ..  +15.7   run
auto-10                9   9  0.91           0.00   -15.4 ..   -3.2   reroll
tera-self-defeat      13   6  0.47           0.11   -24.6 ..   +0.2   reroll
photo-swim            16  15  0.89           1.00   +38.5 ..  +43.0   run
photo-other           10   8  0.75           1.00   +25.5 ..  +39.8   run
sneak-up               9   9  0.91           1.00   +17.5 ..  +22.7   run
catch-any              4   1  0.33           0.02   -56.4 ..   -8.6   reroll
catch-normal-fire      2   2  0.75           0.81   -13.4 ..  +26.3   run
photo-savanna          2   2  0.75           0.98   +14.2 ..  +38.4   run
catch-typed           24  16  0.65           0.98    +6.9 ..  +20.8   run
catch-poison-dark      5   0  0.14           0.01   -64.4 ..  -19.6   reroll
```

Notes: tera-self counts are mostly the Chargestone court (6/13); the Pride Rock route is 1/1 so far. Auto-10 succeeds 9/9 but loses on time (~250 s), so its verdict depends on the time estimate, which this does not sample. Rare quests (about 1% of draws each) are not worth chasing for data alone: R14 took ~26 rerolls (~260 BP) per rare quest seen.

## Draw rates (Bulbapedia, advanced difficulty)

Bulbapedia's Blueberry Quest table gives each quest a rate per difficulty pool. This save
draws from the advanced pool. Blue: sneak-up 12%, auto-10 12%, travel 13%, pick-up 12%,
Make TM 5%, catch any 5%, photo in flight 6%, photo swimming 6%, Tera-self 9%, each biome
photo 1.5%, typed catch 14% in total (0.78% per type). Bonus (red): wash 8%, wild Tera 15%,
auto-30 9%, three-ingredient sandwich 15%, Tera raid 15%, each flavour sandwich 3%, typed
photo 15% in total (0.83% per type), hatch 8%. Rewards: blue 20 (photos and Tera-self 30,
typed catches 40), red 100-200.

Our 584 logged blue draws agree within the noise, except Make TM (9.4% vs 5%) and typed
catches (10.1% vs 14%).

Sampling the typed catches by rerolling everything else: each is 0.78% of blue draws, so
3000 BP (~300 draws) sees a given one with 90% chance and all four of
flying/fighting/bug/psychic with 67%; 5000 BP gives 98% and 92%. A typed photo (red)
is 0.83% of red draws, and reds come once per 10 completions, so the red Flying-type photo
cannot be sampled this way; it stays unsampled until it shows up in normal runs.
