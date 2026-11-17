/*  AutoStory League Plan Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League plan PokemonSimulation's planned policy runs
 *  (docs/battles/frlg-league-plan.md, format "frlg-league-plan/1"), built
 *  into the program from StoryData/frlg-league-plan.json: for each fight,
 *  each foe's move ranking and setup goals, what to restore before the
 *  fight, and the PP and items the fight must leave for later ones.
 *
 *  Items and moves are the program's slugs ("x-special", "surf"); the
 *  file's sim names ("xspecial") are mapped when it is read.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeaguePlanData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeaguePlanData_H

#include <map>
#include <optional>
#include <string>
#include <string_view>
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

//  One foe of a fight, by its trainer's party slot.
struct LeagueFoePlan{
    std::string species = {};
    //  The ranking: the first with PP above the reserve is used.
    std::vector<std::string> moves = {};
    std::vector<LeagueSetupGoal> setup = {};
};

//  One fight of the plan: its foes, the top-ups before it and its reserves.
struct LeagueFightData{
    //  "lorelei", "bruno", "agatha", "lance", "champion".
    std::string name = {};
    std::vector<LeagueFoePlan> foes = {};
    //  Before this fight: the HP to restore to, as a share of the maximum
    //  (none: no target), and each move's PP floor.
    std::optional<double> between_hp = std::nullopt;
    SlugCounts between_pp = {};
    //  PP this fight must not spend, by move, and the items it must leave
    //  in the bag.
    SlugCounts reserve_pp = {};
    SlugCounts reserve_items = {};
};

//  The whole plan: its fights in the League's order.
struct LeaguePlanData{
    std::vector<LeagueFightData> fights = {};
};

//  The plan in `json`; throws ParseException on another format or an
//  unknown item.
LeaguePlanData parse_league_plan(std::string_view json);

//  The plan built into the program (StoryData/frlg-league-plan.json).
const LeaguePlanData& league_plan_data();

//  The fight a battle step's trainer ("TRAINER_ELITE_FOUR_LORELEI",
//  "TRAINER_CHAMPION_FIRST_BULBASAUR") is, as the plan names it; none for
//  another trainer.
std::optional<std::string> league_fight_name(std::string_view trainer);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
