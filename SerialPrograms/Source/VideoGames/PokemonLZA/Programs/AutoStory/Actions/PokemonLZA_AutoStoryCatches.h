/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Wild Zone 1's catches (03.06's tutorial, 03.07's five species), from
 *  console B, 2026-11-10.
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

//  03.06's second catch (Bunnelby), after the cards: A past its "weaken
//  it" banner (D-pad up is ignored while it shows), D-pad up sends the
//  partner out (again, twice at most per round, after two lock-ons with no
//  move symbols), then rounds of TrainerBattleState attacks (3 that land),
//  a throw and A through any lines, until no target locks on for a round:
//  caught. Throws OperationFailedException after `max_rounds`.
void battle_then_catch(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int max_rounds
);

//  03.07's catches for the "register five species" quest (3/5 after the
//  tutorial), from where the player stands in Wild Zone 1: throws at what
//  locks on there, then walks 3 s spokes out and back (ahead, then left and
//  right in turn, by the minimap's heading) and throws at what locks on at
//  each end, then faces the start heading again. A throw followed by text
//  or a card counts as a catch; a repeat species counts too, so ask for
//  more than are needed. Throws OperationFailedException with fewer than
//  `catches_wanted` after the spokes or `max_throws`.
void catch_in_zone(ConsoleHandle& console, ProControllerContext& context, int catches_wanted, int max_throws);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
