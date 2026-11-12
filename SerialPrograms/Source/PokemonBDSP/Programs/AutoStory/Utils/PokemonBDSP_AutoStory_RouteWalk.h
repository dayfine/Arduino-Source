/*  BDSP AutoStory - Route Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What walking on the story maps is made of: walking times per tile, the
 *  stick for a direction, turning, talking to someone next to us, finishing
 *  a dialogue, and going through doors. PlannedRoute walks whole plans.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteWalk_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteWalk_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP_AutoStory_RouteDoor.h"
#include "PokemonBDSP_AutoStory_RouteInput.h"
#include "PokemonBDSP_AutoStory_RouteTalk.h"
#include "PokemonBDSP_AutoStory_RouteTiming.h"
#include "PokemonBDSP_AutoStory_Walk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Whether a walk to the wall that ends on any of `after` can tell it has
//  stopped: not in tall grass, whose sway keeps the screen changing at a
//  wall. Elsewhere, a walk to the wall that stops confirms where it ended.
bool route_wall_readable(const StoryWorld& grid, const std::vector<Pokemon::TilePos>& after);

//  Shows what the route walk is doing in the log and the video overlay.
void route_status(VideoStream& stream, const std::string& text);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
