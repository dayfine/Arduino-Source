/*  AutoStory League Rows
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League's fights as verify rows (Pokemon_StoryVerify.h): each fight
 *  is its own checkpoint, run as the shared engine's verification loop
 *  from its start save. FRLG's part is the pass predicate: the fight the
 *  row's battle names, won (the battle step fails otherwise) spending no
 *  item over the plan's caps (league_try_passes()).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeagueRows_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeagueRows_H

#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "PokemonFRLG_AutoStoryCheckpoints.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Whether a try of `row` that completed passes: for a row whose battle is
//  a League fight, the items spent from `start` (the start save's
//  supplies) to `end` within the fight's caps; any other row passes.
Pokemon::Outcome league_row_pass(const StoryCheckpoint& row, const Supplies& start, const Supplies& end);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
