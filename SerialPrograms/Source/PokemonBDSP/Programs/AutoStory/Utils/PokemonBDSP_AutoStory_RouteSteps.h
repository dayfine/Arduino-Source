/*  BDSP AutoStory - Route steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the planned route and the planned talk both do through a RouteIO.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteSteps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteSteps_H

#include <string>
#include "PokemonBDSP_AutoStory_RouteInput.h"
#include "PokemonBDSP_AutoStory_RouteIO.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Fails the program with `message` (RouteNote::FAILURE).
[[noreturn]] void fail(RouteIO& io, const std::string& message);

//  route_turn() through `io`.
void turn(RouteIO& io, Pokemon::Direction direction);

//  A press of A, to talk to someone in front of us.
RouteInput press_a(RouteInputEnd end);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
