/*  Mail Glitch Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <vector>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_PartyDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_BagDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyHeldItemDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_DigitReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryActions.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryGameArgs.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryItemUse.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPocketList.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryStartMenu.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySummary.h"
#include "PokemonFRLG_MailGlitchActions.h"
#include "PokemonFRLG_MailGlitchBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::Outcome;
using Pokemon::RunState;
using Pokemon::StoryArgType;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;


namespace{

constexpr Milliseconds MENU_TAP = 100ms;
constexpr Milliseconds MENU_WAIT = 250ms;
constexpr Milliseconds DIALOG_TIMEOUT = 60000ms;
constexpr Milliseconds BAG_SLIDE_WAIT = 1500ms;

void tap_dpad_times(ProControllerContext& context, DpadPosition dir, size_t count){
    for (size_t c = 0; c < count; c++){
        pbf_press_dpad(context, dir, MENU_TAP, MENU_WAIT);
    }
}

bool is_berry_item(std::string_view item){
    return item.ends_with("-berry");
}

//  Language-free Berry Pouch row finder using the fixed 2-digit berry index
//  ("No.XX ", pokefirered/src/berry_pouch.c:686-697). Returns 0-based row from
//  the top of the Berry Pouch (cursor reset back to top), or nullopt if not found.
std::optional<int> berry_pouch_target_number(std::string_view item){
    if (item == "oran-berry"){
        return 7;
    }
    if (item == "sitrus-berry"){
        return 10;
    }
    return std::nullopt;
}

std::optional<size_t> find_berry_row_in_pouch(
    ConsoleHandle& console,
    ProControllerContext& context,
    std::string_view item
){
    const std::optional<int> target_num = berry_pouch_target_number(item);
    if (!target_num){
        return std::nullopt;
    }
    //  Berry Pouch window (pokefirered/src/berry_pouch.c:218-225, 670-675):
    //  tilemapLeft=11 (x=88), tilemapTop=1 (y=8), item_X=9 (x=97), upText_Y=2 (y=10),
    //  7 visible rows (16px pitch). "No." spans ~12px (x=97..109); 2 digits sit in x=108..124.
    constexpr int BERRY_POUCH_VISIBLE_ROWS = 7;
    constexpr size_t MAX_BERRIES = 43;
    auto read_row_num = [&](const ImageViewRGB32& screen, int shown) -> int{
        const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
        const ImageFloatBox box(108.0 / 240, (10.0 + 16.0 * shown) / 160, 16.0 / 240, 14.0 / 160);
        return read_digits_waterfill_template(console.logger(), extract_box_reference(game, box), DigitTemplateType::StatBox);
    };

    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    if (!screen){
        return std::nullopt;
    }
    for (int shown = 0; shown < BERRY_POUCH_VISIBLE_ROWS; shown++){
        const int num = read_row_num(screen, shown);
        if (num == *target_num){
            return static_cast<size_t>(shown);
        }
    }
    //  Scroll past the first 7 rows (Berry Pouch scrolls once cursor passes row 3).
    size_t downs = 0;
    for (int c = 0; c < 3; c++){
        pbf_press_dpad(context, DPAD_DOWN, 100ms, 350ms);
        downs++;
    }
    std::optional<size_t> found;
    int last_num = -1;
    for (size_t idx = BERRY_POUCH_VISIBLE_ROWS; idx < MAX_BERRIES; idx++){
        pbf_press_dpad(context, DPAD_DOWN, 100ms, 350ms);
        downs++;
        context.wait_for_all_requests();
        VideoSnapshot cur = console.video().snapshot();
        if (!cur){
            break;
        }
        const int num = read_row_num(cur, BERRY_POUCH_VISIBLE_ROWS - 1);
        if (num == *target_num){
            found = idx;
            break;
        }
        if (num < 0 || num == last_num){
            break;
        }
        last_num = num;
    }
    for (size_t c = 0; c < downs; c++){
        pbf_press_dpad(context, DPAD_UP, 80ms, 120ms);
    }
    context.wait_for_all_requests();
    return found;
}

//  Opens Bag to `pocket` (0 = Items, 1 = Key Items, 2 = Poke Balls) with cursor on top row.
void open_bag_pocket_top(ConsoleHandle& console, ProControllerContext& context, int pocket){
    open_bag_from_overworld(console, context, start_menu_context(StartMenu::FULL));
    pbf_wait(context, BAG_SLIDE_WAIT);
    for (int c = 0; c < 3; c++){
        pbf_press_dpad(context, DPAD_LEFT, 100ms, 600ms);
    }
    for (int c = 0; c < pocket; c++){
        pbf_press_dpad(context, DPAD_RIGHT, 100ms, 600ms);
    }
    for (int c = 0; c < 25; c++){
        pbf_press_dpad(context, DPAD_UP, 80ms, 100ms);
    }
    context.wait_for_all_requests();
}

//  Finds an item's 0-based row in the currently open pocket (cursor starts and ends on row 0).
std::optional<size_t> find_item_row_in_open_pocket(
    ConsoleHandle& console, ProControllerContext& context, Language language, std::string_view item
){
    const PocketRead read = read_open_pocket(console, context, language);
    for (size_t row = 0; row < read.rows.size(); row++){
        if (read.rows[row].item == item && read.rows[row].count != 0){
            return row;
        }
    }
    return std::nullopt;
}

//  Buys `coins` from Celadon Game Corner CoinsClerk (facing clerk).
//  Each talk opens multichoice: 0 = 50 COINS ($1,000), 1 = 500 COINS ($10,000), 2 = NO THANKS.
Outcome run_buy_coins(StoryEnv& env, const StoryStep& step){
    const int64_t total_coins = Pokemon::story_arg<int64_t>(step.args, "coins");
    if (total_coins <= 0){
        return Pokemon::outcome_ok();
    }
    int64_t remaining = total_coins;
    env.console.log(std::format("MailGlitch: buying {} Game Corner coins.", total_coins));
    while (remaining > 0){
        const bool buy_500 = remaining >= 500;
        const int64_t batch = buy_500 ? 500 : 50;
        //  Talk to CoinsClerk:
        //    Page 1: "Welcome to ROCKET GAME CORNER!\p" -> A advances page break.
        //    Page 2: "Do you need some game COINS?..." + multichoice (0 = 50, 1 = 500, 2 = NO THANKS).
        pbf_press_button(env.context, BUTTON_A, 160ms, 1800ms);
        pbf_press_button(env.context, BUTTON_A, 160ms, 1800ms);
        if (buy_500){
            pbf_press_dpad(env.context, DPAD_DOWN, 120ms, 300ms);
        }
        pbf_press_button(env.context, BUTTON_A, 160ms, 1500ms);
        mash_until_overworld(env.console, env.context, DIALOG_TIMEOUT);
        remaining -= batch;
        env.console.log(std::format("MailGlitch: bought {} coins ({} remaining).", batch, std::max<int64_t>(0, remaining)));
    }
    return Pokemon::outcome_ok();
}

//  Redeems a prize Pokemon from Celadon Game Corner PrizeClerkMons (facing clerk across counter).
//  Multichoice: 0 = ABRA (180c FR / 120c LG), 1 = CLEFAIRY, 2 = DRATINI/PINSIR, 3 = SCYTHER/DRATINI, 4 = PORYGON.
Outcome run_redeem_prize_mon(StoryEnv& env, const StoryStep& step){
    const int64_t slot = Pokemon::story_arg<int64_t>(step.args, "slot");
    if (slot < 0 || slot > 4){
        return Pokemon::outcome_failed("bad_args", std::format("prize slot {} out of range 0-4", slot));
    }
    env.console.log(std::format("MailGlitch: redeeming Game Corner prize slot {}.", slot));
    //  "We exchange your coins for prizes." -> A -> "Which prize would you like?" + multichoice.
    pbf_press_button(env.context, BUTTON_A, 160ms, 1800ms);
    pbf_press_button(env.context, BUTTON_A, 160ms, 1800ms);
    tap_dpad_times(env.context, DPAD_DOWN, static_cast<size_t>(slot));
    pbf_press_button(env.context, BUTTON_A, 160ms, 1200ms);
    //  "So, you want the ABRA?" YES/NO -> YES.
    answer_yes_no(env.console, env.context, true, BUTTON_A, DIALOG_TIMEOUT);
    //  "PLAYER received the ABRA!" + "Give a nickname?" -> B answers NO and clears to overworld.
    mash_until_overworld(env.console, env.context, DIALOG_TIMEOUT);
    env.party.invalidate();
    return Pokemon::outcome_ok();
}

//  Completes an in-game trade facing the NPC (Elyssa for CH'DING or Reyley for MIMIEN).
Outcome run_ingame_trade(StoryEnv& env, const StoryStep& step){
    const std::string& offer_species = Pokemon::story_arg<std::string>(step.args, "species");
    const std::string* receive_species = game_arg<std::string>(step.args, "receive");

    //  Idempotency: if we already have the received species and not the offer species, skip.
    std::optional<size_t> offer_slot = find_party_slot_by_species(env, offer_species);
    if (!offer_slot){
        if (receive_species != nullptr && !receive_species->empty()
            && find_party_slot_by_species(env, *receive_species).has_value()
        ){
            env.console.log(std::format("MailGlitch: {} already in party; skipping in-game trade.", *receive_species));
            return Pokemon::outcome_ok();
        }
        return Pokemon::outcome_failed("missing_trade_species", offer_species);
    }

    env.console.log(std::format("MailGlitch: trading {} (party slot {}).", offer_species, *offer_slot + 1));
    pbf_press_button(env.context, BUTTON_A, 160ms, 1200ms);
    //  "Do you have a X? Want to trade it for my Y?" -> YES.
    answer_yes_no(env.console, env.context, true, BUTTON_A, DIALOG_TIMEOUT);

    //  Wait for Party Menu to open to pick the offered Pokemon.
    PartyMenuWatcher party_menu(COLOR_RED);
    env.context.wait_for_all_requests();
    if (wait_until(env.console, env.context, 15000ms, {party_menu}) < 0){
        return Pokemon::outcome_failed("trade_no_party_menu", offer_species);
    }
    pbf_wait(env.context, 500ms);
    tap_dpad_times(env.context, DPAD_DOWN, *offer_slot);
    //  Select mon -> SUMMARY / TRADE (cursor starts on SUMMARY? No: pokefirered EventScript_ChooseMonForInGameTrade
    //  opens party menu in PARTY_MENU_TYPE_INGAME_TRADE where pressing A directly selects the mon!).
    pbf_press_button(env.context, BUTTON_A, 160ms, 1000ms);
    //  The trade cutscene shows a blue battle-style dialog ("Take good care of ...!")
    //  that only closes on A/B (and that BattleScreenWatcher treats as a battle screen,
    //  causing mash_until_overworld to stall). Tap A until the cutscene fades back to
    //  the NPC's white post-trade dialog, then clear to overworld.
    WhiteDialogWatcher post_trade_dialog(COLOR_RED);
    run_until<ProControllerContext>(
        env.console, env.context,
        [](ProControllerContext& context){
            for (int c = 0; c < 45; c++){
                pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
            }
        },
        {{post_trade_dialog}}
    );
    mash_until_overworld(env.console, env.context, DIALOG_TIMEOUT);
    env.party.invalidate();
    return Pokemon::outcome_ok();
}

//  Swaps a species or explicit slot to a target slot (1-based in JSON).
Outcome run_party_swap(StoryEnv& env, const StoryStep& step){
    const std::string* species = game_arg<std::string>(step.args, "species");
    const std::optional<size_t> target_slot = counted_arg(step.args, "target_slot", PARTY_SLOTS);
    const std::optional<size_t> slot_a = counted_arg(step.args, "slot_a", PARTY_SLOTS);
    const std::optional<size_t> slot_b = counted_arg(step.args, "slot_b", PARTY_SLOTS);

    size_t from = 0;
    size_t to = 0;
    if (species != nullptr && !species->empty() && target_slot){
        const std::optional<size_t> found = find_party_slot_by_species(env, *species);
        if (!found){
            return Pokemon::outcome_failed("party_swap_missing", *species);
        }
        from = *found;
        to = *target_slot;
    }else if (slot_a && slot_b){
        from = *slot_a;
        to = *slot_b;
    }else{
        return Pokemon::outcome_failed("bad_args", std::string(PARTY_SWAP_ACTION));
    }

    if (from == to){
        env.console.log(std::format("MailGlitch: party slot {} already in place.", to + 1));
        return Pokemon::outcome_ok();
    }
    if (!swap_party_slots(env, from, to)){
        return Pokemon::outcome_failed("party_swap_failed", std::format("{} <-> {}", from + 1, to + 1));
    }
    return Pokemon::outcome_ok();
}

//  Writes 1 Easy Chat word on the Mail editor screen and confirms with START -> YES.
//  Verified against pokefirered src/easy_chat_2.c:
//    State 0 (SelectField) -> A -> State 2 (SelectGroup) -> A -> State 3 (SelectWord) -> A -> State 0.
//    START (BUTTON_PLUS) -> Confirm_CreateYesNoMenu (cursor on YES) -> A confirms mail.
void write_single_word_mail(ConsoleHandle& console, ProControllerContext& context){
    console.log("MailGlitch: writing 1 Easy Chat word on Retro Mail and confirming.");
    pbf_wait(context, 2500ms);
    //  Pick word 0 in group 0 (A, A, A).
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
    //  START -> "The MAIL message is as shown. Okay?" YES/NO (YES on top).
    pbf_press_button(context, BUTTON_PLUS, 160ms, 1200ms);
    pbf_press_button(context, BUTTON_A, 160ms, 2000ms);
    mash_until_overworld(console, context, DIALOG_TIMEOUT);
}

//  Gives an item from Items pocket (including Retro Mail) or a Berry from Key Items -> Berry Pouch
//  to a party member (identified by `species` or 1-based `target_slot`).
Outcome run_give_bag_item(StoryEnv& env, const StoryStep& step){
    const std::string& item = Pokemon::story_arg<std::string>(step.args, "item");
    const std::string* species = game_arg<std::string>(step.args, "species");
    const std::optional<size_t> explicit_slot = counted_arg(step.args, "target_slot", PARTY_SLOTS);

    std::optional<size_t> slot = explicit_slot;
    if (species != nullptr && !species->empty()){
        slot = find_party_slot_by_species(env, *species);
    }
    if (!slot){
        return Pokemon::outcome_failed("give_item_no_target", item);
    }

    if (is_berry_item(item)){
        env.console.log(std::format("MailGlitch: giving {} from Berry Pouch to party slot {}.", item, *slot + 1));
        open_bag_pocket_top(env.console, env.context, /*Key Items=*/1);
        const std::optional<size_t> pouch_row = find_item_row_in_open_pocket(env.console, env.context, env.language, "berry-pouch");
        if (!pouch_row){
            mash_until_overworld(env.console, env.context, DIALOG_TIMEOUT);
            return Pokemon::outcome_failed("no_berry_pouch", item);
        }
        tap_dpad_times(env.context, DPAD_DOWN, *pouch_row);
        //  A on BERRY POUCH -> OPEN (row 0) -> A opens Berry Pouch.
        pbf_press_button(env.context, BUTTON_A, 160ms, 800ms);
        pbf_press_button(env.context, BUTTON_A, 160ms, 2000ms);
        //  Reset Berry Pouch cursor to top row.
        for (int c = 0; c < 12; c++){
            pbf_press_dpad(env.context, DPAD_UP, 80ms, 100ms);
        }
        env.context.wait_for_all_requests();
        //  Identify target berry by its language-free 2-digit index ("No.07" / "No.10").
        const std::optional<size_t> berry_row = find_berry_row_in_pouch(env.console, env.context, item);
        const size_t row = berry_row.value_or(0);
        if (!berry_row){
            env.console.log(std::format("MailGlitch: couldn't match {} index in Berry Pouch; using row 1.", item), COLOR_ORANGE);
        }
        tap_dpad_times(env.context, DPAD_DOWN, row);
        //  Berry Pouch field context menu (pokefirered/src/berry_pouch.c:187-192):
        //    0 = USE, 1 = GIVE, 2 = TOSS, 3 = EXIT.
        pbf_press_button(env.context, BUTTON_A, 160ms, 800ms);
        tap_dpad_times(env.context, DPAD_DOWN, 1);
        pbf_press_button(env.context, BUTTON_A, 160ms, 1500ms);
    }else{
        env.console.log(std::format("MailGlitch: giving {} from Items pocket to party slot {}.", item, *slot + 1));
        open_bag_pocket_top(env.console, env.context, /*Items=*/0);
        const std::optional<size_t> item_row = find_item_row_in_open_pocket(env.console, env.context, env.language, item);
        if (!item_row){
            mash_until_overworld(env.console, env.context, DIALOG_TIMEOUT);
            return Pokemon::outcome_failed("missing_bag_item", item);
        }
        tap_dpad_times(env.context, DPAD_DOWN, *item_row);
        //  Items pocket context menu (pokefirered src/item_menu.c):
        //    Normal items: 0 = USE, 1 = GIVE, 2 = TOSS, 3 = CANCEL.
        //    Mail items:   0 = CHECK, 1 = GIVE, 2 = TOSS, 3 = CANCEL.
        //  GIVE is always row 1!
        pbf_press_button(env.context, BUTTON_A, 160ms, 800ms);
        pbf_press_dpad(env.context, DPAD_DOWN, MENU_TAP, MENU_WAIT);
        pbf_press_button(env.context, BUTTON_A, 160ms, 1500ms);
    }

    //  Wait for Party Menu to choose recipient.
    PartyMenuWatcher party_menu(COLOR_RED);
    env.context.wait_for_all_requests();
    if (wait_until(env.console, env.context, 15000ms, {party_menu}) < 0){
        mash_until_overworld(env.console, env.context, DIALOG_TIMEOUT);
        return Pokemon::outcome_failed("give_item_no_party_menu", item);
    }
    pbf_wait(env.context, 400ms);
    tap_dpad_times(env.context, DPAD_DOWN, *slot);
    pbf_press_button(env.context, BUTTON_A, 160ms, 1200ms);

    //  If the Pokemon is already holding an item, confirm "Switch items?" YES.
    env.context.wait_for_all_requests();
    VideoSnapshot check = env.console.video().snapshot();
    if (check && yes_no_cursor(check)){
        env.console.log("MailGlitch: target already holds an item; replacing it.", COLOR_ORANGE);
        pbf_press_button(env.context, BUTTON_A, 160ms, 1500ms);
    }

    if (item == "retro-mail"){
        write_single_word_mail(env.console, env.context);
    }else{
        mash_until_overworld(env.console, env.context, DIALOG_TIMEOUT);
    }
    return Pokemon::outcome_ok();
}

