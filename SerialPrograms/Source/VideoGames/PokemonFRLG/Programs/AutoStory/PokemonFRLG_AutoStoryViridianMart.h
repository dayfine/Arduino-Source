/*  AutoStory Viridian Mart
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The SHOP step: Viridian Mart before the Boulder Badge, whose rows the
 *  story knows, and the bag's counts read back after it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryViridianMart_H
#define PokemonAutomation_PokemonFRLG_AutoStoryViridianMart_H

#include <optional>
#include <vector>
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  `items` as orders at the Viridian Mart before the Boulder Badge: the
//  rows of POKE BALL, POTION, ANTIDOTE, PARLYZ HEAL. Nothing if it does
//  not sell one of them.
std::optional<std::vector<MartOrder>> viridian_mart_orders(const std::vector<Pokemon::StoryItem>& items);

//  A SHOP step: its "items" bought at the Viridian Mart, counted into
//  `env.supplies`, and with Thorough checks the counts read from the bag.
Pokemon::Outcome viridian_shop(StoryEnv& env, const StoryStep& step);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
