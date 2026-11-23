/*  AutoStory Viridian Mart
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Viridian City's Mart (02.02's shop step): the orders from the step's
 *  items, bought at the counter, added to the supplies and checked against
 *  the Bag.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryViridianMart_H
#define PokemonAutomation_PokemonFRLG_AutoStoryViridianMart_H

#include <optional>
#include <vector>
#include "Pokemon/AutoStory/Grid/Pokemon_GridStoryArgs.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::optional<std::vector<MartOrder>> viridian_mart_orders(const std::vector<Pokemon::StoryItem>& items);

std::optional<Pokemon::StoryFailure> viridian_shop(StoryEnv& env, const StoryStep& step);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
