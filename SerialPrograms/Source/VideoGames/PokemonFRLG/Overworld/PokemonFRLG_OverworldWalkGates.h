/*  Overworld Walk Gates
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console steps of a route through gates (RouteAction::OPEN_GATE and
 *  PRESS_SWITCH), next to the other steps of OverworldWalkSteps.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWalkGates_H
#define PokemonAutomation_PokemonFRLG_OverworldWalkGates_H

#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace OverworldWalkSteps{


//  Facing a card-key gate `direction` (turning first unless `facing_gate`),
//  presses A: with the Card Key the game says so and the door opens. B
//  clears the message, then the walk waits for the door to settle.
void open_gate(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_gate);

//  Facing a statue `direction` (turning first unless `facing_statue`),
//  presses A, takes Yes at the prompt, and clears the text with B.
void press_switch(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_statue);



} // namespace OverworldWalkSteps
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
