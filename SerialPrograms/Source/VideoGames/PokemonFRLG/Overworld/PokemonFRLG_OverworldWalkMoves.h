/*  Overworld Walk Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The presses a leg is walked with: the D-pad held walking or running,
 *  for how long, and the pace line the walker logs.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWalkMoves_H
#define PokemonAutomation_PokemonFRLG_OverworldWalkMoves_H

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_LegWalker.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalkWatches.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace OverworldWalkSteps{

//  Maps Direction to corresponding DpadPosition.
DpadPosition dpad(Direction direction);

//  The leg mover: holds the D-pad. FRLG moves exactly a tile per 16 frames.
void hold_dpad(ProControllerContext& context, Direction direction, Milliseconds duration);

//  The leg mover for running: holds the D-pad with B for the whole hold.
//  Turn taps go without B: a tap only turns the player.
void hold_dpad_running(ProControllerContext& context, Direction direction, Milliseconds duration);

//  How long one tile of `step` takes at `speed`: on the water, the Surf
//  pace whatever the speed.
Milliseconds tile_time(const RouteStep& step, MoveSpeed speed);

//  The mover for `step` at `speed`: hold_dpad_running() for a run on foot,
//  hold_dpad() otherwise (B does nothing on the water).
Pokemon::LegMover<ProControllerContext> leg_mover(const RouteStep& step, MoveSpeed speed);

//  How long the walker holds the D-pad for a WALK `step` at `speed`
//  (Pokemon::leg_hold()).
Milliseconds leg_hold_time(const RouteStep& step, MoveSpeed speed);

//  For a leg to a wall on foot, the line the walker logs so a console run
//  can check the pace: "[Speed] run west: 40 tiles in 5320 ms, held 5586 ms",
//  the tiles at the assumed pace and the hold. Empty for other steps.
std::string speed_note(const RouteStep& step, MoveSpeed speed);

} // namespace OverworldWalkSteps



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
