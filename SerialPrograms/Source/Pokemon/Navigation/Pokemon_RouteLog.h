/*  Route Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The log lines a route walk writes, shared by the games' walkers so that
 *  every game's log reads the same and devtools/maps can draw any run's
 *  routes from it. These are BDSP AutoStory's route lines, lifted from
 *  PokemonBDSP_AutoStory_RouteWalk.cpp, with what the other games need:
 *
 *      [Route] To the grass: plan from Route 202 (15, 25), Route 202 (16, 25):
 *          east 15 (to wall) to Route 202 (23, 25), Route 202 (30, 26)
 *          north 5 (to wall) to Route 202 (23, 24)
 *      [Route] Leg 1/2: east 15 (to wall) to Route 202 (23, 25), Route 202 (30, 26)
 *      [Route] To the grass: east 15 (to wall): ended at Route 202 (23, 25) (walked).
 *      [Route] To the grass: north 5 (to wall): stopped early after about 2 tiles, at Route 202 (23, 23); planning again.
 *      [Route] To the grass: found again at Route 202 (22, 24), after 2 holds.
 *      [Route] To the grass: arrived at Route 202 (23, 24).
 *
 *  A position is a set of tiles: BDSP plans from every tile the player may
 *  be on, FRLG from the one it is on. A leg's end says how it is known:
 *  walked (as planned, or seen), assumed (taken to have ended as planned
 *  after something interrupted it), or unknown.
 *
 *  The functions return a line without the "[Route] " tag, which
 *  ROUTE_LOG_TAG holds; the caller logs it.
 *
 */

#ifndef PokemonAutomation_Pokemon_RouteLog_H
#define PokemonAutomation_Pokemon_RouteLog_H

#include <cstddef>
#include <string>
#include <vector>
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


inline constexpr const char* ROUTE_LOG_TAG = "[Route] ";

//  A tile of a named map.
struct RouteTile{
    std::string map;
    TilePos pos;
};
//  "Route 202 (23, 25)".
std::string to_string(const RouteTile& tile);
//  The tiles, comma-separated; "an unknown tile" for none.
std::string describe(const std::vector<RouteTile>& tiles);


enum class RouteLegAction{
    WALK,
    SURF,
    FACE,
};

//  How a leg ends: at a wall, on a warp, or where it is timed to.
enum class RouteLegEnd{
    OPEN,
    WALL,
    WARP,
};

struct RouteLogLeg{
    RouteLegAction action = RouteLegAction::WALK;
    Direction direction = Direction::NORTH;
    size_t tiles = 0;
    RouteLegEnd end = RouteLegEnd::OPEN;
};
//  "east 15 (to wall)", "south 4 (open)", "north 1 (warp)", "surf east",
//  "face west".
std::string to_string(const RouteLogLeg& leg);

//  A planned leg and the tiles it may end on.
struct PlannedRouteLeg{
    RouteLogLeg leg;
    std::vector<RouteTile> after;
};

//  How the end of a walked leg is known.
enum class EndBasis{
    WALKED,     //  walked as planned, or seen
    ASSUMED,    //  taken to have ended as planned after an interrupt
    UNKNOWN,    //  stopped somewhere on the way
};
const char* basis_name(EndBasis basis);


//  "<label>: plan from <tiles>:", then a line per leg.
std::string route_plan_line(const std::string& label, const std::vector<RouteTile>& from, const std::vector<PlannedRouteLeg>& legs);
//  "Leg <number>/<count>: <leg> to <tiles>", as the leg starts.
std::string route_leg_line(size_t number, size_t count, const PlannedRouteLeg& leg);
//  "<label>: <leg>: ended at <tiles> (<basis>).", or "(<basis>: <why>)".
std::string route_leg_end_line(
    const std::string& label, const RouteLogLeg& leg,
    const std::vector<RouteTile>& at, EndBasis basis, const std::string& why
);
//  "<label>: <leg>: <what>, at <tiles>; planning again.": the leg stopped
//  short, where the player was seen to be, and the route is planned again
//  from there.
std::string route_replan_line(
    const std::string& label, const RouteLogLeg& leg,
    const std::string& what, const std::vector<RouteTile>& at
);
//  "<label>: found again at <tiles>, after <holds> hold(s).": after a stop
//  where the player wasn't seen, holds to walls found them.
std::string route_found_line(const std::string& label, const std::vector<RouteTile>& at, size_t holds);
//  "<label>: <who> stopped us about <tiles>."
std::string route_trainer_line(const std::string& label, const std::string& who, const std::vector<RouteTile>& at);
//  "<label>: arrived at <tiles>."
std::string route_arrived_line(const std::string& label, const std::vector<RouteTile>& at);



}
}
#endif
