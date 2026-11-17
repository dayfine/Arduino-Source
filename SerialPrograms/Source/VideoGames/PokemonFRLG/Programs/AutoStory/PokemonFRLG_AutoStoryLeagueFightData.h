/*  AutoStory League Fight Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One fight of the League plan (PokemonFRLG_AutoStoryLeaguePlanData.h):
 *  finding it and its foes, what it holds back for later fights, and when
 *  a try of it ends early or passes (frlg_league_final.py's accept rule).
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

//  The foe at party slot `fallen` has just fainted with our Speed at
//  `speed_stage` and `used` spent in the fight so far: the plan's early
//  reset (Bruno's after Onix #1) ends the try.
bool league_early_reset(const LeagueFightData& fight, size_t fallen, int speed_stage, const SlugCounts& used);

//  How a try of a fight ended.
struct LeagueTry{
    bool won = false;
    //  A Roar or Whirlwind landed in a fight that resets on one.
    bool roared = false;
    bool early_reset = false;
    //  Every item it spent, the top-up before it included.
    SlugCounts used = {};
};

//  The try passes: a win, not reset, spending no item over its cap.
bool league_try_passes(const LeagueFightData& fight, const LeagueTry& attempt);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
