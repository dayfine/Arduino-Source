/*  AutoStory Battle Bag
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Color.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonFRLG_AutoStoryBattleBag.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryPocketList.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using namespace std::chrono_literals;


bool bag_row_is(
    ConsoleHandle& console, ProControllerContext& context,
    int row, int shown, const std::string& item, Pokemon::StoryCheckLevel checks, Language language
){
    if (!Pokemon::runs_check(checks, Pokemon::StoryCheckKind::MENU_ROW)){
        return true;
    }
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    const std::string read = screen ? read_item_list_name(console, language, screen, shown) : "";
    if (read == item){
        return true;
    }
    if (read.empty()){
        console.log(std::format("Bag: row {} unread, taking it as the {} by its position.", row + 1, item), COLOR_ORANGE);
        return true;
    }
    console.log(std::format("Bag: row {} reads \"{}\", not {}: not using it.", row + 1, read, item), COLOR_RED);
    dump_debug_image(console, "AutoStory/bag", std::format("row{}-not-{}", row + 1, item), screen);
    return false;
}

namespace{

//  `item` at its row as `supplies` has it, if it has one.
bool use_battle_item_row(const std::string& item, const Supplies& supplies, Logger& logger, BattleBag& bag){
    const int row = item_row(supplies, item);
    if (row < 0){
        logger.log(std::format("Battle: no {} in the Items pocket.", item));
        return false;
    }
    logger.log(std::format("Battle: using the {} in row {} of the Items pocket.", item, row + 1));
    return bag.use_row(supplies, row, item_shown_row(supplies, row), item);
}

//  The UP presses that bring the cursor to the pocket's top: its rows as
//  counted, and its CANCEL.
size_t rows_to_top(const Supplies& supplies){
    return supplies.pocket.size() + 1;
}

} // namespace

bool use_battle_item(const std::string& item, Logger& logger, BattleBag& bag, Supplies& supplies){
    if (use_battle_item_row(item, supplies, logger, bag)){
        return true;
    }
    logger.log(std::format("Battle: the {} isn't where the counts put it; reading the Items pocket again.", item));
    const std::vector<BagRow> rows = bag.read_pocket(supplies);
    if (rows.empty()){
        return false;
    }
    set_from_bag(supplies, rows);
    return use_battle_item_row(item, supplies, logger, bag);
}

ConsoleBattleBag::ConsoleBattleBag(
    ConsoleHandle& console, ProControllerContext& context, ItemTarget target, Pokemon::StoryCheckLevel checks, Language language
)
    : m_console(console)
    , m_context(context)
    , m_target(target)
    , m_checks(checks)
    , m_language(language)
{}

std::vector<BagRow> ConsoleBattleBag::read_pocket(const Supplies& supplies){
    return read_battle_pocket(m_console, m_context, m_language, rows_to_top(supplies));
}

bool ConsoleBattleBag::use_row(const Supplies& supplies, int row, int shown, const std::string& item){
    open_battle_bag(m_context, rows_to_top(supplies));
    for (int c = 0; c < row; c++){
        pbf_press_dpad(m_context, DPAD_DOWN, /*hold=*/160ms, /*release=*/240ms);
    }
    if (!bag_row_is(m_console, m_context, row, shown, item, m_checks, m_language)){
        pbf_press_button(m_context, BUTTON_B, /*hold=*/160ms, /*release=*/1500ms);
        return false;
    }
    //  The item, USE, then the starter in the party list. An X item's USE
    //  goes straight to the battle's text, which the loop reads.
    pbf_press_button(m_context, BUTTON_A, /*hold=*/160ms, /*release=*/800ms);
    pbf_press_button(m_context, BUTTON_A, /*hold=*/160ms, /*release=*/1500ms);
    if (m_target == ItemTarget::PARTY_MEMBER){
        pbf_press_button(m_context, BUTTON_A, /*hold=*/160ms, /*release=*/1000ms);
    }
    return true;
}

std::string supply_line(const Supplies& supplies){
    std::string line = std::format("{} Potions, {} Super Potions", supplies.potions, supplies.super_potions);
    for (const auto& [item, count] : supplies.other_counts){
        if (count > 0 && medicine_hp(item) > 0){
            line += std::format(", {} {}", count, item);
        }
    }
    line += std::format(
        ", {} Antidotes, {} Parlyz Heals, {} Awakenings, {} Full Heals",
        supplies.antidotes, supplies.parlyz_heals, supplies.awakenings, supplies.full_heals
    );
    return line;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
