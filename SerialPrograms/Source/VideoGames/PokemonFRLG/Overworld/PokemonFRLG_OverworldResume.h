/*  Overworld Resume
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  After an open leg stopped part-way: walking on along it instead of finding the player at walls first, and how far each way walks.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldResume_H
#define PokemonAutomation_PokemonFRLG_OverworldResume_H

#include <optional>
#include <vector>
#include "PokemonFRLG_OverworldReanchor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  How many tiles `anchor`'s holds and way back walk, as far as planned.
int reanchor_tiles(const Reanchor& anchor);

//  Walking on along a stopped open leg instead of finding the player at
//  walls first: the rest of the leg from the middle of the tiles they may
//  be on, after which the route's next steps, as planned, end with a hold
//  that brings them to one tile from every one of them. Walking on from
//  how far a leg got, trusting the timing, went wrong on the console; here
//  a hold finds the player whichever tile they were on.
struct Resume{
    //  From the middle candidate to the stopped leg's end; no tiles if the
    //  middle candidate is its end.
    RouteStep rest;
    //  How many of the steps after the stopped leg go by until one, a hold
    //  to a wall, brings every candidate to one tile.
    size_t steps = 0;
    //  Where each of those steps may start, `rest` first.
    std::vector<std::vector<MapPosition>> starts;
};

//  After `step`, an open leg, stopped part-way on one of `candidates`
//  (none stopped by a trainer), with `after` the route's steps after it:
//  the way to walk on. Nothing if, from some candidate, a step of `after`
//  before a hold that brings them all to one tile would walk otherwise
//  than planned, or a way would cross a tile off the planned way that a
//  trainer may see.
std::optional<Resume> plan_resume(
    const std::vector<MapData>& maps,
    const std::vector<StopCandidate>& candidates,
    const RouteStep& step,
    const std::vector<RouteStep>& after
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
