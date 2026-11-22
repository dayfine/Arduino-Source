# FRLG Language-Agnostic Design (ENG / JPN / FRA / GER / SPA / ITA)

Design audit and implementation plan for making **`PokemonFRLG:AutoStory`** and **`PokemonFRLG:MailGlitchActivation`** (PR #781) work across all six FireRed / LeafGreen ROM languages: English (`eng`), Japanese (`jpn`), French (`fra`), German (`deu`), Spanish (`spa`), and Italian (`ita`).

Cross-referenced against `pokefirered` (`PokemonSimulation/external/pokefirered`) and `origin/frlg/lean`.

---

## 1. Guiding Principle: Eliminate OCR First, Reuse Upstream 6-Language DBs Second

1. **Structural / pixel / digit invariants first**: whenever `pokefirered` guarantees a fixed cursor wrap, fixed menu row order, sprite graphic, or 2-digit index across all languages, use that directly instead of OCR.
2. **Reuse existing upstream 6-language resources**: `upstream/main` already ships 6-language OCR databases and multi-language detectors (`BattleMovesOCR.json`, `PokeballNameOCR.json`, `BerryNameOCR.json`, `PokemonOCR-{eng,jpn,fra,deu,spa,ita}.json`, `SummarySpriteReader`, JPN variants in `BattleDialogs` / `BattleMenuDetector` / `StatsReader` / `PartyHeldItemDetector`). Avoid duplicating English-only tables in `Inference/Menus/`.
3. **State-driven dialog sync**: never hardcode `BUTTON_A` counts to step past `\p` page breaks; page-break counts vary by language and text speed (Slow/Mid/Fast). Wait on `AdvanceWhiteDialogDetector` / `SelectionArrowDetector` / `yes_no_cursor`.

---

## 2. Provenance Summary: Upstream FRLG vs. AutoStory (`frlg/lean`) vs. Mail Glitch (#781)

Almost every non-English breakage lives in **our `frlg/lean` AutoStory code** or **Mail Glitch PR #781** — not in `upstream/main`:

| Layer | What works across all 6 languages today | What breaks / needs work |
|---|---|---|
| **Upstream FRLG** (`upstream/main`) | `SummarySpriteReader` (tested on all 6 langs), `BattleMovesOCR.json` (all 354 moves × 6 langs), `PokeballNameOCR.json`, `BerryNameOCR.json`, `PokemonOCR-*.json`, `StatsReader` (+JPN), `WildEncounterReader` (+JPN), `BattleDialogDetector` / `BattleMenuDetector` / `AdvanceBattleDialogDetector` (+JPN), `PartyHeldItemDetector` (+JPN), `WhiteDialogDetector`, `SelectionDialogDetector`, `PartyMenuDetector`, `BagDetector`, `SummaryDetector`, `KantoMapDetector`, `LoadMenuDetector`. | (1) Slug convention mismatch between two upstream JSONs: `PokemonOCR-*.json` emits `"farfetch-d"` while `PokemonSprites.json` emits `"farfetchd"`.<br>(2) Verify `StartMenuDetector` JPN box width (`tilemapLeft` 22 vs 23).<br>(3) `BattleLearnDialogDetector` (in `PokemonFRLG_BattleDialogs.cpp`) has no JPN variant yet. |
| **Our AutoStory** (`frlg/lean`) | Overworld navigation (`OverworldWalk`, `OverworldSim`, `PokemonCenterFloorReader`), `BattleLevelUpReader` (waterfill digits), `PartyEmptySlotDetector`. | • `PokemonFRLG_AutoStoryActions.cpp`: `yes_no_cursor()` white check hits GER `NEIN` & JPN `いいえ`.<br>• `PokemonFRLG_AutoStoryPocketList.cpp`: `read_whole_pocket()` aborts early on adjacent unrecognized items (`{"", -1}` in Key Items).<br>• `PokemonFRLG_AutoStorySummary.cpp`: `ConsolePartyScreens::read_species()` uses `StatsReader` (hits `"farfetch-d"` mismatch) instead of `SummarySpriteReader`; `skills_hp_digits()` hardcodes Western HP box.<br>• `PokemonFRLG_AutoStoryLaunch.cpp`: hardcodes English `"Pokémon FireRed Version"` Switch HOME title.<br>• `PokemonFRLG_AutoStoryCaptureConsole.cpp`: `throw_ball()` uses Western-only `BattleSelectionArrowDetector`; `open_ball_pocket()` expects `"poke-ball"` on row 0 via ENG-only `ItemListReader`.<br>• `PokemonFRLG_BattleSelectionArrowDetector.cpp` & `PokemonFRLG_BattleScreenDetector.cpp`: Western-only battle cursor & YES/NO border coordinates.<br>• ENG-only menu readers added for AutoStory: `ItemListReader`, `SummaryMovesReader`, `PartyMemberMenuReader`, `SaveCardReader`, `OptionMenuReader`, `MoneyBoxReader` (currency suffix on JPN). |
| **Mail Glitch PR** (`#781`) | `write_single_word_mail()` (verified identical across all 6 langs in `pokefirered/src/easy_chat_2.c:421-485, 613-713`). | • `PokemonFRLG_MailGlitchActions.cpp`: Berry Pouch `GIVE` presses `DOWN` × 2 (selects **TOSS**!) instead of `DOWN` × 1 (`berry_pouch.c:187-192`), and reads 6-row Bag layout instead of 7-row `"No.XX "` Berry Pouch.<br>• `swap_party_slots()` calls ENG-only `PartyMemberMenuReader` instead of language-free `DPAD_UP` × 3.<br>• `run_buy_coins()` / `run_redeem_prize_mon()` hardcode `BUTTON_A` counts for `\p` page breaks.<br>• `mg-02/04/05.plan.json` use `"farfetchd"` (collides with `PokemonOCR` `"farfetch-d"`). |

---

## 3. Immediate Correctness Bugs (Break ENG as Well as Non-ENG)

1. **`"farfetchd"` vs. `"farfetch-d"` slug mismatch** *(Upstream JSON convention + AutoStorySummary + MailGlitch plans)*
   - `PokemonOCR-{eng,jpn,fra,deu,spa,ita}.json` emits `"farfetch-d"`; `PokemonSprites.json` and `mg-02/04/05.plan.json` use `"farfetchd"`.
   - `ConsolePartyScreens::read_species()` calls `StatsReader::read_page1` → returns `"farfetch-d"` ≠ `"farfetchd"` → fails on `mg-02` skip, `mg-04` CH'DING grind, and `mg-05` party swap.
   - *(Note on in-game trade nicknames `CH'DING` / `MIMIEN`: **safe** in `StatsReader`, which reads the right-pane `speciesNameStrBuf` per `pokefirered/src/pokemon_summary_screen.c:2467`, not the nickname box.)*
   - **Fix**: use upstream `SummarySpriteReader` (100% language-free, already 6-lang tested, emits `"farfetchd"`) as primary in `ConsolePartyScreens::read_species()`, and normalize `"farfetch-d"` → `"farfetchd"` in the `StatsReader` fallback.

2. **Berry Pouch `GIVE` is row 1, not row 2 — and Berry Pouch is 7 rows with `"No.XX "` prefix** *(Mail Glitch #781)*
   - `pokefirered/src/berry_pouch.c:187-192, 1023` displays `sOptions_UseGiveTossExit = {USE(0), GIVE(1), TOSS(2), EXIT(3)}`. Pressing `DPAD_DOWN` × 2 selects **`TOSS`** and deletes the Sitrus Berry!
   - Berry Pouch shows **7** rows (`listMenuMaxShowed = 7`, `berry_pouch.c:901`), not 6, and prepends `"No.XX "` (`berry_pouch.c:686-697`), shifting item names ~26 px right.
   - **Fix**: press `DPAD_DOWN` × **1** for `GIVE`, and identify berries language-freely by their 2-digit Berry index (`07` = Oran Berry, `10` = Sitrus Berry; `items.h:143,146,181`) via `read_digits_waterfill_template` (Berry Pouch is always sorted by Berry number, `berry_pouch.c:888`), with upstream 6-language `Pokemon::BerryNameReader` (`BerryNameOCR.json`) as fallback.

3. **`read_whole_pocket()` aborts prematurely on Key Items & large Items pockets** *(AutoStory `frlg/lean`)*
   - `PokemonFRLG_AutoStoryPocketList.cpp:50`: `same = (row->item == reads.back().item && row->count == reads.back().count)`.
   - In Key Items, every row has `count == -1`, and `ItemListReader` only recognizes `bicycle` and `berry-pouch`. Any two adjacent unrecognized Key Items compare `{"", -1} == {"", -1}` and abort scrolling before reaching `berry-pouch`! Same happens in Items if two adjacent unrecognized items share a count above `retro-mail` / `revive` / `escape-rope`.
   - **Fix**: detect end-of-scroll in `ConsolePocketList` by comparing the bottom row's raw image pixels before vs. after `DPAD_DOWN` (only short-circuit on matching text when `!row->item.empty()`).

4. **`yes_no_cursor()` breaks on German (`NEIN`) and Japanese (`いいえ`)** *(AutoStory `frlg/lean`)*
   - `PokemonFRLG_AutoStoryActions.cpp:448` checks `is_white(x=196..212, y=92..102)`.
   - In `pokefirered/src/new_menu_helpers.c:48`, `sYesNo_WindowTemplate` is `tilemapLeft=21` (`x=168`), `width=6` (`x=168..216`), text starts at `x=176`. German `"NEIN"` reaches `x≈200` and Japanese `"いいえ"` reaches `x≈203`, putting dark ink inside `196..212` and breaking `save_game`, `cut_tree`, `redeem_prize_mon`, `ingame_trade`, `give_bag_item`, and `capture`.
   - **Fix**: move the white check to the right margin `x=211..216, y=74..104` (same clear strip already used by `SelectionDialogDetector`), and widen the arrow X box slightly if JPN shifts `tilemapLeft` by 1 tile.

5. **`launch_frlg_save()` hardcodes English FireRed Switch HOME title** *(AutoStory `frlg/lean`)*
   - `PokemonFRLG_AutoStoryLaunch.cpp:190` calls `switch_home_tile_named(Language::English, "Pokémon FireRed Version")`. Fails on every non-English Switch OS language and on LeafGreen.
   - **Fix**: accept all 6-language FireRed & LeafGreen HOME titles (or restart the active tile at index 0 without title OCR when already focused).

---

## 4. Component-by-Component Audit Matrix

| # | Component | Source | Current Status | Language-Agnostic Design |
|---|---|---|---|---|
| 1 | Party Species Lookup (`ConsolePartyScreens::read_species`) | AutoStory + Upstream JSONs | Partial + `"farfetchd"` bug | **Language-free primary**: upstream `SummarySpriteReader` (6-lang tested, uses `"farfetchd"`). Fallback: `StatsReader::read_name` normalizing `"farfetch-d"` → `"farfetchd"`. |
| 2 | Party Swap `SWITCH` (`swap_party_slots`) | Mail Glitch #781 | ENG-only (calls `PartyMemberMenuReader`) | **100% Language-free**: verified in `pokefirered/src/party_menu.c:2963-2988, 3069-3072` — for ≥2 non-Egg mons, `numActions >= 4` so `Menu_ProcessInput_other()` wraps, and the bottom 3 rows are always `[SWITCH, ITEM/MAIL, CANCEL]` regardless of field moves or Mail. Press **`DPAD_UP` × 3** from `SUMMARY`. |
| 3 | Field Move Menu (`PartyMemberMenuReader`) | AutoStory | ENG-only hardcoded array | Load the 12 field moves from upstream **`Packages/Resources/PokemonFRLG/BattleMovesOCR.json`** (all 6 langs already populated). Support JPN narrower window (`TEXT_LEFT = 168`). |
| 4 | Summary Moves (`SummaryMovesReader` + `skills_hp_digits`) | AutoStory | ENG-only hardcoded array + Western HP box | Replace hardcoded `MOVE_NAMES` with upstream **`BattleMovesOCR.json`**. Use `m_box_hp_jpn` (`x=0.717`) for `Language::Japanese` in `skills_hp_digits()`. |
| 5 | Berry Pouch (`run_give_bag_item`) | Mail Glitch #781 | Broken on all langs (`DOWN` × 2 = TOSS) | Fix `DOWN` × **1** for `GIVE`. Read 2-digit berry number (`07` / `10`) across 7-row Berry Pouch via `DigitReader` (100% language-free); fallback to upstream 6-lang `BerryNameReader` on shifted name box (`x=122..192`). |
| 6 | Item List & Bag Scrolling (`ItemListReader`, `read_whole_pocket`) | AutoStory | ENG-only + premature scroll abort + clipped EUR names | (1) Add 6-lang `Packages/Resources/PokemonFRLG/ItemListOCR.json` (§5).<br>(2) Widen `item_list_name_box` from `72px` (`x=96..168`) to `94px` (`x=96..190`) so EUR names (`SAQUITO BAYAS`, `CORDE SORTIE`, `REVITALIZZ.`) fit.<br>(3) Pixel-diff bottom row in `ConsolePocketList` to fix scroll abort. *(Optional future upgrade: 24×24 item sprite icon matching at `(24, 140)` per `item_menu_icons.c:396`.)* |
| 7 | Battle Ball Throwing (`throw_ball`, `open_ball_pocket`, `BattleSelectionArrowDetector`) | AutoStory | ENG-only + Western-only battle arrow | (1) Navigate to BAG via corner-reset `DPAD_LEFT, DPAD_UP, DPAD_RIGHT` (zero coordinates) and add JPN boxes to `BattleSelectionArrowDetector`.<br>(2) Open Poké Balls pocket via `DPAD_RIGHT` × 3 (no-wrap per `item_menu.c:1137-1140`) + `DPAD_UP` × 12, then read ball with upstream 6-lang `Pokemon::PokeballNameReader` to skip `master-ball`. |
| 8 | Overworld YES/NO (`yes_no_cursor`) | AutoStory | Breaks GER `NEIN` & JPN `いいえ` | Move white check to right margin `x=211..216, y=74..104`. |
| 9 | Battle YES/NO (`battle_yes_no_shows`, `BattleLearnDialogDetector`) | AutoStory + Upstream `BattleDialogs.cpp` | Western-only border box | Widen `YES_NO_FRAME_BOX` horizontal search to `x=184..201` and ensure `m_menu_top_box` starts at `0.835` (covers both tile 24 and tile 25 for JPN). |
| 10 | Dialog `\p` Page Breaks & Multichoice (`buy_coins`, `redeem_prize_mon`, `ConsoleMartActions::buy`, `pick_lift_floor`) | Mail Glitch #781 + AutoStory | Fragile hardcoded `A` counts | Add `advance_to_multichoice(console, context, arrow_box, timeout)` watching `SelectionArrowDetector` at the menu's known tile column (`CoinsClerk`: `x=104..120`, `PrizeClerkMons`: `x=88..104`, `Elevator`: `x=8..24`, `ShopMenu`: `x=16..32`) and pressing `A` only on `AdvanceWhiteDialogDetector`. Wait for `yes_no_cursor` before mart buy confirmation. |
| 11 | Easy Chat Mail Editor (`write_single_word_mail`) | Mail Glitch #781 | Already language-agnostic | Verified identical across all 6 langs in `pokefirered/src/easy_chat_2.c:421-485, 613-713` (`A, A, A, START, A`). |
| 12 | Launch / Save Card / Options / Mart Money | AutoStory | ENG-only (`Launch`, `SaveCard`, `Options`); JPN suffix in `MoneyBox` | (1) Multi-lang FireRed/LeafGreen HOME title matching.<br>(2) 6-lang `SaveCardPlacesOCR.json` (52 Kanto place names).<br>(3) 6-lang `OptionMenuReader` values.<br>(4) `MoneyBoxReader`: `DigitReader` + JPN left-shifted box `x=16..64` (JPN renders `10586えん` with currency on right). |

---

## 5. Gen 3 FRLG 6-Language Item Dictionary (Core Mail Glitch & Mart Items)

For the new `Packages/Resources/PokemonFRLG/ItemListOCR.json` (note: `BerryNameOCR.json`, `PokeballNameOCR.json`, and `BattleMovesOCR.json` already have all 6 languages upstream):

| Slug | ENG | JPN | FRA | GER (`deu`) | SPA | ITA |
|---|---|---|---|---|---|---|
| `retro-mail` | `Retro Mail` | `レトロメール` | `Lettre Rétro` | `Retrobrief` | `Carta Retro` | `Mess. Retro` |
| `poke-doll` | `Poké Doll` | `ピッピにんぎょう` | `Poképoupée` | `Poképuppe` | `Poké Muñeco` | `Pokébambola` |
| `berry-pouch` | `Berry Pouch` | `きのみぶくろ` | `Sac à Baies` | `Beerentüte` | `Saquito Bayas` | `Portabacche` |
| `oran-berry` | `Oran Berry` | `オレンのみ` | `Baie Oran` | `Sinelbeere` | `Baya Aranja` | `Baccarancia` |
| `sitrus-berry` | `Sitrus Berry` | `オボンのみ` | `Baie Sitrus` | `Tsitrubeere` | `Baya Zidra` | `Baccacedro` |
| `revive` | `Revive` | `げんきのかけら` | `Rappel` | `Beleber` | `Revivir` | `Revitalizz.` |
| `escape-rope` | `Escape Rope` | `あなぬけのヒモ` | `Corde Sortie` | `Fluchtseil` | `Cuerda Huida` | `Fune di Fuga` |
| `poke-ball` | `Poké Ball` | `モンスターボール` | `Poké Ball` | `Pokéball` | `Poké Ball` | `Poké Ball` |
| `great-ball` | `Great Ball` | `スーパーボール` | `Super Ball` | `Superball` | `Super Ball` | `Mega Ball` |
| `ultra-ball` | `Ultra Ball` | `ハイパーボール` | `Hyper Ball` | `Hyperball` | `Ultra Ball` | `Ultra Ball` |
| `cancel` | `Cancel` | `やめる` | `Retour` | `Zurück` | `Salir` | `Esci` |

*(Remaining ~30 AutoStory Mart/Bag items — potions, status heals, repels, X items, stones, `rare-candy`, `bicycle`, `nugget` — should be extracted directly from the 6 ROM string tables to preserve exact Gen 3 abbreviations/punctuation such as `Parlyz Heal`, `Guard Spec.`, `Revitalizz.`.)*

---

## 6. Phased Implementation & Verification Plan

### Phase 1 — Critical Correctness Fixes (Land with PR #781 / Immediate AutoStory Patch)
- Fix Berry Pouch `GIVE` (`DOWN` × 1) + 7-row `"No.XX"` digit-based Berry lookup.
- Switch `ConsolePartyScreens::read_species()` to `SummarySpriteReader` (with `"farfetch-d"` → `"farfetchd"` normalization in `StatsReader` fallback).
- Fix `read_whole_pocket()` scroll-end detection via bottom-row pixel diff.
- Fix `yes_no_cursor()` right-margin white box (`x=211..216`).
- Replace `swap_party_slots()` menu OCR with `DPAD_UP` × 3.

### Phase 2 — Language-Free Structural Hardening (Shared AutoStory PR)
- Replace hardcoded `A` counts in `buy_coins`, `redeem_prize_mon`, mart `buy`, and `pick_lift_floor` with `advance_to_multichoice()` / `yes_no_cursor()`.
- Update `ConsoleCaptureActions::throw_ball()` & `open_ball_pocket()` to use corner-reset navigation + `PokeballNameReader`.
- Add JPN boxes to `BattleSelectionArrowDetector`, `battle_yes_no_shows`, `BattleLearnDialogDetector`, `skills_hp_digits`, `MoneyBoxReader`, and verify `StartMenuDetector`.

### Phase 3 — 6-Language OCR Wiring & Cross-Language mGBA Verification
- Switch `SummaryMovesReader` and `PartyMemberMenuReader` to upstream `BattleMovesOCR.json`.
- Add 6-language `ItemListOCR.json` + widen `item_list_name_box` to `94px`.
- Multi-language `AutoStoryLaunch`, `SaveCardPlacesOCR.json`, and `OptionMenuReader`.
- **Verification**:
  1. Unit tests with cropped fixtures for ENG, JPN, GER (`NEIN`), FRA/SPA/ITA (long item names).
  2. Live mGBA end-to-end verification of `MailGlitchActivation` (`mg-01..mg-05`) on **ENG** and **JPN** ROMs, plus **GER** `yes_no_cursor` / Mart / Bag spot-check.
