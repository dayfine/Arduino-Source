/*  AutoStory Viridian Mart
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string_view>
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
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


Pokemon::Outcome viridian_shop(StoryEnv& env, const StoryStep& step){
    const std::optional<std::vector<MartOrder>> orders = viridian_mart_orders(Pokemon::story_arg<std::vector<Pokemon::StoryItem>>(step.args, "items"));
    if (!orders){
        return Pokemon::outcome_failed("unknown_item", step.name);
    }
    //  2000 of the 3000 a new game starts with, plus the rival's prize.
    buy_items(env.console, env.context, *orders, env.checks, env.language);
    for (const Pokemon::StoryItem& item : Pokemon::story_arg<std::vector<Pokemon::StoryItem>>(step.args, "items")){
        env.supplies.*viridian_mart_item(item.item)->count += (int)item.quantity;
    }
    if (!checks_screen(env.checks)){
        return Pokemon::outcome_ok();
    }
    //  The counts from the bag, not from the orders: a wrong row once bought
    //  Parlyz Heals for Antidotes unnoticed (console B, 2026-11-01).
    const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language);
    if (rows.empty()){
        env.console.log("Story: couldn't read the Items pocket; keeping the counts ordered.", COLOR_ORANGE);
        return Pokemon::outcome_ok();
    }
    for (const std::string& item : set_from_bag(env.supplies, rows)){
        env.console.log(std::format(
            "Story: couldn't read how many {} the bag holds; counting {}.",
            item, item_count(env.supplies, item)
        ), COLOR_ORANGE);
    }
    return Pokemon::outcome_ok();
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
