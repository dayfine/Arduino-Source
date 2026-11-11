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

//  Walking duration for a single tile (16 frames at 60 fps).
inline constexpr Milliseconds TILE_TIME(267);
//  Surfing duration for a single tile.
inline constexpr Milliseconds SURF_TILE_TIME(133);
//  8 frames: running is 2 pixels a frame, twice walking. Not measured on the
//  console yet; the walker logs each leg to a wall (speed_note()) for that.
inline constexpr Milliseconds RUN_TILE_TIME(133);
//  turns a player facing another way, without a step.
inline constexpr Milliseconds TURN_TAP(40);
//  duration to wait after turning to allow state to settle.
inline constexpr Milliseconds TURN_SETTLE(300);
//  short enough to turn without stepping.
inline constexpr Milliseconds FACE_HOLD(40);
//  duration of button release between movement steps.
inline constexpr Milliseconds STEP_RELEASE(100);
//  After a warp's fade: leaving by a door the game walks the player a tile out
//  on its own, and a leg started before that ends loses tiles (on the console a
//  600ms wait cost 2 tiles).
inline constexpr Milliseconds AFTER_WARP(1500);

} // namespace OverworldWalkSteps



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
