/*  AutoStory Viridian Mart
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string_view>
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryViridianMart.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Viridian Mart before the Boulder Badge (pokefirered ViridianCity_Mart_Items):
//  POKE BALL, POTION (300), ANTIDOTE (100), PARLYZ HEAL (200). Each item
//  the story buys: its row, and where the bag's count is kept.
struct MartItem{
    std::string_view item;
    int row;
    int Supplies::* count;
    //  The name the item list reader reads (read_item_list_name()).
    std::string_view slug;
};
constexpr MartItem VIRIDIAN_MART_ITEMS[] = {
    {.item = "potion", .row = 1, .count = &Supplies::potions, .slug = "potion"},
    {.item = "antidote", .row = 2, .count = &Supplies::antidotes, .slug = "antidote"},
    {.item = "parlyz_heal", .row = 3, .count = &Supplies::parlyz_heals, .slug = "parlyz-heal"},
};

const MartItem* viridian_mart_item(const std::string& name){
    for (const MartItem& item : VIRIDIAN_MART_ITEMS){
        if (name == item.item){
            return &item;
        }
    }
    return nullptr;
}

} // namespace


std::optional<Pokemon::StoryFailure> viridian_shop(StoryEnv& env, const StoryStep& step){
    const std::vector<Pokemon::StoryItem>& items = frlg_arg<std::vector<Pokemon::StoryItem>>(step.args, "items");
    const std::optional<std::vector<MartOrder>> orders = viridian_mart_orders(items);
    if (!orders){
        return frlg_failure("unknown_item", step.name);
    }
    //  2000 of the 3000 a new game starts with, plus the rival's prize.
    buy_items(env.console, env.context, *orders, env.checks, env.language);
    for (const Pokemon::StoryItem& item : items){
        env.supplies.*viridian_mart_item(item.item)->count += static_cast<int>(item.quantity);
    }
    if (!Pokemon::runs_check(env.checks, Pokemon::StoryCheckKind::BAG_CONTENTS)){
        return std::nullopt;
    }
    //  A check of what was bought: a wrong row once bought Parlyz Heals for
    //  Antidotes unnoticed (console B, 2026-11-01). It changes no count:
    //  the next battle that doesn't know them reads the pocket itself.
    const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language);
    if (rows.empty()){
        env.console.log("Story: couldn't read the Items pocket to check the order.", COLOR_ORANGE);
        return std::nullopt;
    }
    Supplies read = env.supplies;
    set_from_bag(read, rows);
    for (const Pokemon::StoryItem& item : items){
        const int counted = env.supplies.*viridian_mart_item(item.item)->count;
        const int held = read.*viridian_mart_item(item.item)->count;
        if (held != counted){
            env.console.log(std::format("Story: the bag holds {} {}, the story counts {}.", held, item.item, counted), COLOR_ORANGE);
        }
    }
    return std::nullopt;
}

std::optional<std::vector<MartOrder>> viridian_mart_orders(const std::vector<Pokemon::StoryItem>& items){
    std::vector<MartOrder> orders;
    for (const Pokemon::StoryItem& wanted : items){
        const MartItem* found = viridian_mart_item(wanted.item);
        if (found == nullptr){
            return std::nullopt;
        }
        orders.push_back(MartOrder{found->row, (int)wanted.quantity, found->slug.data()});
    }
    return orders;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
