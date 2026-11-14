/*  Overworld Walk Times
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How long a tile, a turn and a press take on the overworld.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWalkTimes_H
#define PokemonAutomation_PokemonFRLG_OverworldWalkTimes_H

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_LegWalker.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalkOptions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace OverworldWalkSteps{

//  The pace, tap and release times are the game's movement mechanics
//  (PokemonFRLG_MovementMechanics.h).

//  After a warp's fade: leaving by a door the game walks the player a tile out
//  on its own, and a leg started before that ends loses tiles (on the console a
//  600ms wait cost 2 tiles).
inline constexpr Milliseconds AFTER_WARP(1500);

} // namespace OverworldWalkSteps



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