//  Verifies that `species` (MIMIEN) holds a normal item icon (the recycled Berry)
//  rather than Mail or empty on the Party screen.
Outcome run_verify_mail_glitch(StoryEnv& env, const StoryStep& step){
    const std::string& species = Pokemon::story_arg<std::string>(step.args, "species");
    const std::optional<size_t> slot = find_party_slot_by_species(env, species);
    if (!slot){
        return Pokemon::outcome_failed("verify_missing_species", species);
    }
    open_party_menu_from_overworld(env.console, env.context, start_menu_context(StartMenu::FULL));
    pbf_wait(env.context, 800ms);
    env.context.wait_for_all_requests();
    const VideoSnapshot screen = env.console.video().snapshot();
    PartyHeldItemDetector held_item(COLOR_GREEN, &env.console.overlay(), static_cast<PartySlot>(*slot), env.language);
    const bool has_item_icon = screen && held_item.detect(screen);
    mash_until_overworld(env.console, env.context, DIALOG_TIMEOUT);
    if (!has_item_icon){
        return Pokemon::outcome_failed(
            "mail_glitch_not_active",
            std::format("{} (slot {}) does not show a held item icon after Recycle", species, *slot + 1)
        );
    }
    env.console.log(std::format("MailGlitch: verified! {} (slot {}) holds the recycled Berry (Mail Glitch active).", species, *slot + 1), COLOR_BLUE);
    return Pokemon::outcome_ok();
}

} // namespace


