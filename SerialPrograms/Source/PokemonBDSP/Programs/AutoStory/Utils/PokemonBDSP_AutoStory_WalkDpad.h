/*  BDSP AutoStory - Walk on the D-pad
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Straight walks on the D-pad, which moves whole tiles and keeps to the row.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WalkDpad_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WalkDpad_H

#include <cstddef>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Controllers/Joystick.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  How walk_until_dpad() presses: RUN holds B too, twice as fast and as
//  tile-true.
enum class DpadPace{
    WALK,
    RUN,
};
//  Configuration options controlling D-pad based walking maneuvers.
struct DpadWalkOptions{
    WalkArrived arrived = nullptr;
    DpadPace pace = DpadPace::WALK;
};

//  walk_until() on the D-pad, for a straight walk along a row or a column
//  (`stick` is one of the four directions). The D-pad moves whole tiles and
//  keeps to the row: the stick drifts off it, catches tree corners and slides
//  a row on stairs (Route 203, r226a and r227b; by hand the D-pad went
//  through the same places).
WalkResult walk_until_dpad(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label,
    const DpadWalkOptions& options = DpadWalkOptions()
);

//  A set number of whole tiles on the D-pad, a tap a tile (a tap moves
//  exactly one tile; a stick hold timed for n tiles is a tile or two off),
//  with the same watch for wild battles and dialogue as walk_until(). Ends
//  LIMIT after `tiles` taps, or BLOCKED when two taps in a row move nothing.
WalkResult walk_tiles_dpad(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, size_t tiles, const std::string& label
);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
