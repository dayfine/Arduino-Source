/*  AutoStory RNG Returns
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The way back into the story from where an upstream RNG program leaves
 *  the game: SID Helper's Trainer Card, Gift RNG's summary.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRngReturns_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRngReturns_H

#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  From the Trainer Card SID Helper leaves open, or from HOME (its "go
//  home when done"): back to the game, the card and the START menu closed.
//  Returns on the overworld.
void close_trainer_card(ConsoleHandle& console, ProControllerContext& context);

//  From the gift's summary Gift RNG leaves open, or from HOME (its "go home
//  when done"): back to the game, the summary, the party and the START
//  menu closed. Returns on the overworld.
void leave_gift_summary(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
