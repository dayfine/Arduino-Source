/*  BDSP AutoStory - Route input
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Short presses a route walk is made of, and turning on the spot.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteInput_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteInput_H

#include <cstddef>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A D-pad tap this short only turns, whichever way the player faced: on the
//  console 40, 50, 60 and 80 ms taps each turned without a step. A 100 ms
//  stick tap stepped about 0.6 tile, off the tile grid, and a 160 ms D-pad
//  tap steps a whole tile (r49: a stick "turn" put us a tile west of
//  Sandgem's Center door; r248a).
constexpr Milliseconds ROUTE_TURN_TAP = Milliseconds(50);

//  What a RouteInput presses.
enum class RouteInputKind{
    DPAD,       //  the D-pad toward `direction`: whole tiles
    STICK,      //  the left stick toward `direction`: any distance
    BUTTON_A,
    WAIT,       //  nothing, for `hold`, after what is already queued
    PAUSE,      //  nothing, for `hold` from now (ControllerContext::wait_for())
};
//  Whether press_route_input() returns once the presses are done, or at once.
enum class RouteInputEnd{
    DONE,
    QUEUED,
};
//  A short press, `count` times: held for `hold`, then let go for `release`.
//  `direction` is for DPAD and STICK only.
struct RouteInput{
    RouteInputKind kind;
    Pokemon::Direction direction;
    Milliseconds hold;
    Milliseconds release;
    size_t count;
    RouteInputEnd end;
};
//  Executes the specified route input sequence on the controller context.
void press_route_input(ProControllerContext& context, const RouteInput& input);

//  The tap route_turn() turns with: on the D-pad, ROUTE_TURN_TAP long.
RouteInput route_turn_tap(Pokemon::Direction direction);

//  Turns the player to face `direction` without moving. Only for a player who
//  faces another way: one already facing it may take a step.
void route_turn(VideoStream& stream, ProControllerContext& context, Pokemon::Direction direction);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
