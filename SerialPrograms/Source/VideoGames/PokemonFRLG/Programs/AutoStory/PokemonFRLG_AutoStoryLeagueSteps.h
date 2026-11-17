/*  AutoStory League Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League's own steps (one checkpoint from Lorelei's door to the Hall
 *  of Fame, no save between; PokemonFRLG_AutoStoryLeaguePlan.h plans it):
 *
 *  league_start: reads the Items pocket, logs what the bag lacks of the
 *  League's restock list, and logs and keeps the plan made from the bag
 *  as it is; the fights' battles spend its budgets (Supplies::budget).
 *
 *  league_between_fights (next WORD, the next fight's trainer; max_hp
 *  INTEGER, the lead's maximum HP when its summary wasn't read): does
 *  between_fight_action()'s moves one at a time from the lead's state. A
 *  budgeted item (Full Restore, Hyper or Max Potion, Full Heal, Elixir,
 *  Max Elixir) counts against the next fight's budget. Never a fight on an unknown HP (Bruno, console B, 2026-11-16:
 *  entered at 71%), and never a Full Restore for a read failure.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeagueSteps_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeagueSteps_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryLeaguePlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step at the League's start (league_start above).
inline const std::string LEAGUE_START_ACTION = "league_start";
//  The GAME step between two League fights (league_between_fights above).
inline const std::string LEAGUE_BETWEEN_FIGHTS_ACTION = "league_between_fights";

//  The lead between two fights, as between_fight_action() reads it.
struct BetweenFightView{
    //  None when no bar was measured since the load and no item healed it
    //  to a known HP.
    std::optional<int> hp = std::nullopt;
    int max_hp = 0;
    //  Whether the HP was already read again this step (READ_HP).
    bool hp_reread = false;
    //  The item that cures the lead's status ("parlyz-heal"; "full-heal"
    //  for one with no own cure), "" for none.
    std::string cure;
    int surf_pp = 0;
    int surf_max_pp = 0;
    int blizzard_pp = 0;
    int blizzard_max_pp = 0;
};

//  What league_between_fights does next (between_fight_action()).
enum class BetweenFightMove{
    //  Into the fight.
    READY,
    //  The action's item on the lead.
    USE_ITEM,
    //  Read the lead's HP again.
    READ_HP,
    //  The HP is still unknown after reading it again: stop before the fight.
    HP_UNKNOWN,
};

//  between_fight_action()'s move, with its item.
struct BetweenFightAction{
    BetweenFightMove move = BetweenFightMove::READY;
    //  USE_ITEM's item (a bag slug).
    std::optional<std::string> item = std::nullopt;

    bool operator==(const BetweenFightAction&) const = default;
};

//  The next move before `next`, from the lead's state, in this order:
//    1. a status: its own cure, else a Full Heal, else a Full Restore;
//    2. HP unknown: read it again; still unknown: HP_UNKNOWN (a Full
//       Restore is never spent on a read failure);
//    3. HP below the fight's entry target: the cheapest heal held, a
//       Super Potion, then a Hyper Potion, a Max Potion, a Full Restore
//       last;
//    4. Surf or Blizzard below the fight's PP floor: an Elixir when it
//       reaches the floor, else a Max Elixir, else the Elixir anyway;
//    5. READY (also when the bag has nothing that helps).
BetweenFightAction between_fight_action(
    const BetweenFightView& lead, const BagCounts& bag, const LeagueFightPlan& next
);

//  LEAGUE_START_ACTION's and LEAGUE_BETWEEN_FIGHTS_ACTION's specs.
std::vector<Pokemon::StoryActionSpec> league_action_specs();

//  On the console, a LEAGUE_START_ACTION or LEAGUE_BETWEEN_FIGHTS_ACTION
//  step; nothing for another.
std::optional<Pokemon::Outcome> run_league_step(StoryEnv& env, const Pokemon::StoryStep& step);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
