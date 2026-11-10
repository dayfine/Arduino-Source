/*  AutoStory Mart Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The MART step's plan: the stock it expects and the lines it buys, and
 *  its args in the story table.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMartPlan_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMartPlan_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "PokemonFRLG_AutoStoryBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  One item a Mart sells: its slug (as read_item_list_name() names it,
//  e.g. "parlyz-heal") and its price (pokefirered items.json).
struct MartStockItem{
    std::string_view item;
    size_t price;
};

//  The BUY list of the Mart on map `map` (e.g. "PewterCity_Mart"), in row
//  order, CANCEL not included; empty for a map whose stock isn't known.
const std::vector<MartStockItem>& mart_stock(std::string_view map);

//  One line of a buy list: an item and how many.
struct MartLine{
    std::string item = {};
    size_t quantity = 1;
};

//  What a Mart step buys.
struct MartPlan{
    //  The Mart's map, for its stock.
    std::string map = {};
    //  In priority order.
    std::vector<MartLine> lines = {};
    //  The most the step spends.
    size_t budget = 0;
    //  The least money the player has here, from the money plan: used
    //  when the money box can't be read.
    std::optional<size_t> money_floor = std::nullopt;
};

//  The GAME action of a Mart step: map WORD (required), the Mart's map;
//  budget INTEGER (required); floor INTEGER (optional), the money floor;
//  items ITEMS (required), the lines in order.
inline const std::string MART_ACTION = "mart";
//  `plan` as its action's args.
Pokemon::StoryActionArgs mart_args(const MartPlan& plan);
//  The plan a MART_ACTION step's args give; nothing if it has no line or
//  names an item its Mart doesn't sell.
std::optional<MartPlan> mart_plan_of(const Pokemon::StoryActionArgs& args);

//  The table's Mart step.
Pokemon::StoryStep mart_step(std::string name, std::string why, const MartPlan& plan);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
