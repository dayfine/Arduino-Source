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
    Milliseconds across{};  //  east or west
    Milliseconds up_down{}; //  north or south

    constexpr Milliseconds along(Direction direction) const{
        return direction == Direction::EAST || direction == Direction::WEST ? across : up_down;
    }
};

//  A tap that takes one step: the press, and the release before the next
//  press. Their sum is the time per tapped tile.
struct StepTap{
    Milliseconds hold{};
    Milliseconds release{};
};

//  One way of moving: walking, running or riding.
struct Gait{
    TileTime tile;
    //  The step tap in this gait; a game's taps differ by gait (FRLG's
    //  Bicycle taps far faster than its walk).
    StepTap tap;
    OpenLegPress open_legs = OpenLegPress::STEP_TAPS;
};

//  How one game moves the player: its turn tap, and a tile time and step
//  tap per gait.
struct MoveMechanics{
    //  A tap that turns a player facing another way, without a step.
    Milliseconds turn_tap{};
    //  From pressing, standing still, to the first step under way.
    Milliseconds start_delay{};
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
    .start_delay = Milliseconds(250),
    .walk = {
        .tile = {.across = Milliseconds(160), .up_down = Milliseconds(240)},
        .tap = {.hold = Milliseconds(160), .release = Milliseconds(640)},
        .open_legs = OpenLegPress::STEP_TAPS,
    },
    .run = {
        .tile = {.across = Milliseconds(100), .up_down = Milliseconds(150)},
        .tap = {.hold = Milliseconds(160), .release = Milliseconds(640)},
        .open_legs = OpenLegPress::STEP_TAPS,
    },
    .bike = std::nullopt,
};

//  FireRed and LeafGreen, measured on console B: a walking tile is 16
//  frames, running 8, and a held leg stops where its time says, so open
//  legs on foot are timed holds; on the Bicycle they are counted in taps,
//  40 ms at one per 100 ms (BIKE_TAP_PERIOD_WORKED; at a 60 ms period one
//  tap in 12 drops). The walker's holds count no start delay.
//  Measurements: PokemonFRLG_MovementMechanics.h and
//  PokemonFRLG_BikeSpinMechanics.h.
inline constexpr MoveMechanics FRLG_MOVE_MECHANICS{
    .turn_tap = Milliseconds(40),
    .start_delay = Milliseconds(0),
    .walk = {
        .tile = {.across = Milliseconds(267), .up_down = Milliseconds(267)},
        .tap = {.hold = Milliseconds(66), .release = Milliseconds(700)},
        .open_legs = OpenLegPress::TIMED_HOLD,
    },
    .run = {
        .tile = {.across = Milliseconds(133), .up_down = Milliseconds(133)},
        .tap = {.hold = Milliseconds(66), .release = Milliseconds(700)},
        .open_legs = OpenLegPress::TIMED_HOLD,
    },
    .bike = Gait{
        .tile = {.across = Milliseconds(100), .up_down = Milliseconds(100)},
        .tap = {.hold = Milliseconds(40), .release = Milliseconds(60)},
        .open_legs = OpenLegPress::STEP_TAPS,
    },
};


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
