/*  AutoStory League Plan Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League plan PokemonSimulation's planned policy runs
 *  (docs/battles/frlg-league-plan.md, format "frlg-league-plan/1",
 *  PokemonSimulation #115), built into the program unchanged from
 *  StoryData/frlg-league-plan.json: for each fight, each foe's move ranking,
 *  move sequence, setup goals and heal block, what to restore before the
 *  fight, the PP and items the fight must leave for later ones, its item
 *  caps, its reset rules and its pass figures.
 *
 *  Items and moves are the program's slugs ("x-special", "surf"); the
 *  file's sim names ("xspecial") are mapped when it is read.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeaguePlanData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeaguePlanData_H

#include <cstddef>
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

//  A try that ends early: when the foe at party slot `index` falls, our
//  Speed stage is below `speed_below` and at least `items_used` of each
//  item went into the fight by then.
struct LeagueEarlyReset{
    size_t index = 0;
    int speed_below = 0;
    SlugCounts items_used = {};
};

//  How often a try passes (a win within the caps): per try, the passes of
//  `min_pass_of` tries that 95% of batches reach, and the tries for about
//  95% to pass once.
struct LeaguePassFigures{
    double expected_pass = 0;
    int min_pass = 0;
    int min_pass_of = 0;
    int reset_budget = 0;
};

//  One fight of the plan: its foes, the top-ups before it and its reserves.
struct LeagueFightData{
    //  "lorelei", "bruno", "agatha", "lance", "champion".
    std::string name = {};
    //  What the plan wants from the fight, for the log.
    std::string goal = {};
    std::vector<LeagueFoePlan> foes = {};
    //  Before this fight: one heal item (between.heal "one"), else the HP
    //  to restore to as a share of the maximum (none: no target); and each
    //  move's PP floor.
    bool between_one_heal = false;
    std::optional<double> between_hp = std::nullopt;
    SlugCounts between_pp = {};
    //  The most of each item a passing try spends, the top-up before it
    //  included.
    SlugCounts caps = {};
    //  A Roar or Whirlwind that lands ends the try.
    bool reset_on_roar = false;
    std::optional<LeagueEarlyReset> early_reset = std::nullopt;
    std::optional<LeaguePassFigures> pass = std::nullopt;
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
