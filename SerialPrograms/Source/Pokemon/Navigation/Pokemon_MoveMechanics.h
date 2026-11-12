/*  Move Mechanics
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How each game moves the player for a press, as measured on the console:
 *  one entry per game, read by that game's walker and by its sim.
 *
 */

#ifndef PokemonAutomation_Pokemon_MoveMechanics_H
#define PokemonAutomation_Pokemon_MoveMechanics_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How the walker presses an open leg, one that ends short of a wall.
enum class OpenLegPress{
    STEP_TAPS,      //  a step tap a tile: the count is exact
    TIMED_HOLD,     //  one hold, timed by the tile time
};

//  Time per tile along each axis of the screen.
struct TileTime{
    Milliseconds across;    //  east or west
    Milliseconds up_down;   //  north or south

    constexpr Milliseconds along(Direction direction) const{
        return direction == Direction::EAST || direction == Direction::WEST ? across : up_down;
    }
};

//  One way of moving: walking, running or riding.
struct Gait{
    TileTime tile;
    OpenLegPress open_legs;
};

struct MoveMechanics{
    //  A tap that turns a player facing another way, without a step.
    Milliseconds turn_tap;
    //  A tap that takes one step, and the release before the next press.
    Milliseconds step_tap;
    Milliseconds step_tap_release;
    //  From pressing, standing still, to the first step under way.
    Milliseconds start_delay;
    Gait walk;
    //  B held, once the player has the Running Shoes.
    Gait run;
    //  None where the walker does not ride yet.
    std::optional<Gait> bike;
};


//  Brilliant Diamond and Shining Pearl, measured on console B. The camera
//  tilts, so a row takes longer than a column. D-pad taps move exactly a
//  tile each, while a held run overshoots about 3 tiles in 10 (2026-11-09,
//  Jubilife), so open legs are tapped. Taps: PokemonBDSP_AutoStory_RouteInput.h;
//  tile times: PokemonBDSP_AutoStory_RouteTiming.h.
inline constexpr MoveMechanics BDSP_MOVE_MECHANICS{
    .turn_tap = Milliseconds(50),
    .step_tap = Milliseconds(160),
    .step_tap_release = Milliseconds(640),
    .start_delay = Milliseconds(250),
    .walk = {.tile = {.across = Milliseconds(160), .up_down = Milliseconds(240)}, .open_legs = OpenLegPress::STEP_TAPS},
    .run = {.tile = {.across = Milliseconds(100), .up_down = Milliseconds(150)}, .open_legs = OpenLegPress::STEP_TAPS},
    .bike = std::nullopt,
};

//  FireRed and LeafGreen, measured on console B: a walking tile is 16
//  frames, running 8, and a held leg stops where its time says, so open
//  legs on foot are timed holds; on the Bicycle they are counted in taps.
//  The walker's holds count no start delay. Measurements:
//  PokemonFRLG_MovementMechanics.h and PokemonFRLG_BikeSpinMechanics.h.
inline constexpr MoveMechanics FRLG_MOVE_MECHANICS{
    .turn_tap = Milliseconds(40),
    .step_tap = Milliseconds(66),
    .step_tap_release = Milliseconds(700),
    .start_delay = Milliseconds(0),
    .walk = {.tile = {.across = Milliseconds(267), .up_down = Milliseconds(267)}, .open_legs = OpenLegPress::TIMED_HOLD},
    .run = {.tile = {.across = Milliseconds(133), .up_down = Milliseconds(133)}, .open_legs = OpenLegPress::TIMED_HOLD},
    .bike = Gait{.tile = {.across = Milliseconds(100), .up_down = Milliseconds(100)}, .open_legs = OpenLegPress::STEP_TAPS},
};


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
