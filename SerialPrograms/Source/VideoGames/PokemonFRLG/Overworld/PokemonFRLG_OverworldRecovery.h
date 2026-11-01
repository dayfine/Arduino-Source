/*  Overworld Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Runs a planned route step by step, and when a step ends short of where
 *  it was planned to, gets the player back onto it:
 *
 *    - a leg to a wall interrupted after its hold is walked again;
 *    - an open leg to a warp, off the grass, stopped by a message (a
 *      trainer), is held on toward the warp;
 *    - any other open leg stopped part-way is found again with holds to
 *      walls (plan_reanchor()), and rejoins the route at that leg's end by
 *      a route planned on the caller's maps and costs. The rest of the
 *      route is then walked as planned.
 *
 *  The console side is passed in, so this is tested with scripted steps.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldRecovery_H
#define PokemonAutomation_PokemonFRLG_OverworldRecovery_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  How one step of a route ended.
enum class StepEnd{
    DONE,
    STOPPED,    //  stopped part-way: where the player is isn't known
    LATE,       //  something was seen after the hold, which may have
                //  stopped the player before the end
};

//  What the route was planned with, to plan the way back onto it: the maps
//  (with the event flags the caller set) and the costs; and, for the route
//  log (Pokemon_RouteLog.h), where it starts and what the caller calls it.
struct RoutePlanning{
    const std::vector<MapData>* maps = nullptr;
    RouteCosts costs = default_route_costs();
    std::optional<MapPosition> start;
    std::string label = "Route";
};

//  The console side. `walk` walks one step from `facing` (unknown after a
//  stop); `hold_to_warp` holds on along a stopped leg until its warp;
//  `fail` throws.
struct RouteCalls{
    std::function<StepEnd(const RouteStep& step, std::optional<Direction> facing)> walk;
    std::function<void(const RouteStep& step)> hold_to_warp;
    std::function<void(const std::string& message)> log;
    std::function<void(const std::string& message)> fail;
};

//  How often a route may stop part-way before it gives up.
constexpr size_t MAX_ROUTE_STOPS = 8;

//  Walks `route` from `facing`, logging it in the shared route log lines:
//  the plan, each leg as it starts and how it ended, and where the player
//  was found again. Without `planning`, a stop that needs the player found
//  again fails instead.
void run_route(
    const std::vector<RouteStep>& route, std::optional<Direction> facing,
    const RoutePlanning* planning, const RouteCalls& calls
);


void add_tests_OverworldRecovery(UnitTestDatabase& database);



}
}
}
#endif
