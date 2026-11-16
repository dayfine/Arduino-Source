/*  AutoStory League Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League's own steps (one checkpoint from Lorelei's door to the Hall
 *  of Fame, no save between; PokemonFRLG_AutoStoryLeaguePlan.h plans it):
 *
 *  league_start: reads the Items pocket, stops the program (UserSetupError)
 *  when the bag holds less than the League's minimum, naming each short
 *  item and the count needed, and logs and keeps the plan; the fights'
 *  battles spend its budgets (Supplies::budget).
 *
 *  league_between_fights (next WORD, the next fight's trainer; max_hp
 *  INTEGER, the lead's maximum HP when its summary wasn't read): uses
 *  between_fight_item()'s items one at a time from the lead's state. A
 *  Full Restore, Elixir or Max Elixir counts against the next fight's
 *  budget. HP unknown: a Full Restore first, never a fight on an unknown
 *  HP (Bruno, console B, 2026-11-16: entered at 71%).
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

//  The lead between two fights, as between_fight_item() reads it.
struct BetweenFightView{
    int hp = 0;
    int max_hp = 0;
    //  The item that cures the lead's status ("parlyz-heal"; "full-heal"
    //  for one with no own cure), "" for none.
    std::string cure;
    int surf_pp = 0;
    int surf_max_pp = 0;
    int blizzard_pp = 0;
    int blizzard_max_pp = 0;
};

//  The next item to use on the lead before `next`, none when the lead is
//  ready or the bag has nothing that helps:
//    1. a status: its own cure, else a Full Heal, else a Full Restore;
//    2. HP below the fight's entry target: a Super Potion, a Full Restore
//       only when none is left;
//    3. Surf or Blizzard below the fight's PP floor: an Elixir when it
//       reaches the floor, else a Max Elixir, else the Elixir anyway.
std::optional<std::string> between_fight_item(
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
