/*  BDSP AutoStory - Table Routes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story table's ROUTE steps, each from where the steps before it in
 *  its row leave the player, and the legs the program plans for them: what
 *  the route tests and the route bench check.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_TableRoutes_H
#define PokemonAutomation_PokemonBDSP_AutoStory_TableRoutes_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A ROUTE step of the table, and where the steps before it in its row leave
//  the player: the row's start, then each step's target or pose_after. A LEGS
//  or STEP step that does not say where it ends leaves the start unknown.
struct TableRoute{
    std::string row;
    const Pokemon::StoryStep* step;
    std::optional<Pokemon::Pose> from;
    //  The world the step is planned on; none if the start is unknown or no
    //  world joins its map and the target's.
    const StoryWorld* world;
};

//  Every ROUTE step of the story table, in table order.
std::vector<TableRoute> table_routes();

//  The world tiles of `pose` on `world`.
std::vector<Pokemon::TilePos> world_tiles(const StoryWorld& world, const Pokemon::Pose& pose);

//  The legs a table ROUTE step plans, as walk_route() plans them: the step's
//  costs, then the default ones; no legs if neither plans.
struct RouteStepPlan{
    std::vector<Pokemon::TilePos> start;
    std::optional<std::vector<PlannedLeg>> legs;
    bool default_costs = false;     //  the step's own costs found no plan
};

//  Plans `route`, which must have a world.
RouteStepPlan plan_table_route(const TableRoute& route);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
