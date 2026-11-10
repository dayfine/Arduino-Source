/*  AutoStory Scripted Rooms
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rooms the story walks by a fixed script of taps, not a planned route:
 *  every Pokemon Center's ground floor (one layout, so one script) and the
 *  Rocket Hideout's lift. Their tiles are coded (PokemonFRLG_CodedRooms.h),
 *  not mapped: the user's call, 2026-11-09 ("mostly the same"; "small area,
 *  high overhead, just script it").
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRooms_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRooms_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step into a Pokemon Center: from the tile below its door,
//  facing north, through the door onto the mat (CENTER_MAT) and up the
//  nurse's column to below her counter (CENTER_NURSE_FRONT), facing her.
//  The HEAL step after it talks to her.
inline const std::string CENTER_ENTER_VARIANT = "center_enter";
//  The GAME step out of a Pokemon Center: from below the nurse's counter
//  down to the mat and out; the game walks the player a tile out of the
//  door, onto the tile below it, facing south.
inline const std::string CENTER_EXIT_VARIANT = "center_exit";
//  Whether `variant` is one of the Center's steps.
bool is_center_variant(std::string_view variant);

//  A straight piece of a room's script: `tiles` steps `direction`, one
//  tap a tile; none: a turn to face `direction`. `warps`: the last step
//  takes a warp, whose fade is waited out.
struct RoomLeg{
    Pokemon::Direction direction = Pokemon::Direction::NORTH;
    int tiles = 0;
    bool warps = false;
};
//  Into a Center: north through the door, north 4 to the nurse.
const std::vector<RoomLeg>& center_enter_legs();
//  Out of a Center: south 4 to the mat, south through the door.
const std::vector<RoomLeg>& center_exit_legs();
//  Into the hideout's lift, from below a floor's lift door facing north:
//  north through the door onto LIFT_ARRIVAL, north 2, west 2 to below the
//  panel (LIFT_PANEL_FRONT), facing it.
const std::vector<RoomLeg>& lift_in_legs();
//  Out of the lift, from below the panel: east 2, south 2 onto the arrow
//  warp, south through it, onto the floor picked's lift door.
const std::vector<RoomLeg>& lift_out_legs();
//  The legs of the Center's step `variant`; empty if it isn't one.
const std::vector<RoomLeg>& center_legs(std::string_view variant);

//  On the console: the legs, a step tap a tile (step_once()), each warp's
//  fade waited out. `facing` is the way the player faces at the start, if
//  known.
void walk_room_legs(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RoomLeg>& legs, std::optional<Pokemon::Direction> facing
);


//  Registers the scripted rooms' tests.
void add_tests_AutoStoryRooms(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
