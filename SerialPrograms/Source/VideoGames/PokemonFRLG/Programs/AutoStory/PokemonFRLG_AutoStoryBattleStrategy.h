/*  AutoStory Battle Strategy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The lead's plan for the hard battles from Lavender to the Champion, as
 *  data (BattleData/strategies.json): how many X item stages to set up
 *  before attacking, per trainer, from the setup goals PokemonSimulation
 *  measured for the Pokeguy Squirtle route (docs/battles/
 *  pokeguy-conditions.md, its "C2" rules). A trainer with no entry gets the
 *  general policy: own_turn() of PokemonFRLG_AutoStoryBattleSim.h, with a
 *  Super Potion, Hyper Potion or Full Restore where a Potion would not
 *  cover the foe's next turn.
 *
 *  The setup turn (C2, simplified to the solo lead):
 *  - No setup while asleep, against a foe with Self-Destruct or Explosion,
 *    against one whose best hit faints the lead from full, against one
 *    whose next turn is more likely than not a status move (sleep, poison,
 *    paralysis: the setup turn hands it the status), or on the last foe
 *    when the lead's attack faints it in one turn.
 *  - Setup only while the lead's HP is above D, the foe's highest roll plus
 *    poison and Leech Seed, and at least R = min(3/4 of its HP,
 *    D + (N - 1) * M), with N the setup turns still planned and M the
 *    foe's mean turn. Below that, the smallest heal whose HP after the
 *    foe's mean turn reaches R, else the one that leaves the most; with no
 *    heal above D, the lead attacks.
 *  - X Attack and X Special first, then X Speed until the lead outspeeds
 *    every foe left, then X Accuracy up to the goal plus the foe's evasion
 *    (no evasion chase without an accuracy goal: Minimize outpaces it).
 *
 *  The battle loop doesn't use these items yet (Supplies doesn't count
 *  them), so the story's own win chance is unchanged: these items count
 *  only in the sim.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleStrategy_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleStrategy_H

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryBattleSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The stat stages the lead sets up before it attacks, one X item a stage.
struct SetupGoals{
    int8_t attack = 0;
    int8_t sp_attack = 0;
    //  Over the active foe's evasion stages; 0 for none at all.
    int8_t accuracy = 0;
    //  Faster than every foe left.
    bool outspeed = false;

    bool any() const{
        return attack > 0 || sp_attack > 0 || accuracy > 0 || outspeed;
    }
};

//  `trainer`'s goals in strategies.json; none set for one not there.
SetupGoals setup_goals(std::string_view trainer);


//  An item the plan uses that own_turn() doesn't.
enum class PlanItem{
    SUPER_POTION,
    HYPER_POTION,
    FULL_RESTORE,
    X_ATTACK,
    X_SPECIAL,
    X_SPEED,
    X_ACCURACY,
};
//  Item slug for logging: "super-potion", "x-special".
std::string_view plan_item_slug(PlanItem item);

//  The HP `item` heals on `own` now; 0 for an X item.
uint16_t plan_item_heal(PlanItem item, const SimBattler& own);

//  What the bag holds of `item` in `items`.
int plan_item_count(const SimItems& items, PlanItem item);


//  The lead's turn under the plan: own_turn()'s, or one of the plan's items.
struct PlannedTurn{
    OwnTurn turn = {};
    std::optional<PlanItem> item = {};
};

//  The next X item `goals` still need with `foe` out and `to_come` left,
//  that `items` holds; none once they are met.
std::optional<PlanItem> next_setup_item(
    const SimBattler& own, const SimItems& items, const SimBattler& foe,
    const std::vector<SimBattler>& to_come, const SetupGoals& goals
);

//  The lead's turn: setup and its heals (see the top), else own_turn(),
//  with a bigger heal than a Potion when own_turn() attacks into a turn
//  that could faint the lead and the bigger heal pays (potion_pays()).
PlannedTurn planned_turn(
    const SimBattler& own, const SimItems& items, const SimBattler& foe,
    const SimTrainer& trainer, size_t foes_left,
    const std::vector<SimBattler>& to_come, const SetupGoals& goals
);


//  The hard battles' win chances with and without the plan.
void add_tests_AutoStoryBattleStrategy(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
