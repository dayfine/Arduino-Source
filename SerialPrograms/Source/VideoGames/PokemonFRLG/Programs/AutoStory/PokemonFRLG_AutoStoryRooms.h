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
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h"

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
//  takes a warp, whose fade is waited out. `dims`: the warp's fade only
//  dims the screen, never black (the lift's way out; console B,
//  2026-11-10, 00.32 lift_to_b4f), so the warp is confirmed by the scene
//  from before the leg's first step gone and settled, and any step of the
//  leg may take it. `to`: the warp's place, for the log.
struct RoomLeg{
    Pokemon::Direction direction = Pokemon::Direction::NORTH;
    int tiles = 0;
    bool warps = false;
    bool dims = false;
    std::string_view to = {};
};

//  On the console: the legs, a step tap a tile (step_once()), each warp's
//  fade waited out. `facing` is the way the player faces at the start, if
//  known.
void walk_room_legs(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RoomLeg>& legs, std::optional<Pokemon::Direction> facing, PlayerGender player
);


//  Registers the scripted rooms' tests.
void add_tests_AutoStoryRooms(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
