/*  AutoStory Healing Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's steps for full HP into a battle (PokemonFRLG_AutoStoryHealing.h)
 *  on the console: heal to full with the bag's Potions and Super Potions,
 *  check the HP is full, and a Mart step's buy cut to what the bag lacks.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryHealingSteps_H
#define PokemonAutomation_PokemonFRLG_AutoStoryHealingSteps_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step that heals the lead to full with the bag (heal_to_full()):
//  max_hp INTEGER, the lead's maximum HP when the summary wasn't read. HP
//  still missing when the bag runs out is logged; the check fails on it.
inline const std::string HEAL_TO_FULL_ACTION = "heal_to_full";
//  The GAME step that fails unless the lead's HP is known full
//  (missing_hp() measured and 0 or less): max_hp INTEGER, as above.
inline const std::string CHECK_FULL_HP_ACTION = "check_full_hp";
//  The GAME step that sets its own name as a run flag when the bag, as the
//  last count_items read it, holds what its `items` list (bag_holds()): items
//  ITEMS, with a quantity each; any BOOLEAN (default false), at least one of
//  them rather than all. Later steps name the flag in skip_if.
inline const std::string BAG_CHECK_ACTION = "bag_check";

//  From the overworld: `item` from the bag on the lead, if one is left:
//  its count spent, and its HP (healing_hp()) or its cure taken into
//  `supplies`. Returns whether it was used. Defined with the bag's other
//  overworld use, in PokemonFRLG_AutoStoryBattle.cpp.
bool use_recovery_item(
    ConsoleHandle& console, ProControllerContext& context,
    const std::string& item, Pokemon::StoryCheckLevel checks, Language language, Supplies& supplies
);

//  HEAL_TO_FULL_ACTION's, CHECK_FULL_HP_ACTION's and BAG_CHECK_ACTION's specs.
std::vector<Pokemon::StoryActionSpec> healing_action_specs();

//  On the console, a HEAL_TO_FULL_ACTION, CHECK_FULL_HP_ACTION or
//  BAG_CHECK_ACTION step (its flag set in `state`); nothing for another.
std::optional<Pokemon::Outcome> run_healing_step(StoryEnv& env, Pokemon::RunState& state, const Pokemon::StoryStep& step);

//  A MART_ACTION step with "cover_hp" or "fill_to": its first line cut to the
//  hp_cover_shortfall() or fill_shortfall() of `supplies` (at most the plan's quantity), in
//  `step`. Nothing to buy: the step's Outcome, unchanged, logged. Nothing
//  for another step, left as it was.
std::optional<Pokemon::Outcome> cover_hp_in_mart(Logger& logger, const Supplies& supplies, Pokemon::StoryStep& step);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