std::vector<Pokemon::StoryActionSpec> mail_glitch_action_specs(){
    return {
        {.kind = StoryStepKind::GAME, .name = std::string(BUY_COINS_ACTION), .args = {
            required_arg("coins", StoryArgType::INTEGER, "total Game Corner coins to buy (multiples of 50)"),
        }},
        {.kind = StoryStepKind::GAME, .name = std::string(REDEEM_PRIZE_MON_ACTION), .args = {
            required_arg("slot", StoryArgType::INTEGER, "0-based prize Pokemon menu slot (0=Abra, 3=Scyther)"),
        }},
        {.kind = StoryStepKind::GAME, .name = std::string(INGAME_TRADE_ACTION), .args = {
            required_arg("species", StoryArgType::WORD, "offered species slug in party, e.g. \"spearow\" or \"abra\""),
            absent_arg("receive", StoryArgType::WORD, "received species slug for idempotent skip, e.g. \"farfetchd\""),
        }},
        {.kind = StoryStepKind::GAME, .name = std::string(PARTY_SWAP_ACTION), .args = {
            absent_arg("species", StoryArgType::WORD, "species slug to move to target_slot"),
            absent_arg("target_slot", StoryArgType::INTEGER, "1-based destination party slot"),
            absent_arg("slot_a", StoryArgType::INTEGER, "1-based first slot when swapping by index"),
            absent_arg("slot_b", StoryArgType::INTEGER, "1-based second slot when swapping by index"),
        }},
        {.kind = StoryStepKind::GAME, .name = std::string(GIVE_BAG_ITEM_ACTION), .args = {
            required_arg("item", StoryArgType::WORD, "item or berry slug, e.g. \"retro-mail\" or \"sitrus-berry\""),
            absent_arg("species", StoryArgType::WORD, "recipient species slug"),
            absent_arg("target_slot", StoryArgType::INTEGER, "1-based recipient party slot"),
        }},
        {.kind = StoryStepKind::GAME, .name = std::string(SWITCH_GRIND_ACTION), .args = {
            required_arg("species", StoryArgType::WORD, "trainee species slug (e.g. \"mr-mime\" or \"farfetchd\")"),
            required_arg("sweeper", StoryArgType::WORD, "sweeper species slug (e.g. \"blastoise\")"),
            required_arg("target_level", StoryArgType::INTEGER, "level to reach (2-100)"),
            required_arg("learn_move", StoryArgType::WORD, "move slug to ensure is learned (e.g. \"recycle\", \"knock-off\")"),
            optional_arg("learn_slot", StoryArgType::INTEGER, int64_t(1), "1-based move slot to overwrite on move learn prompts"),
            optional_arg("pace", StoryArgType::DIRECTION, Direction::EAST, "pacing direction on the encounter patch"),
            optional_arg("heal_pad_steps_east", StoryArgType::INTEGER, int64_t(0), "tiles east from patch anchor to Pokemon Tower 5F PurifiedZone (0 = no pad)"),
            optional_arg("heal_every", StoryArgType::INTEGER, int64_t(10), "battles between PurifiedZone heals"),
            optional_arg("encounters", StoryArgType::INTEGER, int64_t(300), "maximum wild battles before failing"),
        }},
        {.kind = StoryStepKind::GAME, .name = std::string(PREPARE_BERRY_HP_ACTION), .args = {
            required_arg("species", StoryArgType::WORD, "low-level Berry holder species slug to faint and Revive"),
            required_arg("sweeper", StoryArgType::WORD, "sweeper species slug to flee after faint"),
            optional_arg("pace", StoryArgType::DIRECTION, Direction::WEST, "pacing direction in grass"),
        }},
        {.kind = StoryStepKind::GAME, .name = std::string(MAIL_GLITCH_DOUBLE_BATTLE_ACTION), .args = {
            required_arg("knock_off_species", StoryArgType::WORD, "Knock Off user species (\"farfetchd\")"),
            required_arg("recycle_species", StoryArgType::WORD, "Recycle user species (\"mr-mime\")"),
            required_arg("sweeper", StoryArgType::WORD, "sweeper species (\"blastoise\")"),
            optional_arg("step_direction", StoryArgType::DIRECTION, Direction::NORTH, "direction to step into Twins Eli & Anne sightline"),
        }},
        {.kind = StoryStepKind::GAME, .name = std::string(VERIFY_MAIL_GLITCH_ACTION), .args = {
            required_arg("species", StoryArgType::WORD, "species whose Mail Glitch held-item icon to verify (\"mr-mime\")"),
        }},
    };
}

