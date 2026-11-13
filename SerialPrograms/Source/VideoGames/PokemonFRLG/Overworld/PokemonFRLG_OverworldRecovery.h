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
 *    - a leg that crosses a trainer's sight (trainer_crossing()) is watched
 *      after for long enough that the trainer, if they saw the player, has
 *      walked up and spoken;
 *    - any other open leg stopped part-way, or a leg seen stopped by a
 *      trainer, has the tiles the player may be on worked out from the
 *      stop's timing and the trainers' sight (stop_candidates()). One tile:
 *      the route is planned again from it. More: holds to walls that end on
 *      one tile from all of them find the player (plan_reanchor()), and a
 *      way back to the stopped leg's end rejoins the route. The rest of the
 *      route is then walked as planned. A leg onto a warp whose fade was
 *      a wild battle's counts as stopped before its warp
 *      (walked_before_warp()), and the way back goes on through the warp;
 *    - an open leg that ended with nothing seen may still have ended up to
 *      OPEN_LEG_SHORTFALL tiles short. When the next step is a hold to a
 *      wall that doesn't end on its tile from each of those, or a step onto
 *      a spinner (which a short start misses), the player is
 *      found at walls the same way and walked back to the leg's end first.
 *      When no holds find them, the leg is taken as walked, as before.
 *
 *  Where the player is is never guessed: when no holds find them from every
 *  tile they may be on, the route fails, and the caller stops without
 *  saving.
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
#include "Pokemon/AutoStory/Types/Pokemon_StoryBreakpoints.h"
#include "PokemonFRLG_OverworldGatedRoute.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldTrainerCrossing.h"
#include "PokemonFRLG_OverworldStepChecks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What the route was planned with, to plan the way back onto it: the maps
//  (with the event flags the caller set) and the costs; and, for the route
//  log (Pokemon_RouteLog.h), where it starts and what the caller calls it.
struct RoutePlanning{
    //  The maps as loaded: a gate tile is a wall until `gates` has it open.
    //  The run reads them through the gates, which each OPEN_GATE or
    //  PRESS_SWITCH step it walks changes.
    const std::vector<MapData>* maps = nullptr;
    Pokemon::GateState gates;
    RouteCosts costs = default_route_costs();
    std::optional<MapPosition> start;
    std::string label = "Route";
    WallCheck wall_check = WallCheck::TAP;
    OpenLegCheck open_leg_check = OpenLegCheck::REANCHOR;
    //  Where the run pauses before each leg (breakpoints, LEGS mode). None:
    //  never.
    Pokemon::StoryPausePoints* pause = nullptr;
};

//  The console side. `walk` walks one step from `facing` (unknown after a
//  stop); `hold_to_warp` holds on along a stopped leg until its warp;
//  `fail` throws.
struct RouteCalls{
    std::function<StepOutcome(const RouteStep& step, std::optional<Direction> facing)> walk;
    std::function<void(const RouteStep& step)> hold_to_warp;
    std::function<void(const std::string& message)> log;
    std::function<void(const std::string& message)> fail;
    //  Taps once more toward the wall a leg to a wall ended at, and says
    //  whether the screen moved. Only logged: a bump and a step can't be
    //  told apart reliably yet. Not set (or WallCheck::SKIP): not tapped.
    std::function<bool(const RouteStep& step)> moved_past_wall;
    //  After a leg that ended as planned but crosses a trainer's sight:
    //  watches long enough for a trainer who saw the player to walk up
    //  `approach_tiles` tiles and speak, has what shows cleared (the
    //  caller's interrupts fight the trainer), and returns LATE if anything
    //  showed, DONE if not. Not set: not watched.
    std::function<StepEnd(const RouteStep& step, int approach_tiles)> watch_for_trainer;
};


//  How often a route may stop part-way before it gives up.
constexpr size_t MAX_ROUTE_STOPS = 8;

//  Walks `route` from `facing`, logging it in the shared route log lines:
//  the plan, each leg as it starts and how it ended, and where the player
//  was found again. Without `planning`, a stop that needs the player found
//  again fails instead, and no leg is watched for trainers.
void run_route(
    const std::vector<RouteStep>& route, std::optional<Direction> facing,
    const RoutePlanning* planning, const RouteCalls& calls
);


//  Registers overworld recovery unit tests.
void add_tests_OverworldRecovery(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
