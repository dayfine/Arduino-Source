/*  AutoStory League Fight Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One fight of the League plan (PokemonFRLG_AutoStoryLeaguePlanData.h):
 *  finding it and its foes, and what it holds back for later fights.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeagueFightData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeagueFightData_H

#include <cstddef>
#include <string_view>
#include "PokemonFRLG_AutoStoryLeaguePlanData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The fight `name` in `plan`; none if it has none.
const LeagueFightData* league_fight_data(const LeaguePlanData& plan, std::string_view name);

//  The foe at party `slot`, or, when another species is there (a party cut
//  short), the first of `species`; none if the fight has none.
const LeagueFoePlan* league_foe_plan(const LeagueFightData& fight, size_t slot, std::string_view species);

//  `counts`'s count of `name`; 0 if it has none.
int count_of(const SlugCounts& counts, std::string_view name);

//  The bag's count of `item` above the fight's reserve for it.
int spendable(const LeagueFightData& fight, const SlugCounts& bag, std::string_view item);

//  `pp` of `move` above the fight's reserve for it.
int pp_above_reserve(const LeagueFightData& fight, std::string_view move, int pp);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
