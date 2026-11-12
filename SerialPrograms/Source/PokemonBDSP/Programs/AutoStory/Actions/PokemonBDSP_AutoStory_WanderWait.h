/*  BDSP AutoStory - Wait beside a wanderer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A talk to someone who wanders, by waiting for him rather than chasing
 *  him: stand on an end column of his box, inside it, face in, and ask
 *  each time he may have come level, then try the next tile. Our tile is
 *  one he can no longer step on, so he comes level more often. A box wider
 *  than two columns has a far column he can stay on out of reach: the wait
 *  then takes turns between the two ends. The Mart clown (00.05b, 00.05c)
 *  wanders columns 54 to 56: he is waited for on 56 facing west, then on
 *  54 facing east.
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


//  One end of his box to wait at: tiles on one column, in order, a tile
//  apart (the list is walked again from its start: end it a tile from its
//  first), and the way to face from them, into the box.
struct WanderWaitSide{
    std::vector<Pokemon::TilePos> stands;
    Pokemon::Direction facing = Pokemon::Direction::WEST;
};

//  Where and how long to wait. Between columns the wait walks round the
//  stands' rows: along the row above them or the one below, whichever is
//  shorter. Those rows must be open.
struct WanderWaitPlan{
    //  The ends to wait at, in turn; the list is walked `rounds` times.
    std::vector<WanderWaitSide> sides;
    //  Where the wait ends after the talk. A step from it toward each of
    //  `exit_walls` must not move: the walls there prove the tile.
    Pokemon::TilePos exit;
    std::vector<Pokemon::Direction> exit_walls;
    //  Asks from each stand before the next one: few, so the wait steps on
    //  to his row instead of pressing A at nobody (lab-B 2026-11-12 11:03:
    //  twelve asks from (56, 32) while he stood on (55, 33)).
    size_t asks = 2;
    //  Times a side's stands are walked before the next side.
    size_t passes = 3;
    //  The pause after an ask that found nobody.
    Milliseconds between_asks = Milliseconds(500);
    //  The wait for someone in the column to step out of it: a wanderer
    //  tries a step every 2 s or so, and may stay.
    Milliseconds blocked_wait = Milliseconds(2000);
    //  Blocked steps on the way before giving up, and on the way out
    //  before walking round him.
    size_t blocked_tries = 12;
    size_t exit_tries = 3;
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
    STUCK,      //  a step on the way stayed blocked
    LOST,       //  an exit wall was not there: not on the exit
};
//  How a wait ended, and where.
struct WanderWaitResult{
    WanderWaitEnd end;
    Pokemon::TilePos at;        //  where the player is, as counted
    Pokemon::TilePos talked;    //  where the talk counted, for TALKED
};

//  Waits for the wanderer from `at`, at the plan's sides in turn. A step
//  in his box that something stops is him in the column: the wait asks
//  that way too, then waits and steps again.
WanderWaitResult wait_beside_wanderer(WanderWaitBody& body, Pokemon::TilePos at, const WanderWaitPlan& plan);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
