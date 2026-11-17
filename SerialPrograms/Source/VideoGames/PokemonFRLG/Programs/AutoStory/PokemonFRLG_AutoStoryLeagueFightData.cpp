/*  AutoStory League Fight Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_AutoStoryLeagueFightData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


const LeagueFightData* league_fight_data(const LeaguePlanData& plan, std::string_view name){
    for (const LeagueFightData& fight : plan.fights){
        if (fight.name == name){
            return &fight;
        }
    }
    return nullptr;
}

const LeagueFoePlan* league_foe_plan(const LeagueFightData& fight, size_t slot, std::string_view species){
    if (slot < fight.foes.size() && fight.foes[slot].species == species){
        return &fight.foes[slot];
    }
    for (const LeagueFoePlan& foe : fight.foes){
        if (foe.species == species){
            return &foe;
        }
    }
    return nullptr;
}

int count_of(const SlugCounts& counts, std::string_view name){
    const auto it = counts.find(name);
    return it == counts.end() ? 0 : it->second;
}

int spendable(const LeagueFightData& fight, const SlugCounts& bag, std::string_view item){
    return count_of(bag, item) - count_of(fight.reserve_items, item);
}

int pp_above_reserve(const LeagueFightData& fight, std::string_view move, int pp){
    return pp - count_of(fight.reserve_pp, move);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
