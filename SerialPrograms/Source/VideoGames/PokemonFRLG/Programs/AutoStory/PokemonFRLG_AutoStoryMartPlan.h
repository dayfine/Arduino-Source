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

//  What the clerk pays for one `item` (half its pokefirered items.json
//  price): the items the story sells (the NUGGET); nothing for another,
//  or for one priced 0, which the clerk won't buy ("OH, no. I can't buy
//  that.", as the MOON STONE).
std::optional<size_t> sale_price(std::string_view item);

//  What a Mart step sells, then buys: each line's quantity.
struct MartPlan{
    //  The Mart's map, for its stock.
    std::string map = {};
    //  In priority order.
    std::vector<MartLine> lines = {};
    //  A line the money doesn't cover (the Mart refuses it, or its
    //  quantity box can't reach the count) is skipped with a log line,
    //  and the step goes on, instead of failing. With the money read on
    //  the BUY list, a line is cut to what the money covers instead.
    bool if_affordable = false;
    //  Sold at the same counter (SELL) before the buys, each from its
    //  pocket; a line the bag has none of is skipped, logged.
    std::vector<MartLine> sells = {};
};

//  The GAME action of a Mart step: map WORD (required), the Mart's map;
//  items ITEMS (required), the lines in order; if_affordable BOOLEAN
//  (absent: false), MartPlan::if_affordable; sell ITEMS (absent: none),
//  MartPlan::sells.
inline const std::string MART_ACTION = "mart";
//  `plan` as its action's args.
Pokemon::StoryActionArgs mart_args(const MartPlan& plan);
//  The plan a MART_ACTION step's args give; nothing if it has no line,
//  names an item its Mart doesn't sell, or sells one the clerk won't buy.
std::optional<MartPlan> mart_plan_of(const Pokemon::StoryActionArgs& args);

//  The table's Mart step, once per save (StoryStep::once_per_save): a
//  retry from the same save skips it when it had nothing to buy.
Pokemon::StoryStep mart_step(std::string name, std::string why, const MartPlan& plan);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
