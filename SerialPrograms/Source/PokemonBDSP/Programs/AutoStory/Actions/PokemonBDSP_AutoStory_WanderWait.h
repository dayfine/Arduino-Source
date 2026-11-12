/*  BDSP AutoStory - Wait beside a wanderer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A talk to someone who wanders, by waiting for him rather than chasing
 *  him: stand on a tile beside his box, face it, and ask each time he may
 *  have come level, then try the next tile. The Mart clown (00.05b,
 *  00.05c) is waited for on column 56, east of his box.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WanderWait_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WanderWait_H

#include <cstddef>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Where and how long to wait. The stands and the exit are on one column.
struct WanderWaitPlan{
    //  The tiles to wait on, in order; the list is walked `rounds` times.
    std::vector<Pokemon::TilePos> stands;
    //  The way to face from a stand: toward his box.
    Pokemon::Direction facing = Pokemon::Direction::WEST;
    //  Where the wait ends after the talk. A step from it toward each of
    //  `exit_walls` must not move: the walls there prove the tile.
    Pokemon::TilePos exit;
    std::vector<Pokemon::Direction> exit_walls;
    //  Asks from each stand before the next one.
    size_t asks = 12;
    //  The pause after an ask that found nobody.
    Milliseconds between_asks = Milliseconds(500);
    //  The wait for someone in the column to step out of it: a wanderer
    //  tries a step every 2 s or so, and may stay.
    Milliseconds blocked_wait = Milliseconds(2000);
    size_t rounds = 2;
};

//  What the wait does in the game: the console's D-pad and A, or the
//  simulation's.
class WanderWaitBody{
public:
    virtual ~WanderWaitBody() = default;
    //  One whole tile toward `direction`. False if something stopped it.
    virtual bool step(Pokemon::Direction direction) = 0;
    virtual void face(Pokemon::Direction direction) = 0;
    //  A toward the faced tile. True if the talk counted: his question.
    virtual bool ask() = 0;
    virtual void wait(Milliseconds time) = 0;
};

//  How a wait ended.
enum class WanderWaitEnd{
    TALKED,     //  the talk counted; the player is on the plan's exit
    NO_TALK,    //  every ask of every round found nobody to talk to
    STUCK,      //  a step along the column stayed blocked
    LOST,       //  an exit wall was not there: off the column
};
//  How a wait ended, and where.
struct WanderWaitResult{
    WanderWaitEnd end;
    Pokemon::TilePos at;        //  where the player is, as counted
    Pokemon::TilePos talked;    //  where the talk counted, for TALKED
};

//  Waits beside the wanderer from `at`, on the plan's column. A step along
//  the column that something stops is someone in the column: the wait asks
//  that way too, then waits and steps again.
WanderWaitResult wait_beside_wanderer(WanderWaitBody& body, Pokemon::TilePos at, const WanderWaitPlan& plan);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
