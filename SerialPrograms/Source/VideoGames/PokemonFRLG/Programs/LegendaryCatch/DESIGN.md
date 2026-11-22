# FRLG Legendary Catch — Design Document

Self-contained automation design for catching every legendary Pokémon in **Pokémon FireRed / LeafGreen** using **Luxury Balls**, with **no Master Ball** and **no trading** required.

---

## 1. Program Options & Preconditions

Implemented in:
- `PokemonFRLG_LegendaryCatch.h` / `PokemonFRLG_LegendaryCatch.cpp`
- `PokemonFRLG_LegendaryCatchRequirements.h` / `PokemonFRLG_LegendaryCatchRequirements.cpp`

### User Options
- **Target Legendaries** (`BooleanCheckBoxOption` per target):
  - **Static:** Articuno (Lv. 50), Zapdos (Lv. 50), Moltres (Lv. 50), Mewtwo (Lv. 70), Deoxys (Lv. 30), Lugia (Lv. 70), Ho-oh (Lv. 70)
  - **Roamers:** Raikou (Lv. 50), Entei (Lv. 50), Suicune (Lv. 50)
- **Precondition Settings:**
  - `LUXURY_BALLS_PER_LEGENDARY` (default `50`, range `1–200`)
  - `REQUIRE_TYPE_WALLS` (default `false`; enforces Dark/Ground/Flash-Fire walls when enabled)
  - `CHECK_BAG_ITEMS` (default `true`)
  - `LANGUAGE` (`OCR::LanguageOCROption`)

### Requirement Aggregation (OR / MAX & SUM)
`compute_legendary_catch_requirements()` merges all ticked targets:
- **Party members (OR / MAX):**
  - **Parasect** (`Spore` + `False Swipe`): required for any selected target (`Lv. 50+` for Deoxys, `Lv. 55+` for Kanto birds, `Lv. 70+` for Mewtwo / Lugia / Ho-oh / Roamers).
  - **Haunter or Gengar** (`Mean Look` + `Taunt` [TM12] + `Hypnosis`): required if Mewtwo, Lugia, Ho-oh, or any Roamer is selected (`Lv. 90+` and `Speed >= 208` for Mewtwo; `Lv. 70+` and `Speed >= 137` for Raikou; `Lv. 70+` and `Speed >= 122` for Entei; `Lv. 70+` for Lugia / Ho-oh / Suicune).
  - **Optional Type Walls** (when `REQUIRE_TYPE_WALLS` is enabled):
    - Mewtwo → `Tyranitar` / `Houndoom` (`Lv. 70+`)
    - Zapdos → `Steelix` / `Rhydon` / `Golem` (`Lv. 55+`)
    - Moltres / Ho-oh → `Arcanine` / `Houndoom` / `Ninetales` (`Lv. 55+` / `70+`)
- **Bag items (SUM for consumables, MAX for reusable):**
  - **Luxury Ball** (`PokeBalls` pocket): **SUM** (`LUXURY_BALLS_PER_LEGENDARY × selected_count`)
  - **Max Repel** (`Items` pocket): **SUM** across cave/roamer targets (`5` each for Articuno, Moltres, Mewtwo, Raikou, Entei, Suicune; `3` for Zapdos)
  - **Leftovers** (`Items` pocket): **MAX** (`2` if Haunter/Gengar trapper is needed, `1` otherwise)
  - **Smoke Ball** (`Items` pocket): `1` if any Roamer is selected

### Precondition Verification (`run_preconditions`)
1. `home_black_border_check`
2. **Party Check (`read_party_members`):** opens the Party Menu, detects occupied slots, opens Summary Page 1 (`StatsReader::read_page1` for species + level) and Page 2 (`StatsReader::read_page2` for Speed) for each slot, and checks against `check_party_requirements()`. Throws `UserSetupError` on missing members or insufficient Level/Speed.
3. **Bag Check (`read_bag_items`):** opens the Bag, navigates to the required pockets (`Items`, `PokeBalls`), OCRs item names and quantities (`xNNN`), and checks against `check_bag_requirements()`. Throws `UserSetupError` if any item is missing or below `min_count`.

---

## 2. Navigation & Map Prerequisites

- **Map Pack (`PokemonSimulation`):** Sevii Islands story/catching maps, `BirthIsland_*`, `PowerPlant`, and `SeafoamIslands_1F`–`B4F` (with `pushable` boulders, `rock_smash` gates, `i` sliding ice, and `I` thin ice).
- **Navel Rock:** compressed step + warp paths in `PokemonSimulation/data/games/frlg/routes/navel_rock.json` (`ho_oh`: 167 steps; `lugia`: 195 steps from `NavelRock_Exterior` `(9, 16)`).
- **Birth Island (Deoxys):** 100% deterministic 11-step triangle puzzle (`sDeoxysCoords` / `sDeoxysStepCaps` in `pokefirered`), suitable for a fixed button/step script.

---

## 3. Battle & Catching Loops (Planned)

- **Standard Static (Articuno, Zapdos, Moltres, Deoxys):**
  1. Lead **Parasect**: `Spore` → `False Swipe` to `1 HP`.
  2. Throw **Luxury Balls**; re-apply `Spore` whenever it wakes (sleeping Pokémon consume 0 PP in Gen 3).
- **Healing / Safeguard Static (Lugia, Ho-oh, Mewtwo):**
  - **Mewtwo:** Lead **Haunter/Gengar** → `Taunt` Turn 1 (blocks `Safeguard` and `Recover`) → switch to **Parasect** → `Spore` → `False Swipe` → **Luxury Balls**.
  - **Lugia / Ho-oh:** `Spore` + `False Swipe` (re-`False Swipe` + `Spore` or `Taunt` if `Recover` triggers).
- **Roaming Legendaries (Raikou, Entei, Suicune):**
  - *FRLG Roar Bug:* if Raikou or Entei uses `Roar`, it is permanently deleted from the save.
  - **Step 1:** Lead **Parasect** (Smoke Ball) → `False Swipe` to `1 HP` (damage persists) → save.
  - **Step 2:** Lead **Haunter/Gengar** → Turn 1 `Mean Look` (reset if it Roars) → Turn 2 `Taunt` → Turn 3 `Hypnosis` → throw **Luxury Balls** while keeping `Taunt` / Sleep active.
