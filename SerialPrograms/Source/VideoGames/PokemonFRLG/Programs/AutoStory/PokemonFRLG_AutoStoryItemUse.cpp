/*  AutoStory Item Use
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_PartyDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_BagDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryBagUse.h"
#include "PokemonFRLG_AutoStoryBag.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using namespace std::chrono_literals;


//  The most a use_item step's count may be.
constexpr size_t MAX_USE_COUNT = 99;

MoveSpeed move_speed(const StoryStep& step){
    if (step.action == BIKE_VARIANT){
        return MoveSpeed::BIKE;
    }
    return step.action == RUN_VARIANT ? MoveSpeed::RUN : MoveSpeed::WALK;
}

std::optional<BagPocket> usable_item_pocket(std::string_view item){
    if (is_repel(item) || item == "escape-rope"){
        return BagPocket::ITEMS;
    }
    if (is_party_item(item) || is_lead_recovery_item(item)){
        return BagPocket::ITEMS;
    }
    if (item == "bicycle"){
        return BagPocket::KEY_ITEMS;
    }
    return std::nullopt;
}
bool is_party_item(std::string_view item){
    return item == "rare-candy";
}
bool is_lead_recovery_item(std::string_view item){
    return item == "elixir" || item == "full-restore";
}
bool is_repel(std::string_view item){
    return item == "repel" || item == "super-repel" || item == "max-repel";
}

std::vector<std::string> repel_renew_order(std::string_view used, std::optional<int> remaining_steps){
    struct Repel{
        std::string_view item;
        int steps;
    };
    static constexpr Repel REPELS[] = {{"repel", 100}, {"super-repel", 200}, {"max-repel", 250}};
    //  Where the order starts: the smallest that lasts the steps (the
    //  largest if none does), or the one used.
    size_t start = std::size(REPELS) - 1;
    for (size_t c = 0; c < std::size(REPELS); c++){
        if (remaining_steps ? REPELS[c].steps >= *remaining_steps : REPELS[c].item == used){
            start = c;
            break;
        }
    }
    if (!remaining_steps && !is_repel(used)){
        start = 0;
    }
    std::vector<std::string> order;
    for (size_t c = start; c < std::size(REPELS); c++){
        order.emplace_back(REPELS[c].item);
    }
    for (size_t c = start; c-- > 0;){
        order.emplace_back(REPELS[c].item);
    }
    return order;
}

std::optional<std::string> parse_use_item_step(const Pokemon::StoryStep& step){
    const std::string* item = step.action == USE_ITEM_VARIANT ? game_arg<std::string>(step.args, "item") : nullptr;
    if (item == nullptr || step.args.contains("slot") || !usable_item_pocket(*item) || is_lead_recovery_item(*item)){
        return std::nullopt;
    }
    if (step.args.contains("count") && !number_arg(step.args, "count", 1, MAX_USE_COUNT)){
        return std::nullopt;
    }
    return *item;
}
size_t use_item_count(const Pokemon::StoryStep& step){
    return number_arg(step.args, "count", 1, MAX_USE_COUNT).value_or(1);
}
bool is_item_use_step(const Pokemon::StoryStep& step){
    const bool bike = step.action == RIDE_BIKE_VARIANT || step.action == REGISTER_BIKE_VARIANT;
    return (bike && step.args.empty()) || parse_use_item_step(step).has_value();
}

std::optional<size_t> bag_row_of(const std::vector<BagRow>& rows, std::string_view item){
    for (size_t row = 0; row < rows.size(); row++){
        if (rows[row].item == item && rows[row].count != 0){
            return row;
        }
    }
    return std::nullopt;
}

std::string item_use_end_name(ItemUseEnd end){
    switch (end){
    case ItemUseEnd::USED:
        return "used";
    case ItemUseEnd::NONE_LEFT:
        return "none_left";
    case ItemUseEnd::NO_BAG:
        return "no_bag";
    case ItemUseEnd::UNKNOWN_ITEM:
        return "unknown_item";
    case ItemUseEnd::NOT_USED:
        return "not_used";
    case ItemUseEnd::LINGERED:
        return "lingered";
    }
    return "?";
}

namespace{

//  After USE on the Repel `item`: what the game said, LINGERED if one
//  was still on. A message not read is taken as used, as before it was
//  read.
ItemUseEnd repel_end(Logger& logger, ItemUseActions& actions, std::string_view item){
    switch (actions.repel_use()){
    case RepelUse::LINGERED:
        logger.log(std::format("Bag: the {} wasn't used: a Repel used before is still on.", item));
        return ItemUseEnd::LINGERED;
    case RepelUse::UNREAD:
        logger.log(std::format("Bag: what the {} said wasn't read: taken as used.", item));
        return ItemUseEnd::USED;
    case RepelUse::USED:
        break;
    }
    return ItemUseEnd::USED;
}

//  Opens the pocket of `items` (all in one), finds the first of them it
//  holds and `uses` it, then back to the overworld.
ItemUseEnd with_bag_row(
    Logger& logger, ItemUseActions& actions, const std::vector<std::string>& items, bool registers
){
    const std::optional<BagPocket> pocket = items.empty() ? std::nullopt : usable_item_pocket(items[0]);
    if (!pocket){
        return ItemUseEnd::UNKNOWN_ITEM;
    }
    if (!actions.open_pocket(*pocket)){
        logger.log("Bag: the bag didn't open.");
        return ItemUseEnd::NO_BAG;
    }
    const std::vector<BagRow> rows = actions.read_pocket();
    std::optional<size_t> row;
    std::string_view item = items[0];
    for (const std::string& candidate : items){
        row = bag_row_of(rows, candidate);
        if (row){
            item = candidate;
            break;
        }
    }
    if (!row){
        logger.log("Bag: no " + std::string(item) + " left.");
        actions.close_bag();
        return ItemUseEnd::NONE_LEFT;
    }
    logger.log("Bag: " + std::string(registers ? "registering" : "using") + " the " + std::string(item) + " on row " + std::to_string(*row + 1) + ".");
    if (registers){
        actions.register_row(*row);
    }else if (is_party_item(item)){
        if (!actions.use_row_on_lead(*row)){
            logger.log("Bag: the " + std::string(item) + " showed no level-up.");
            actions.close_bag();
            return ItemUseEnd::NOT_USED;
        }
    }else if (is_lead_recovery_item(item)){
        if (!actions.use_row_on_lead_to_bag(*row)){
            logger.log(std::format("Bag: never back at the bag after the {}.", item));
            actions.close_bag();
            return ItemUseEnd::NOT_USED;
        }
    }else{
        actions.use_row(*row);
    }
    const ItemUseEnd end = !registers && is_repel(item) ? repel_end(logger, actions, item) : ItemUseEnd::USED;
    actions.close_bag();
    return end;
}

} // namespace

ItemUseEnd use_item_from_bag(Logger& logger, ItemUseActions& actions, std::string_view item){
    return with_bag_row(logger, actions, {std::string(item)}, false);
}
ItemUses use_items_from_bag(Logger& logger, ItemUseActions& actions, std::string_view item, size_t count){
    size_t used = 0;
    while (used < count){
        const ItemUseEnd end = use_item_from_bag(logger, actions, item);
        if (end == ItemUseEnd::NONE_LEFT && used != 0){
            logger.log("Bag: " + std::to_string(used) + " of " + std::to_string(count) + " " + std::string(item) + " used: no more in the bag.");
            break;
        }
        if (end != ItemUseEnd::USED){
            return {end, used};
        }
        used++;
        if (is_repel(item)){
            //  A second while the first is on would only linger.
            break;
        }
    }
    return {ItemUseEnd::USED, used};
}
ItemUseEnd register_item_in_bag(Logger& logger, ItemUseActions& actions, std::string_view item){
    return with_bag_row(logger, actions, {std::string(item)}, true);
}

ItemUseEnd renew_repel(Logger& logger, ItemUseActions& actions, std::string_view used, std::optional<int> remaining_steps){
    if (!is_repel(used)){
        return ItemUseEnd::UNKNOWN_ITEM;
    }
    logger.log("Bag: the " + std::string(used) + " wore off: another Repel.");
    return with_bag_row(logger, actions, repel_renew_order(used, remaining_steps), false);
}


namespace{

//  The waits that worked on console B (2026-11-09, the Bicycle from Key
//  Items: TEACHY TV, TM CASE, HELIX FOSSIL, FAME CHECKER, S.S. TICKET,
//  BICYCLE; USE the first option, on the bike about 1.5 s after its A).
//  The bag slides in, and ignores presses until it is done.
constexpr Milliseconds BAG_SLIDE_IN = 1500ms;
//  A pocket switch (left or right), press and wait.
constexpr Milliseconds POCKET_TAP = 100ms;
constexpr Milliseconds POCKET_SWITCH_WAIT = 600ms;
//  A cursor step in a pocket, press and wait: 250 ms in all.
constexpr Milliseconds CURSOR_TAP = 100ms;
constexpr Milliseconds CURSOR_WAIT = 150ms;
//  A presses to the stats window, 800 ms each, and the tries (15 B
//  presses each) to get from it back to the bag.
constexpr int LEVEL_UP_PRESSES = 30;
constexpr int BACK_TO_BAG_ATTEMPTS = 8;
//  B presses through an item's message on the lead ("PP was restored.",
//  "<name>'s HP was restored by N point(s).") back to the bag: one or two
//  boxes take two, the rest is margin. Not timed on the console.
constexpr int MESSAGE_PRESSES = 6;
//  Out of the bag and its messages, or the Escape Rope's warp.
constexpr Milliseconds BAG_CLOSE_TIMEOUT = 30000ms;

} // namespace

ConsoleItemUseActions::ConsoleItemUseActions(ConsoleHandle& console, ProControllerContext& context, Language language, StartMenu menu)
    : m_console(console)
    , m_context(context)
    , m_language(language)
    , m_menu(menu)
{}

bool ConsoleItemUseActions::open_pocket(BagPocket pocket){
    open_bag_from_overworld(m_console, m_context, start_menu_context(m_menu));
    pbf_wait(m_context, BAG_SLIDE_IN);
    //  Left to the Items pocket, which doesn't wrap, then right to `pocket`.
    for (int c = 0; c < 3; c++){
        pbf_press_dpad(m_context, DPAD_LEFT, POCKET_TAP, POCKET_SWITCH_WAIT);
    }
    for (int c = 0; c < (int)pocket; c++){
        pbf_press_dpad(m_context, DPAD_RIGHT, POCKET_TAP, POCKET_SWITCH_WAIT);
    }
    for (int c = 0; c < 20; c++){
        pbf_press_dpad(m_context, DPAD_UP, 100ms, 100ms);
    }
    return true;
}

std::vector<BagRow> ConsoleItemUseActions::read_pocket(){
    m_context.wait_for_all_requests();
    VideoSnapshot screen = m_console.video().snapshot();
    if (!screen){
        return {};
    }
    return read_bag_pocket(m_console.logger(), m_language, screen);
}

void ConsoleItemUseActions::use_row(size_t row){
    for (size_t c = 0; c < row; c++){
        pbf_press_dpad(m_context, DPAD_DOWN, CURSOR_TAP, CURSOR_WAIT);
    }
    //  The item's menu, USE on top.
    pbf_press_button(m_context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, 1500ms);
}

//  Past the party list to the stats window of a Rare Candy.
bool ConsoleItemUseActions::wait_for_level_up_stats(){
    //  "<name> grew to Lv. N!", then the stats window (the RNG loop's
    //  waits, console-proven there).
    PartyLevelUpWatcher level_up(COLOR_RED, PartyLevelUpDialog::stats, m_language);
    m_context.wait_for_all_requests();
    const int stats = run_until<ProControllerContext>(
        m_console, m_context,
        [](ProControllerContext& context){
            for (int c = 0; c < LEVEL_UP_PRESSES; c++){
                pbf_press_button(context, BUTTON_A, 200ms, 800ms);
            }
        },
        {level_up}
    );
    if (stats < 0){
        m_console.log("Bag: no level-up stats window after the Rare Candy.", COLOR_RED);
    }
    return stats >= 0;
}

//  Out of the stats window and the party list back to the bag. A new move
//  is declined: B on "Delete a move?" asks "Stop learning?", A answers YES.
//  An evolution is stopped with B.
bool ConsoleItemUseActions::back_to_bag_declining_moves(){
    for (int attempt = 0; attempt < BACK_TO_BAG_ATTEMPTS; attempt++){
        BagWatcher bag(COLOR_RED);
        PartyMoveLearnWatcher move_learn(COLOR_RED);
        BattleDialogWatcher evolution(COLOR_RED);
        m_context.wait_for_all_requests();
        const int seen = run_until<ProControllerContext>(
            m_console, m_context,
            [](ProControllerContext& context){
                pbf_wait(context, 1000ms);
                for (int c = 0; c < 15; c++){
                    pbf_press_button(context, BUTTON_B, 200ms, 1800ms);
                }
            },
            {bag, move_learn, evolution}
        );
        if (seen == 0){
            return true;
        }
        if (seen == 1){
            m_console.log("Bag: a new move to learn: declined.");
            pbf_press_button(m_context, BUTTON_B, 200ms, 1800ms);
            pbf_press_button(m_context, BUTTON_A, 200ms, 2800ms);
        }else if (seen == 2){
            m_console.log("Bag: an evolution: stopped.");
            pbf_press_button(m_context, BUTTON_B, 200ms, 2800ms);
        }else{
            break;
        }
    }
    m_console.log("Bag: never back at the bag after the Rare Candy.", COLOR_RED);
    return false;
}

bool ConsoleItemUseActions::use_row_on_lead(size_t row){
    for (size_t c = 0; c < row; c++){
        pbf_press_dpad(m_context, DPAD_DOWN, CURSOR_TAP, CURSOR_WAIT);
    }
    //  The item's menu, USE, then the party list, whose cursor is on the
    //  lead, as for a Potion in battle.
    pbf_press_button(m_context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, 1500ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, 1000ms);
    return wait_for_level_up_stats() && back_to_bag_declining_moves();
}

bool ConsoleItemUseActions::use_row_on_lead_to_bag(size_t row){
    for (size_t c = 0; c < row; c++){
        pbf_press_dpad(m_context, DPAD_DOWN, CURSOR_TAP, CURSOR_WAIT);
    }
    //  The item's menu, USE, then the party list on the lead.
    pbf_press_button(m_context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, 1500ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, 1000ms);
    //  The message, then the game returns to the bag. B, not A: an A at
    //  the bag would open the next item's menu.
    BagWatcher bag(COLOR_RED);
    m_context.wait_for_all_requests();
    const int seen = run_until<ProControllerContext>(
        m_console, m_context,
        [](ProControllerContext& context){
            pbf_wait(context, 1000ms);
            for (int c = 0; c < MESSAGE_PRESSES; c++){
                pbf_press_button(context, BUTTON_B, 200ms, 1800ms);
            }
        },
        {bag}
    );
    if (seen < 0){
        m_console.log("Bag: no bag after the item's message on the lead.", COLOR_RED);
    }
    return seen >= 0;
}

RepelUse ConsoleItemUseActions::repel_use(){
    m_context.wait_for_all_requests();
    const VideoSnapshot screen = m_console.video().snapshot();
    return screen ? read_repel_use(m_language, screen) : RepelUse::UNREAD;
}

void ConsoleItemUseActions::register_row(size_t row){
    for (size_t c = 0; c < row; c++){
        pbf_press_dpad(m_context, DPAD_DOWN, CURSOR_TAP, CURSOR_WAIT);
    }
    //  The item's menu: USE, REGISTER, ... Not timed on the console: the
    //  USE waits.
    pbf_press_button(m_context, BUTTON_A, 160ms, 800ms);
    pbf_press_dpad(m_context, DPAD_DOWN, CURSOR_TAP, CURSOR_WAIT);
    pbf_press_button(m_context, BUTTON_A, 160ms, 1500ms);
}

void ConsoleItemUseActions::close_bag(){
    //  B until the overworld: past "<player> used the REPEL.", out of the
    //  bag and the START menu. The Escape Rope and the Bicycle leave the
    //  bag by themselves.
    mash_until_overworld(m_console, m_context, BAG_CLOSE_TIMEOUT);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
