/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Wild Zone 1's tutorial catches (03.06), from console B, 2026-11-10.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatches_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatches_H

#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  03.06's first catch (Fletchling): ZL locks on, ZL+ZR held 1.5 s throws
//  (0.5 s only aims). A break-out brings two or three lines from Taunie and
//  control back; the catch brings her lines and then the "Battling wild
//  Pokemon" cards (console B, 2026-11-10). So: lock, throw, A through her
//  lines, again until a card shows, then A through the cards. Throws
//  OperationFailedException after `max_throws` throws with no card.
void catch_tutorial(ConsoleHandle& console, ProControllerContext& context, int max_throws);

//  03.06's second catch (Bunnelby), after the cards: D-pad up sends the
//  partner out, then rounds of TrainerBattleState attacks (3 that land),
//  a throw and A through any lines, until no target locks on for a round:
//  caught. Throws OperationFailedException after `max_rounds`.
void battle_then_catch(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int max_rounds
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
