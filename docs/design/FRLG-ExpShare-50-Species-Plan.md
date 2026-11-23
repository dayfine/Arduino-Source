# FRLG AutoStory: Interleaving 50 Pokédex Species for Exp. Share (Single-Cartridge)

Written 2026-11-22. Companion to [AutoStory-PostStory-Abstractions.md](AutoStory-PostStory-Abstractions.md) ([PR #804](https://github.com/dayfine/Arduino-Source/pull/804)) and `PokemonFRLG:MailGlitchActivation` ([PR #781](https://github.com/dayfine/Arduino-Source/pull/781)).

## 1. Goal & Scope

In Pokémon FireRed and LeafGreen, **Exp. Share** is awarded by **Professor Oak's Aide on 2F of the Route 15 Gatehouse** (`Route15_WestEntrance_2F`) once the player has **50 or more distinct Pokémon species registered as Caught ("Owned")** in the Pokédex.

This document specifies:
1. The **earliest point** in the existing FRLG AutoStory route (`01.01`–`20.05`) where 50 species can be owned **without external link trading** (covering both FireRed and LeafGreen, with and without in-game NPC trades).
2. A **low-detour, high-encounter-rate interleaved plan** aligned to existing `StoryData/` checkpoints, plus a comparison between **Milestone A (earliest possible, pre-Koga)** and **Milestone B (lowest engine effort, zero fishing / zero Safari catching)**.
3. **Ball, money, time, PC-box, and shiny-safety engineering constraints**, and the exact AutoStory engine upgrades required.
4. **Strategic ROI**: when interleaving Exp. Share pays off (Victory Road 2F Mail Glitch grinding, National Dex 60-species unlock, post-game training) and why it must stay **opt-in** rather than part of the core Hall-of-Fame speedrun path.

---

## 2. Baseline FRLG AutoStory Route & Physical Access to Route 15 Gatehouse 2F

### Baseline Owned Species in Current AutoStory (`01.01`–`20.05`)

| Stage | Checkpoints | Maps / Key Milestones | Species Already Owned |
|---|---|---|---|
| Act 1–2 | `01.01`–`04.06` | Pallet → Viridian Forest → Brock → Rt 3/4 → Mt. Moon → Cerulean | **Squirtle** (`01.06`), **Magikarp** (`04.03`, boxed in Box 1 `04.04`); also collects **Moon Stone #1** + **Helix Fossil** (`04.05`) |
| Act 3 | `05.01`–`05.10` | Cerulean → Misty → Nugget Bridge → Bill → Rt 24 → Vermilion | **Rattata** (`05.01`), **Wartortle** (`05.02`), **Oddish** (`05.09`, FR-only stopgap; LG Rt 24 has Bellsprout); farms **10× Nuggets = ₽50,000** (`05.04`) |
| Act 4–5 | `06.01`–`10.06` | S.S. Anne (Cut) → Surge → Bike → Rock Tunnel → Celadon → Tower | **Pidgey** (`08.03`), **Eevee** (`09.04`, Gift RNG); also collects **Moon Stone #2** (`09.02`), **Tea** (`09.03`), **Fly** (`10.01`), **Poké Flute** (`10.06`) |
| Act 6 | `11.01`–`12.03` | Rt 16 Snorlax → Cycling Road → **Fuchsia City** (`11.02`) → Safari (`11.03`) → Koga (`12.02`) → Cinnabar | **Blastoise** (Lv 36 around `11.01`–`12.01`), **HM03 Surf** (`11.03`), **HM04 Strength** (`12.02b`) |
| Act 7–8 | `13.01`–`20.05` | Mansion → Blaine → Erika → Saffron → Silph Co. → Viridian → Sabrina → E4 | **Lapras** (`16.04`, Gift RNG), **Master Ball** (`16.05b`), Hall of Fame (`20.05`) |

- **Current baseline**: only **8 owned species** at Fuchsia (`11.02`–`12.01`) and **9** by Silph Co. (`16.04`). Reaching Exp. Share requires **41–42 additional distinct species**.

### Physical Access to `Route15_WestEntrance_2F`
- Verified from `FuchsiaCity.json` and `Route15_WestEntrance_1F.json`: `FuchsiaCity` connects directly east into `Route15` → `Route15_WestEntrance_1F` (stairs at `[9, 10]` → `2F`).
- **Zero trainers, zero Cut trees, zero Surf, and zero Soul Badge** are required to reach Oak's Aide once inside Fuchsia City.
- The only story gates to reach Fuchsia via Cycling Road are **Boulder + Cascade** (Cut for Rock Tunnel) → **Silph Scope** (`09.02`) → **Poké Flute** (`10.06`) + **Bicycle** (`07.01`).
- Therefore, Oak's Aide is **first physically reachable at checkpoint `11.02`** (3 badges, pre-Koga).

---

## 3. Two Target Milestones: Pre-Koga vs. Zero-Safari / Zero-Fishing

Both FireRed and LeafGreen comfortably clear **50–55+ species** without external trading at either milestone:

| Dimension | Milestone A: Earliest Possible (`11.03`, Pre-Koga) | Milestone B: Lowest Engine Effort (`14.02b`–`16.04`, Recommended) |
|---|---|---|
| **Story point** | First Fuchsia arrival, right after Safari Zone HM03 (`11.03`) | After Cinnabar Lab + Saffron (`13.01`–`16.04`), before Viridian/Sabrina/E4/Mail Glitch |
| **Badges / HMs needed** | Boulder + Cascade, Cut, Fly, Poké Flute, Bicycle | + Soul Badge + Surf (`12.02`) |
| **FR & LG species reachable** | **50** using only `>=10%` wilds + 2 NPC trades; **52–55 pure no-trade** | **50–54 pure no-trade** (**52–56** with `CH'DING` + `MIMIEN`) |
| **New capture mechanics** | Needs **Safari-zone battle capture** + **Good Rod fishing** + `Route15_WestEntrance_2F` room | **Zero fishing and zero Safari catching** — reuses only grass/cave/Surf `capture`, Game Corner prizes, fossils, gifts, and stone evos |
| **National Dex progress** | 50/60 (83% of Oak's 60-species Sevii 4–7 / Mewtwo gate) | 50/60 (83% of National Dex, ready to finish 60 right after E4) |

---

## 4. Interleaved 50-Species Route (FireRed & LeafGreen)

### Part A — Shared On-Route Backbone Through `11.02` (37 Species, Zero Safari, Zero Fishing, Zero Trades)

All wild catches below have **`>= 10%` encounter rate** and high catch rates (avoiding low-CR full-HP traps like Snorlax CR 25, Ditto CR 35, or Onix CR 45). Expected Poké Ball counts assume **full HP** (`ConsoleCaptureActions` currently does not weaken wilds).

| Cum. | Checkpoint | Location (Detour?) | FireRed | LeafGreen | Rate (FR / LG) | CR (~PB full HP) | Source |
|---:|---|---|---|---|---|---|---|
| 1–3 | `01.06`–`12.01` | Oak's Lab / Story | **Squirtle → Wartortle → Blastoise** | same | Gift + Lv 16 / 36 | — | Already in AutoStory *(if Lv 35 at `11.02`, hits Lv 36 by `12.01`)* |
| 4 | `02.03` or `08.01` | Rt 22 *(10-tile west detour)* or Rock Tunnel | **Mankey** | same | 45% (Rt 22) / 15% (RT) | 190 (~3.6) | Grass / Cave |
| 5 | `03.01` | Viridian Forest *(on-route)* | **Caterpie** | same | 40% / 40% | 255 (~3.0) | Grass |
| 6 | `03.01` | Viridian Forest *(on-route)* | **Weedle** | same | 40% / 40% | 255 (~3.0) | Grass |
| 7 | `03.01` | Viridian Forest *(on-route)* | **Kakuna** | **Metapod** | 10% / 10% | 120 (~6.0) | Grass *(opposite cocoon is 5%)* |
| 8 | `04.01` | Route 3 *(on-route)* | **Spearow** | same | 35% / 35% | 255 (~3.0) | Grass *(also `CH'DING` trade fodder)* |
| 9 | `04.01` | Route 3 *(on-route)* | **Nidoran♂** | **Nidoran♀** | 14% / 14% | 235 (~3.0) | Grass *(version-dominant gender)* |
| 10 | `04.01` | Route 3 *(on-route)* | **Jigglypuff** | same | 10% / 10% | 170 (~4.3) | Grass *(Moon Stone evo target)* |
| 11 | `04.03` | Route 4 Center *(on-route)* | **Magikarp** | same | Gift (₽500) | — | Already in AutoStory |
| 12 | `04.05` | Mt. Moon 1F *(on-route)* | **Zubat** | same | 69% / 69% | 255 (~3.0) | Cave |
| 13 | `04.05` | Mt. Moon 1F *(on-route)* | **Geodude** | same | 25% / 25% | 255 (~3.0) | Cave |
| 14 | `04.05` | Mt. Moon B1F *(on-route)* | **Paras** | same | **100%** / **100%** | 190 (~3.6) | Cave *(Mt. Moon B1F is 100% Paras!)* |
| 15 | `05.01` | Route 4 grass *(on-route)* | **Rattata** | same | 35% / 35% | 255 (~3.0) | Already in AutoStory |
| 16 | `05.01` | Route 4 grass *(same tile)* | **Ekans** | **Sandshrew** | 25% / 25% | 255 (~3.0) | Grass *(zero extra travel)* |
| 17 | `05.09` | Route 24 grass *(on-route)* | **Oddish** | **Bellsprout** | 25% / 25% | 255 (~3.0) | Already in AutoStory *(generalize `05.09` for LG)* |
| 18 | `05.10` | Route 5 / Route 6 *(on-route)* | **Meowth** | same | 35% / 35% | 255 (~3.0) | Grass |
| 19 | `06.04` | Route 11 *(short east detour from Vermilion)* | **Drowzee** | same | 25% / 25% | 190 (~3.6) | Grass (`Route11.json` already in `Packages/`) |
| 20 | `06.04` | Diglett's Cave *(adjacent to Rt 11)* | **Diglett** | same | **95%** / **95%** | 255 (~3.0) | Cave |
| 21 | `07.02` | Route 10 north *(on-route)* | **Voltorb** | same | 40% / 40% | 190 (~3.6) | Grass |
| 22 | `08.01` | Rock Tunnel 1F/B1F *(on-route)* | **Machop** | same | 15% / 15% | 180 (~3.6) | Cave |
| 23 | `08.03` | Route 8 *(on-route)* | **Growlithe** | **Vulpix** | 20% / 20% | 190 (~3.6) | Grass *(20% on Rt 8 vs. 10% on Rt 7)* |
| 24 | `08.03` | Route 7 *(on-route)* | **Pidgey** | same | 30% / 30% | 255 (~3.0) | Already in AutoStory |
| 25 | `09.01` | Celadon Game Corner *(on-route)* | **Abra** | same | 180c / 120c | 0 balls | Prize (`buy_coins` + `redeem_prize_mon`; skips Teleport RNG) |
| 26 | `09.01` | Celadon Game Corner *(same room)* | **Clefairy** | same | 500c / 750c | 0 balls | Prize (₽10k FR / ₽15k LG; skips 1–6% Mt. Moon) |
| 27 | `09.04` | Celadon Mansion roof *(on-route)* | **Eevee** | same | Gift | 0 balls | Already in AutoStory *(keep un-evolved if Gift RNG'd)* |
| 28 | `10.01` | Celadon Dept Store 4F Fire Stone (₽2,100) | **Arcanine** | **Ninetales** | Stone evo on #23 | 0 balls | Zero RNG |
| 29 | `10.01` | Moon Stone #1 *(already collected in `04.05`)* | **Clefable** | same | Stone evo on #26 | 0 balls | Zero RNG |
| 30 | `10.01` | Moon Stone #2 *(already collected in `09.02`)* | **Wigglytuff** | same | Stone evo on #10 | 0 balls | Zero RNG |
| 31 | `10.05` | Pokémon Tower 3F–7F *(on-route)* | **Gastly** | same | 75–90% | 190 (~3.6) | Tower |
| 32 | `10.06` | Pokémon Tower 7F *(on-route)* | **Cubone** | same | 10% (7F) | 190 (~3.6) | Tower |
| 33 | `10.06` | Pokémon Tower 7F *(same floor)* | **Haunter** | same | 15% (7F) | 90 (~8.1 PB / ~5.1 GB) | Tower |
| 34 | `11.01` | Route 16 north grass *(Cut already done in `10.01`)* | **Doduo** | same | 35% / 35% | 190 (~3.6) | Grass |
| 35 | `11.02` | Route 18 grass *(on-route into Fuchsia)* | **Raticate** | same | 15% / 15% | 127 (~6.0 PB / ~3.6 GB) | Grass |
| 36 | `11.02` | Route 18 grass *(same patch)* | **Fearow** | same | 15% / 15% | 90 (~8.1 PB / ~5.1 GB) | Grass |
| 37 | `11.02` | Route 15 grass *(right outside Gatehouse)* | **Venonat** | same | 30% / 30% | 190 (~3.6) | Grass |

**Subtotal at `11.02`: 37 species in both FireRed and LeafGreen.**

---

### Part B — Closing 37 → 50+ Under Milestone A (`11.03`, Pre-Koga)

| Add'l (Cum.) | Checkpoint | Location / Method | FireRed | LeafGreen | Notes |
|---:|---|---|---|---|---|
| +6 (**43**) | `11.03` | Safari Zone *(already entered for ₽500 & 30 Safari Balls)* + Celadon 4F Leaf Stone (₽2,100) | **Exeggcute** (20%) → **Exeggutor** (Leaf Stone), **Nidoran♀** (20–30%), **Rhyhorn** (20%), **Nidorino** (10%), **Nidorina** (10%) | Same (**Nidoran♂** 20–30% as opposite gender) | All `>=10%`; Safari Ball = 1.5× catch bonus. Optional **+1 (44)** using hidden Moon Stone #3 in Mt. Moon B2F (`04.05`) → **Nidoking / Nidoqueen**. |
| +4 (**47–48**) | `11.03` | **Good Rod** (`FuchsiaCity_House2`, zero badges/Surf) — fish Fuchsia pond + Fly Vermilion dock | **Poliwag** (60%), **Goldeen** (20%), **Horsea** (60%), **Krabby** (20%) | **Poliwag** (60%), **Goldeen** (20%), **Krabby** (60%), **Horsea** (20%) | CR 225–255 (~3.0 PB each); identical 4 species across both versions. |
| +2 (**49–50**) | `06.04` / `09.01` | **In-game NPC Trades** *(already automated in PR #781)* | **Farfetch'd** (`CH'DING`, Spearow in Vermilion) + **Mr. Mime** (`MIMIEN`, Abra on Rt 2) | same | Zero RNG, 0 balls. With Moon Stone #3 hits **50 at `11.03` with zero `<10%` encounters**. |
| +2 to +7 *(No-Trade Margin)* | Pre-Koga | **Zero-Trade Finishers** *(pick any 2–3 if forbidding even NPC trades)* | • **Hitmonlee/chan** (+1, Saffron Fighting Dojo — Tea is already obtained in `09.03`)<br>• **Pikachu** (5% Viridian Forest) + Thunderstone → **Raichu** (+2)<br>• **Gloom** FR / **Weepinbell** LG (5% Rt 15, or Lv 22–26 Oddish/Bellsprout + 1 Rare Candy) + Leaf Stone → **Vileplume / Victreebel** (+2)<br>• **Metapod** FR / **Kakuna** LG (+1, 5% Viridian Forest)<br>• **Pidgeotto** (+1, 5% Rt 15) | Identical counts in LG | Pre-Koga pure no-trade ceiling exceeds **55 easy species** in both versions. |

---

### Part C — Closing 37 → 50+ Under Milestone B (`14.02b`–`16.04`, Zero Safari / Zero Fishing — Recommended)

Skip Safari-zone catching and Good Rod completely; take the **37** from Part A, then add **7 zero-RNG / 100%-rate / on-route Post-Koga species**:

| Add'l (Cum.) | Checkpoint | Location / Method | FireRed | LeafGreen | Rate / Cost |
|---:|---|---|---|---|---|
| +1 (**38**) | `12.03` | Route 21 Surf *(on-route to Cinnabar)* | **Tentacool** | same | **100%** (CR 190) |
| +1 (**39**) | `12.03` | Route 6 Surf *(1 Fly to Vermilion)* | **Psyduck** | **Slowpoke** | **100%** (CR 190) |
| +1 (**40**) | `13.01` | Cinnabar Lab *(Helix Fossil already in bag from `04.05`)* | **Omanyte** | same | **Zero RNG, 0 balls** |
| +1 (**41**) | `13.01` | Pokémon Mansion *(on-route)* | **Koffing** | **Grimer** | **30%** (CR 190) |
| +1 (**42**) | `13.01` | Cinnabar Lab *(Old Amber from Pewter Museum back door w/ Cut)* | **Aerodactyl** | same | **Zero RNG, 0 balls** |
| +1 (**43**) | `15.01` | Saffron City Fighting Dojo *(adjacent to Saffron Gym)* | **Hitmonlee/chan** | same | **Zero RNG, 0 balls** |
| +1 (**44**) | `16.04` | Silph Co. 7F *(already in AutoStory)* | **Lapras** | same | **Already in AutoStory** |

From **44**, add any **6** from the following high-reliability finishers (all zero-fishing, zero-Safari):
1. **In-game NPC trades** (`CH'DING` + `MIMIEN`): **+2** (zero RNG; already needed for Mail Glitch)
2. **Rt 15 `Oddish`/`Bellsprout` (Lv 22–26) + 1 Rare Candy → `Gloom`/`Weepinbell` + Leaf Stone → `Vileplume`/`Victreebel`**: **+2** (zero 5% RNG!)
3. **Viridian Forest `Pikachu` (5%) + Thunderstone → `Raichu`**: **+2**
4. **Viridian Forest opposite cocoon (5%)**: **+1**
5. **Pokémon Mansion 5% `Grimer` (FR) / `Koffing` (LG)**: **+1**
6. **Route 15 `Pidgeotto` (5%)**: **+1**
7. **Route 21 `Tangela` (100% grass patch south of Pallet, CR 45 — use Great Balls)**: **+1**

Result: **50–54 pure no-trade species (52–56 with NPC trades) by `16.04`** with **zero Safari-battle code and zero Fishing code**, followed by a 15-second Fly to Fuchsia → walk east into `Route15_WestEntrance_2F` to collect **Exp. Share**.

---

## 5. Resources, Hazards & Automation Overhead

### Poké Ball, Money & Time Budget
- **Full-HP capture expectations**:
  - `CR 255 / 235 / 225`: 33.7% per Poké Ball (~3.0 balls)
  - `CR 190 / 180`: 28.0% (~3.6 balls)
  - `CR 170`: 23.4% (~4.3 balls)
  - `CR 127 / 120`: 16.8% (~6.0 PB / ~3.6 Great Ball)
  - `CR 90` (`Haunter`, `Fearow`): 12.3% (~8.1 PB / ~5.1 Great Ball)
- **Ball budget** (24–28 ball-caught species): expected **~95–107 Poké Balls**; safe 1.45× budget = **~145–155 Poké Balls (~₽29,000–31,000)**.
- **Game Corner + Stones + Safari**:
  - Coins: Abra (`200c` = ₽4,000) + Clefairy (`500c` FR = ₽10,000 / `750c` LG = ₽15,000) → **₽14,000 (FR) / ₽19,000 (LG)**.
  - Celadon 4F Evolution Stones: 2–4 stones @ ₽2,100 = **₽4,200–8,400** (Moon Stones are free in `04.05` & `09.02`).
  - Safari Zone entry: ₽500 (already paid in `11.03`).
- **Total extra cash**: **~₽48,000 (FR) / ~₽53,000 (LG)** — cleanly covered by bumping `05.04` Nugget Farm from `10` → `13–14` Nuggets (₽65k–70k).
- **Time overhead**:
  - Sequential single-species `capture`: ~120 wild encounters (~48 min) + ~10–15 min detour/mart/prize/stone overhead = **~60–65 min**.
  - With multi-species **`capture_any`** on shared patches (Viridian Forest, Rt 3, Mt. Moon 1F, Rt 4, Tower 7F, Rt 18): cuts fled encounters ~35% → **~45–50 min**.

### PC Box Space & Existing AutoStory Slot Hazards
- **Gen 3 auto-overflow**: Once the 6-slot party is full, catches go directly to Box 1 (30 slots) and **automatically roll over into Box 2** (up to 14 boxes × 30 = 420 slots; never blocks ball throws).
- **`05.04` Nugget Farm PC-slot hazard**: `04.04` deposits Magikarp into Box 1, and `05.04` currently uses fixed `deposit(box=1, slot=1)` / `withdraw(box=2, slot=1)`. Only ~11 catches occur before `05.04` (so Box 1 will not overflow before `05.04`), **but** boxed catches will occupy early Box 1 slots. Before interleaving pre-`05.04` catches, either:
  1. switch `05.04`'s PC step to species-based lookup (as `20.01a` already does for `magikarp`), or
  2. defer `03.01`–`05.01` catches until after `05.04` (slight backtrack cost via Cerulean/Viridian Fly later).

### Shiny Safety
- `ConsoleCaptureActions::read_encounter()` already routes through `handle_encounter(..., true)`, throwing `ProgramFinishedException` on any shiny **without reloading**. Over ~120 extra encounters, full-odds shiny probability is ~1.45% and 100% preserved.

---

## 6. AutoStory Engine Fit & Required Upgrades

Most building blocks already exist (`capture` in `PokemonFRLG_AutoStoryCaptureStep.h`, `buy_coins` / `redeem_prize_mon` / `ingame_trade` in `PokemonFRLG_MailGlitchActions.h`, `spin_for_encounter` in `PokemonFRLG_MailGlitchBattle.cpp`, and map JSONs for `Route11`, `Route15`, `Route15_WestEntrance_1F` in `Packages/`).

To implement **Milestone B** cleanly (building on [PR #804](https://github.com/dayfine/Arduino-Source/pull/804)):

| # | Engine Upgrade | Why Needed | Priority |
|---|---|---|---|
| 1 | **Multi-species `capture_any(species_list)`** | Catch whichever unowned target appears on shared patches (Viridian Forest, Rt 3, Mt. Moon 1F, Rt 4, Tower 7F, Rt 18); saves ~10–15 min of wasted flee animations | High |
| 2 | **Wire hardened `spin_for_encounter` into `AutoStoryCapture`** | Zero-drift turn-in-place encounter trigger on a single tile (from PR #804 PR B) | High |
| 3 | **Bag `use_item_on_party` for Evolution Stones** | Zero-RNG Fire/Leaf/Moon/Thunder Stone evolutions (`Arcanine`/`Ninetales`, `Clefable`, `Wigglytuff`, `Vileplume`/`Victreebel`, `Raichu`) | High |
| 4 | **Add `Route15_WestEntrance_2F` coded room** | Walk up stairs `[9, 10]` from `Route15_WestEntrance_1F` and talk to Oak's Aide (same pattern as `PokemonFRLG_MailGlitchRooms.h`) | High |
| 5 | **Allow Great Balls in `open_ball_pocket()`** | `PokemonFRLG_AutoStoryCaptureConsole.cpp:271` currently hardcodes `rows.front().item == "poke-ball"`; Great Balls cut throws ~35–40% on CR 90–127 (`Haunter`, `Fearow`, `Raticate`, `Tangela`) | Medium |
| 6 | **Make `05.04` PC step species-aware** | Prevents boxed catches from colliding with `05.04`'s fixed Box 1 slot assumptions | High (if catching pre-`05.04`) |
| 7 | *(Milestone A only)* **Safari & Fishing capture loops** | Wire existing `SafariOptimalAction` + `ShinyHunt-Fishing` bite detection into `AutoStoryCapture` | Optional *(skipped entirely under Milestone B)* |

---

## 7. Strategic Verdict: Should We Interleave 50 Species?

| Target Workflow | Interleave Exp. Share? | Quantitative Reasoning |
|---|---|---|
| **Core AutoStory (`01.01` → `20.05` Hall of Fame)** | **No (keep opt-in)** | Blastoise solos the story. Equipping Exp. Share splits trainer EXP 50/50 and **breaks `frlg-league-plan.json`'s Lv 54–55 League thresholds**, while adding ~45–65 min for zero speedrun benefit. |
| **Mail Glitch (`mg-01` → `mg-05`) on Tower 5F as-is** | **Break-even (~0 net gain)** | `mg-04b` needs ~35,200–35,600 EXP for `MIMIEN` Lv 9(FR)/Lv 7(LG) → Lv 33. On Tower 5F (~163 split traded EXP/battle ≈ ~216 battles), Exp. Share removes switch-in/out (~14s/battle → ~50 min saved), roughly canceling the ~45–60 min spent catching 50 species. |
| **Mail Glitch upgraded to Victory Road 2F + Exp. Share** | **Yes (+35–50 min net win)** | With Exp. Share, Blastoise leads and Turn-1 OHKOs **Victory Road 2F** (~600 split traded EXP/battle → **~59 battles ≈ 22 min** vs. ~216 battles ≈ **115 min** on Tower 5F), saving **~94 min** and eliminating low-level switch-in risk. |
| **Post-Game / National Dex / Multi-Mon / RNG Saves** | **Strong Yes** | Unlocking the **National Dex** (Sevii Islands 4–7, Cerulean Cave/Mewtwo, breeding) *already* requires **60 owned Kanto species**. Milestone B delivers **Exp. Share + 83% of the National Dex** in one pass with zero Safari/Fishing code. |

### Recommendation
1. **Keep core AutoStory (`01.01` → `20.05`) lean** — do not enable 50-species catching by default.
2. **Implement Milestone B as an opt-in AutoStory profile / post-`16.04` module** (funded by `05.04` Nugget Farm `count: 13–14`) for saves targeting **Mail Glitch (Victory Road 2F Exp. Share grind)** and/or **National Dex (60 species)**, reusing the shared actions and `StoryRepeat` loop planned in [PR #804](https://github.com/dayfine/Arduino-Source/pull/804).