std::optional<size_t> find_party_slot_by_species(StoryEnv& env, std::string_view species){
    const SpeciesFinder finder = console_species_finder(env.console, env.context, env.language, env.party);
    return finder(species);
}

bool swap_party_slots(StoryEnv& env, size_t from_slot, size_t to_slot){
    if (from_slot == to_slot){
        return true;
    }
    if (from_slot >= PARTY_SLOTS || to_slot >= PARTY_SLOTS){
        return false;
    }
    env.console.log(std::format("MailGlitch: swapping party slot {} <-> slot {}.", from_slot + 1, to_slot + 1));
    open_party_menu_from_overworld(env.console, env.context, start_menu_context(StartMenu::FULL));
    pbf_wait(env.context, 600ms);
    tap_dpad_times(env.context, DPAD_DOWN, from_slot);

    PartySelectionWatcher mon_menu(COLOR_RED);
    env.context.wait_for_all_requests();
    pbf_press_button(env.context, BUTTON_A, 160ms, 0ms);
    if (wait_until(env.console, env.context, 5000ms, {mon_menu}) < 0){
        mash_until_overworld(env.console, env.context, DIALOG_TIMEOUT);
        return false;
    }
    pbf_wait(env.context, 300ms);
    //  Language-free SWITCH selection (pokefirered/src/party_menu.c:2963-2988, 3069-3072):
    //  For >= 2 non-Egg mons, numActions >= 4 so the menu wraps, and the bottom 3 rows
    //  are always [SWITCH, ITEM/MAIL, CANCEL] regardless of field moves or language.
    //  Starting on SUMMARY (row 0), DPAD_UP x 3 always lands on SWITCH.
    tap_dpad_times(env.context, DPAD_UP, 3);
    pbf_press_button(env.context, BUTTON_A, 160ms, 600ms);

    if (to_slot > from_slot){
        tap_dpad_times(env.context, DPAD_DOWN, to_slot - from_slot);
    }else{
        tap_dpad_times(env.context, DPAD_UP, from_slot - to_slot);
    }
    pbf_press_button(env.context, BUTTON_A, 160ms, 1000ms);
    mash_until_overworld(env.console, env.context, DIALOG_TIMEOUT);

    env.party.invalidate();
    if (from_slot == 0 || to_slot == 0){
        read_lead_moves(env, StartMenu::FULL);
    }
    return true;
}

std::optional<Outcome> run_mail_glitch_game_step(StoryEnv& env, RunState& state, const StoryStep& step){
    if (step.action == BUY_COINS_ACTION){
        return run_buy_coins(env, step);
    }
    if (step.action == REDEEM_PRIZE_MON_ACTION){
        return run_redeem_prize_mon(env, step);
    }
    if (step.action == INGAME_TRADE_ACTION){
        return run_ingame_trade(env, step);
    }
    if (step.action == PARTY_SWAP_ACTION){
        return run_party_swap(env, step);
    }
    if (step.action == GIVE_BAG_ITEM_ACTION){
        return run_give_bag_item(env, step);
    }
    if (step.action == SWITCH_GRIND_ACTION){
        return run_switch_grind_step(env, state, step);
    }
    if (step.action == PREPARE_BERRY_HP_ACTION){
        return run_prepare_berry_hp_step(env, state, step);
    }
    if (step.action == MAIL_GLITCH_DOUBLE_BATTLE_ACTION){
        return run_mail_glitch_double_battle_step(env, state, step);
    }
    if (step.action == VERIFY_MAIL_GLITCH_ACTION){
        return run_verify_mail_glitch(env, step);
    }
    return std::nullopt;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
