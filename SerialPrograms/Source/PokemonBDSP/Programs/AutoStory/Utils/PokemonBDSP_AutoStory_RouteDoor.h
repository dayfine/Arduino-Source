/*  BDSP AutoStory - Route door
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Going in and out of doors, and telling a door's fade from the screen.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteDoor_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteDoor_H

#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  For a player on the tile below a door: walks north through it. If a wall
//  stops the walk (the door frame, a tile off the opening), steps a tile east,
//  then west, and tries again. Fails with a screenshot if no step goes in.
void enter_door_north(VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const std::string& label);

//  For a player inside a building whose door is straight south: closes any
//  talk still up and walks south out of it, stopping at the door's fade or
//  the first look that is not a room, so it ends a tile or so outside. Out is
//  then three standing looks in a row that are not a room: one look of a lit
//  Center can miss its black edge (r259). Fails with a screenshot if five
//  walks south get nowhere.
void leave_door_south(VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const std::string& label);

//  Brightness (mean r + g + b) below which a look is the door's fade, and
//  above which a look that is not a room is outside.
inline constexpr double DOOR_FADE_DARK = 120.0;
//  Brightness threshold above which non-room frame is classified as outside.
inline constexpr double DOOR_OUTSIDE_BRIGHT = 200.0;

//  What leave_door_south() has seen of the door's fade so far.
struct DoorFadeWatch{
    bool saw_dark = false;
};
//  Takes one look (its brightness, and whether indoors() read a room) and
//  returns whether we are out: a dark look seen, and now a bright one that
//  is not a room.
bool door_fade_look(DoorFadeWatch& watch, double brightness, bool room);
//  Whether leave_door_south()'s walk stops at this look: at the door's fade
//  out (door_fade_look()), or at the first look that is not a room, so a
//  held stick does not walk on outside (r261 three rows).
bool door_walk_stops(DoorFadeWatch& watch, double brightness, bool room);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
