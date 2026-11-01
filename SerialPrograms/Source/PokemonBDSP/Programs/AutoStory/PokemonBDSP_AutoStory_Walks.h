/*  BDSP AutoStory - Walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks measured on the console that the planner does not make: straight
 *  holds and D-pad taps, walks until something stops us, and the ways in
 *  and out of doors. Each fails the step with a screenshot when the walk
 *  ends where it should not (a dialogue or a door on a walk to a wall).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Walks_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Walks_H

#include <functional>
#include <string>
#include <utility>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The stick's (x, y) for `direction`: y is up the screen.
std::pair<double, double> dpad_of(Pokemon::Direction direction);

//  Walks `direction` until something stops us, for at most `tiles` tiles'
//  time, turning first. Holding on for the whole time counts as stopped:
//  the leg was measured to end against a wall. Fails on a transition or a
//  dialogue.
void walk_until_stopped(VideoStream& stream, ProControllerContext& context, Pokemon::Direction direction, size_t tiles, const std::string& label);
//  Holds the stick `direction` for `hold` from standing still (200 ms is a
//  row, turning included; 160 ms a tile across, facing that way already).
//  Fails on a transition or a dialogue.
void hold_for(VideoStream& stream, ProControllerContext& context, Pokemon::Direction direction, Milliseconds hold, const std::string& label);
//  `tiles` one-tile taps `direction` in a row, facing that way already,
//  without waiting for the screen to settle in between: six of these moved
//  exactly six tiles on the console, where six single holds moved seven
//  (2026-10-30).
void tap_tiles(VideoStream& stream, ProControllerContext& context, Pokemon::Direction direction, size_t tiles, const std::string& label);
//  Half a tile `direction` (EAST or WEST), facing it first.
void nudge(VideoStream& stream, ProControllerContext& context, Pokemon::Direction direction, const std::string& label);
//  Walks north; true if a door took us through, by its fade or by a whole
//  new scene (the Trainers' School's fade is not always black, r146b).
bool door_north_by_scene(VideoStream& stream, ProControllerContext& context, const std::string& label);

//  One tile south, for a player facing another way.
void step_south(VideoStream& stream, ProControllerContext& context);
//  Holds the stick toward `dir` for `tiles` tiles, turning first.
void hold_tiles(VideoStream& stream, ProControllerContext& context, Pokemon::Direction dir, size_t tiles);
//  walk_until() toward `dir`.
WalkResult walk_dir(
    VideoStream& stream, ProControllerContext& context, Pokemon::Direction dir, Milliseconds limit, const std::string& label,
    const std::function<bool(const ImageViewRGB32& screen)>& arrived = nullptr
);

//  `taps` D-pad taps toward `dpad`, settling between them. On the D-pad the
//  player moves a whole tile per tap, so counted legs do not drift the way
//  stick holds do. The first tap toward a new way only turns the player, so
//  turn first.
void dpad_tiles(VideoStream& stream, ProControllerContext& context, DpadPosition dpad, size_t taps, const std::string& label);
//  Through a door straight north of us: tap up to face it, hold up into it,
//  and wait for its dark fade (by hand: brightness 7, about 1 s after).
bool dpad_through_door_north(VideoStream& stream, ProControllerContext& context, const std::string& label);
//  Out of a building whose door is straight south of us, a D-pad tile at a
//  time, until the location banner shows: it names the map we step into
//  (e.g. "Oreburgh City") for a few seconds, and never shows inside. Ends in
//  the doorway, facing south. Out of the Oreburgh Gym from in front of Roark
//  (by hand, 2026-10-31): 18 taps inside, 2 taps of black fade, the banner on
//  the 21st. The door's fade alone was missed between steps (r219g walked on
//  south to the rocks below the gym).
bool dpad_out_of_building(VideoStream& stream, ProControllerContext& context, size_t max_taps, const std::string& label);
//  For a player inside a Pokemon Center after a whiteout, at the counter:
//  out through the door. The door is straight south of the nurse in most
//  Centers; in Oreburgh's it was two tiles east of where we stood (by hand).
void walk_out_of_center(VideoStream& stream, ProControllerContext& context, const std::string& label);
//  West until a transition, by the user's rule for a wall in the way: walk
//  north to the top wall, which pins the row, then try the rows south of it
//  one at a time. Trainers who stop us are fought. Returns at the first
//  transition (a building, a cave, a gate), or when `arrived` matches.
void west_until_transition(
    VideoStream& stream, ProControllerContext& context, const std::string& label, size_t max_legs,
    const std::function<bool(const ImageViewRGB32& screen)>& arrived = nullptr
);



}
}
}
#endif
