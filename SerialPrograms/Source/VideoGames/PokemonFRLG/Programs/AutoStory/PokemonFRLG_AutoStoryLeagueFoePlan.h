/*  AutoStory League Foe Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One foe of a League fight as the plan (PokemonFRLG_AutoStoryLeaguePlanData.h)
 *  fights it: its move ranking, move sequence, setup goals and heal block.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeagueFoePlan_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeagueFoePlan_H

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Counts by item or move slug.
using SlugCounts = std::map<std::string, int, std::less<>>;

//  An X item (or Guard Spec) to use until its stage is reached.
struct LeagueSetupGoal{
    std::string item = {};
    int stage = 1;
    //  The move the stage is for: the goal is skipped while that move has
    //  no PP above the fight's reserve. None for Guard Spec.
    std::optional<std::string> serves = std::nullopt;
};

//  A foe's heal block: below `below` HP, the smallest spendable heal that
//  restores at least `heal_at_least` HP.
struct LeagueFoeHeal{
    int below = 0;
    int heal_at_least = 0;
    //  Before a sure KO too.
    bool heal_when_can_ko = false;
    //  The KO-risk heal rows still apply.
    bool ko_rule = true;
};

//  One foe of a fight, by its trainer's party slot.
struct LeagueFoePlan{
    std::string species = {};
    //  The ranking: the first with PP above the reserve is used.
    std::vector<std::string> moves = {};
    std::vector<LeagueSetupGoal> setup = {};
    //  Our n-th attack on this foe uses the n-th move here (while it has
    //  PP), then the ranking.
    std::vector<std::string> sequence = {};
    std::optional<LeagueFoeHeal> heal = std::nullopt;
    //  kill_order's row applies (never with a sequence).
    bool kill_order = true;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
