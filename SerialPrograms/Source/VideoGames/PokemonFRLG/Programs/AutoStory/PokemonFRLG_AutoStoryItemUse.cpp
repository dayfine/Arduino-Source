/*  AutoStory Item Use
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryItemUse.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using namespace std::chrono_literals;


MoveSpeed move_speed(const StoryStep& step){
    if (step.variant == BIKE_VARIANT){
        return MoveSpeed::BIKE;
    }
    return step.variant == RUN_VARIANT ? MoveSpeed::RUN : MoveSpeed::WALK;
}

std::optional<BagPocket> usable_item_pocket(std::string_view item){
    if (is_repel(item) || item == "escape-rope"){
        return BagPocket::ITEMS;
    }
    if (item == "bicycle"){
        return BagPocket::KEY_ITEMS;
    }
    return std::nullopt;
}
bool is_repel(std::string_view item){
    return item == "repel" || item == "super-repel" || item == "max-repel";
}

std::string use_item_variant(std::string_view item){
    return std::string(USE_ITEM_PREFIX) + std::string(item);
}
std::optional<std::string> parse_use_item_variant(std::string_view variant){
    if (!variant.starts_with(USE_ITEM_PREFIX)){
        return std::nullopt;
    }
    const std::string_view item = variant.substr(USE_ITEM_PREFIX.size());
    if (!usable_item_pocket(item)){
        return std::nullopt;
    }
    return std::string(item);
}
bool is_item_use_variant(std::string_view variant){
    return variant == RIDE_BIKE_VARIANT || parse_use_item_variant(variant).has_value();
}

std::optional<size_t> bag_row_of(const std::vector<BagRow>& rows, std::string_view item){
    for (size_t row = 0; row < rows.size(); row++){
        if (rows[row].item == item && rows[row].count != 0){
            return row;
        }
    }
    return std::nullopt;
}

const char* item_use_end_name(ItemUseEnd end){
    switch (end){
    case ItemUseEnd::USED:
        return "used";
    case ItemUseEnd::NONE_LEFT:
        return "none_left";
    case ItemUseEnd::NO_BAG:
        return "no_bag";
    case ItemUseEnd::UNKNOWN_ITEM:
        return "unknown_item";
    }
    return "?";
}

ItemUseEnd use_item_from_bag(Logger& logger, ItemUseActions& actions, std::string_view item){
    const std::optional<BagPocket> pocket = usable_item_pocket(item);
    if (!pocket){
        return ItemUseEnd::UNKNOWN_ITEM;
    }
    if (!actions.open_pocket(*pocket)){
        logger.log("Bag: the bag didn't open.");
        return ItemUseEnd::NO_BAG;
    }
    const std::optional<size_t> row = bag_row_of(actions.read_pocket(), item);
    if (!row){
        logger.log("Bag: no " + std::string(item) + " left.");
        actions.close_bag();
        return ItemUseEnd::NONE_LEFT;
    }
    logger.log("Bag: using the " + std::string(item) + " on row " + std::to_string(*row + 1) + ".");
    actions.use_row(*row);
    actions.close_bag();
    return ItemUseEnd::USED;
}

ItemUseEnd renew_repel(Logger& logger, ItemUseActions& actions, std::string_view item){
    if (!is_repel(item)){
        return ItemUseEnd::UNKNOWN_ITEM;
    }
    logger.log("Bag: the " + std::string(item) + " wore off.");
    return use_item_from_bag(logger, actions, item);
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
//  Out of the bag and its messages, or the Escape Rope's warp.
constexpr Milliseconds BAG_CLOSE_TIMEOUT = 30000ms;

} // namespace

ConsoleItemUseActions::ConsoleItemUseActions(ConsoleHandle& console, ProControllerContext& context, Language language)
    : m_console(console)
    , m_context(context)
    , m_language(language)
{}

bool ConsoleItemUseActions::open_pocket(BagPocket pocket){
    open_bag_from_overworld(m_console, m_context);
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

void ConsoleItemUseActions::close_bag(){
    //  B until the overworld: past "<player> used the REPEL.", out of the
    //  bag and the START menu. The Escape Rope and the Bicycle leave the
    //  bag by themselves.
    mash_until_overworld(m_console, m_context, BAG_CLOSE_TIMEOUT);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
