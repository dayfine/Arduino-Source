/*  AutoStory League Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Elite Four and the Champion are one checkpoint with no save between
 *  the fights (user, 2026-11-16), so the bag read at the League's start is
 *  all five fights get. This plans it out: a pure function from the bag's
 *  counts to an item budget per fight, and the rules between the fights.
 *
 *  The planner (plan_league()):
 *    - LEAGUE_MINIMUM is the bag the League is restocked to;
 *      league_shortfalls() names what is short, for the log. The League
 *      is planned from whatever the bag holds (user, 2026-11-16).
 *    - Full Restores are spread over the five fights by the median each
 *      fight spends in PokemonSimulation's budgeted-C2 League chains
 *      (sim/frlg-e4-replica, ~/BDSPLab/notes/frlg-e4-replica-policy.md):
 *      Lance and the Champion take the most. The Champion keeps a reserve
 *      of at least CHAMPION_FULL_RESTORES (half the bag at most), taken
 *      from the earliest fights.
 *    - X Special, X Speed, X Accuracy, Guard Spec, the Elixirs, Hyper (and
 *      Max) Potions and Full Heals go by
 *      each fight's wanted count (the note's budgeted-C2 table); a bag
 *      short of the sum gives the Champion its count first and splits the
 *      rest in proportion. A bag with more than the sum keeps the extra
 *      as spare, which the KO-risk rules may still use.
 *    - Every split is largest-remainder, ties to the later fight.
 *    - What a fight leaves of its budget rolls forward to the next.
 *
 *  Between the fights: between_fight_action() (PokemonFRLG_AutoStoryLeagueSteps.h)
 *  from each fight's entry HP and PP floors.
 *
 *  The in-battle rules that spend the budget are in
 *  PokemonFRLG_AutoStoryBattlePolicy.h (BUDGETED_SETUP).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeaguePlan_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeaguePlan_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryLeagueBudget.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  One League fight in the plan.
struct LeagueFightPlan{
    //  The battle step's trainer ("TRAINER_ELITE_FOUR_LORELEI").
    std::string trainer;
    //  "lorelei".
    std::string name;
    FightBudget budget;
    //  The HP the lead enters with, in percent of its maximum.
    int entry_hp_percent = 100;
    //  The PP the lead enters with, at least.
    int surf_pp_floor = 0;
    int blizzard_pp_floor = 0;
};

//  The League's plan: the fights in order, and what the bag holds beyond
//  their budgets.
struct LeaguePlan{
    std::vector<LeagueFightPlan> fights;
    FightBudget spare;
};

//  The items `bag` holds less of than LEAGUE_MINIMUM, as "full-restore 7
//  of 8, max-elixir 0 of 1"; none when it holds enough.
std::optional<std::string> league_shortfalls(const BagCounts& bag);

//  The budgets and between-fight rows for the five fights from `bag`.
LeaguePlan plan_league(const BagCounts& bag);

//  The plan, one line per fight, for the log.
std::vector<std::string> league_plan_lines(const LeaguePlan& plan);

//  The fight in `plan` whose trainer is `trainer`, none if it isn't one.
const LeagueFightPlan* league_fight(const LeaguePlan& plan, std::string_view trainer);

//  A League run: the plan, and what the fights before have left of theirs.
struct LeagueRun{
    LeaguePlan plan;
    FightBudget carried;
};

//  Registers the League plan tests.
void add_tests_AutoStoryLeaguePlan(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
